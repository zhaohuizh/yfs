// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"


static void *
retry_thread(void * t){
  lock_client_cache * lcc = (lock_client_cache *) t;
  lcc->retry_loop();
  return 0;
}

static void *
release_thread(void * t){
  lock_client_cache * lcc = (lock_client_cache *) t;
  lcc->release_loop();
  return 0;
}

lock_client_cache::lock_client_cache(std::string xdst, 
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  rpcs *rlsrpc = new rpcs(0);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);

  const char *hname;
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlsrpc->port();
  id = host.str();
  pthread_mutex_init(&retry_mutex, NULL);
  pthread_mutex_init(&revoke_mutex, NULL);
  pthread_cond_init(&retry_threadhold, NULL);
  pthread_cond_init(&revoke_threadhold, NULL);

  pthread_t retry_t, revoke_t;
  int rc;
  
  rc = pthread_create(&retry_t, NULL, &retry_thread, (void *)this);
  if(rc){
    tprintf("Error in creating retry thread\n");
  }

  rc = pthread_create(&revoke_t, NULL, &release_thread, (void *)this);
  if(rc){
    tprintf("Error in creating revoke thread\n");
  }
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  int ret = lock_protocol::OK;

  
  std::map<lock_protocol::lockid_t, lock_client_cache::client_state>::iterator it = client_state_map.find(lid);
  if(it == client_state_map.end()){
    pthread_cond_init(&lock_threadhold_map[lid], NULL);
    // pthread_cond_init(&acquire_threadhold_map[lid], NULL);
    pthread_cond_init(&revoke_threadhold_map[lid], NULL);
    // pthread_cond_init(&retry_threadhold_map[lid], NULL);
    pthread_mutex_init(&mutex_map[lid], NULL);
    pthread_mutex_lock(&mutex_map[lid]);
    client_state_map[lid] = NONE;
    pthread_mutex_unlock(&mutex_map[lid]);
  }

  pthread_mutex_lock(&mutex_map[lid]);
  tprintf("Acquire: begin, tid: %lu lid: %llu\n", pthread_self(), lid);
  int r;
  while(true){
    if(client_state_map[lid] == NONE){
      tprintf("Acquire: status is NONE, tid: %lu lid: %llu\n", pthread_self(), lid);
      client_state_map[lid] = ACQUIRING;
      ret = cl->call(lock_protocol::acquire, lid, id, r);
      tprintf("Acquire: call server, tid: %lu, id: %s\n", pthread_self(), id.c_str());
      if(ret == lock_protocol::OK){
        tprintf("Acquire: server return OK\n");
        client_state_map[lid] = FREE;
        goto lock;
      }else if(ret == lock_protocol::RETRY){
        tprintf("Acquire: server return RETRY\n");
        client_state_map[lid] = ACQUIRING;
        pthread_cond_wait(&lock_threadhold_map[lid], &mutex_map[lid]);
        tprintf("Acquire: awake from retry\n");
        if(client_state_map[lid] == FREE){
          goto lock;
        }else{
          tprintf("Acquire: state not Free after wake up from reply RPC!\n");
          continue;
        }
      }

    }else if(client_state_map[lid] == FREE){
      goto lock;
    }else{
      tprintf("Acquire: wait for release to release lock lid: %llu\n", lid);
      pthread_cond_wait(&lock_threadhold_map[lid], &mutex_map[lid]);
      tprintf("Acquire: awake from release()\n");
      if(client_state_map[lid] == FREE){
        goto lock;
      }else{
        continue;
      }
    }

  }

lock:
  client_state_map[lid] = LOCKED;
  tprintf("Acquire: grant lock, tid: %lu, id: %s\n", pthread_self(), id.c_str());
  pthread_mutex_unlock(&mutex_map[lid]);
  return lock_protocol::OK;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  lock_protocol::status ret = lock_protocol::OK;
  tprintf("Release: begin, tid: %lu lid: %llu\n", pthread_self(), lid);
  std::map<lock_protocol::lockid_t, lock_client_cache::client_state>::iterator it = client_state_map.find(lid);
  if(it == client_state_map.end()){
    tprintf("Release: NOENT\n");
    ret = lock_protocol::NOENT;
  }else{
    pthread_mutex_lock(&mutex_map[lid]);
    if(client_state_map[lid] == LOCKED){
      tprintf("Release: status is locked\n");
      client_state_map[lid] = FREE;
      tprintf("Release: signal acquire to obtain lock lid: %llu \n", lid);
      pthread_cond_signal(&lock_threadhold_map[lid]);
    }else{
      assert(client_state_map[lid] == RELEASING);
      tprintf("Release: waiting to release to server\n");
      tprintf("Release: signal revoke_loop to give lock to server lid: %llu \n", lid);
      pthread_cond_signal(&revoke_threadhold_map[lid]); 
    }
    pthread_mutex_unlock(&mutex_map[lid]);
  }
  return ret;

}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int &)
{
  int ret = rlock_protocol::OK;
  pthread_mutex_lock(&revoke_mutex);
  release_lists.push_back(lid);
  pthread_cond_signal(&revoke_threadhold);
  pthread_mutex_unlock(&revoke_mutex);
  return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int &)
{
  int ret = rlock_protocol::OK;
  // logics need to be modified
  pthread_mutex_lock(&retry_mutex);
  retry_lists.push_back(lid);
  pthread_cond_signal(&retry_threadhold);
  pthread_mutex_unlock(&retry_mutex);
  return ret;
}

void
lock_client_cache::retry_loop(){
  int r;
  while(true){
    pthread_mutex_lock(&retry_mutex);
    pthread_cond_wait(&retry_threadhold, &retry_mutex);
    tprintf("Retry loop awake!\n");
    while(retry_lists.size() > 0){
      
      lock_protocol::lockid_t lid = retry_lists.front();
      retry_lists.pop_front();
      pthread_mutex_lock(&mutex_map[lid]);
      lock_protocol::status ret = cl->call(lock_protocol::acquire, lid, id, r);
      tprintf("Retry loop: call acquire, tid: %lu, id: %s\n", pthread_self(), id.c_str());
      assert(ret == lock_protocol::OK);
      
      client_state_map[lid] = FREE;
      tprintf("Retry loop: acquire succeed, set state to FREE lid: %llu id: %s\n", lid, id.c_str());
      tprintf("Retry loop: signal acquire thread retry succeed lid: %llu \n", lid);
      pthread_cond_signal(&lock_threadhold_map[lid]);
      pthread_mutex_unlock(&mutex_map[lid]);
    }
    pthread_mutex_unlock(&retry_mutex);
  }
}


void
lock_client_cache::release_loop(){
  int r;
  while(true){
    pthread_mutex_lock(&revoke_mutex);
    pthread_cond_wait(&revoke_threadhold, &revoke_mutex);
    tprintf("Release loop: awake!\n");
    while(release_lists.size() > 0){
      lock_protocol::lockid_t lid = release_lists.front();
      release_lists.pop_front();
      pthread_mutex_lock(&mutex_map[lid]);
      tprintf("Release loop: status id: %s:  %llu -> %d\n", id.c_str(), lid, client_state_map[lid]);
      if(client_state_map[lid] == LOCKED){
        client_state_map[lid] = RELEASING;
        tprintf("Release loop: wait for release to give lock %llu \n ", lid);
        pthread_cond_wait(&revoke_threadhold_map[lid], &mutex_map[lid]);
        assert(client_state_map[lid] == RELEASING);
      }else{
        assert(client_state_map[lid] == FREE);
      }

      lock_protocol::status ret = cl->call(lock_protocol::release, lid, id, r);
      tprintf("Release loop: call release, tid: %lu, id: %s\n", pthread_self(), id.c_str());
      assert(ret == lock_protocol::OK);
      client_state_map[lid] = NONE;
      pthread_cond_signal(&lock_threadhold_map[lid]);
      pthread_mutex_unlock(&mutex_map[lid]);
    }

    pthread_mutex_unlock(&revoke_mutex);
  }
}

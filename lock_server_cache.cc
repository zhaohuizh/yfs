// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"


static void *
retry_thread(void * t){
  lock_server_cache * lsc = (lock_server_cache *) t;
  tprintf("retry thread: begin retry_loop\n");
  lsc->retry_loop();
  return 0;
}

static void *
revoke_thread(void * t){
  lock_server_cache * lsc = (lock_server_cache *) t;
  tprintf("revoke thread: begin revoke_loop\n");
  lsc->revoke_loop();
  return 0;
}

lock_server_cache::lock_server_cache()
{
  pthread_mutex_init(&mutex, NULL);
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

  rc = pthread_create(&revoke_t, NULL, &revoke_thread, (void *)this);
  if(rc){
    tprintf("Error in creating revoke thread\n");
  }
}

lock_server_cache::~lock_server_cache(){
  pthread_mutex_destroy(&mutex);
  pthread_mutex_destroy(&retry_mutex);
  pthread_mutex_destroy(&revoke_mutex);
  pthread_cond_destroy(&retry_threadhold);
  pthread_cond_destroy(&revoke_threadhold);

}


int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
                               int &)
{
  lock_protocol::status ret = lock_protocol::OK;
  std::map<lock_protocol::lockid_t, lock_server_cache::server_cache_info>::iterator it = server_info_map.find(lid);
  pthread_mutex_lock(&mutex);
  tprintf("Server acquire: new thread: %lu id:%s \n", pthread_self(), id.c_str());
  if(it == server_info_map.end()){
    tprintf("Server acquire: new lid come and update info\n");
    server_cache_info info;
    info.status = FREE;
    server_info_map[lid] = info;
  }

  if(server_info_map[lid].status == FREE){
    tprintf("Server acquire: %llu -> FREE\n", lid);
    server_info_map[lid].status = LOCKED;
    server_info_map[lid].owner = id;
    if(server_info_map[lid].waiting_lists.size() > 0){
      tprintf("Server acquire: schedule a revoke (1) \n");
      server_info_map[lid].status = REVOKING;
      pthread_mutex_lock(&revoke_mutex);
      client_info c_info;
      c_info.lid = lid;
      c_info.id = server_info_map[lid].owner;
      revoke_lists.push_back(c_info);
      tprintf("Server acquire: signal a revoke (1) with id: %s \n", c_info.id.c_str());
      pthread_cond_signal(&revoke_threadhold);
      pthread_mutex_lock(&revoke_mutex);
    }
  }else if(server_info_map[lid].status == RETRYING && server_info_map[lid].retrying_id == id){
    server_info_map[lid].waiting_lists.pop_front();
    tprintf("Server acquire: -> RETRYING lid: %llu id: %s \n", lid , id.c_str());
    server_info_map[lid].status = LOCKED;
    server_info_map[lid].owner = id;
    if(server_info_map[lid].waiting_lists.size() > 0){
      tprintf("Server acquire: schedule a revoke (2) \n");
      server_info_map[lid].status = REVOKING;
      pthread_mutex_lock(&revoke_mutex);
      client_info c_info;
      c_info.lid = lid;
      c_info.id = server_info_map[lid].owner;
      revoke_lists.push_back(c_info);
      tprintf("Server acquire: signal a revoke (2) with id: %s \n", c_info.id.c_str());
      pthread_cond_signal(&revoke_threadhold);
      pthread_mutex_unlock(&revoke_mutex);
    }

  }else if(server_info_map[lid].status == LOCKED){
    tprintf("Server acquire: %llu -> LOCKED\n", lid);
    server_info_map[lid].status = REVOKING;
    server_info_map[lid].waiting_lists.push_back(id);

    tprintf("Server acquire: schedule a revoke (3) \n");
    pthread_mutex_lock(&revoke_mutex);
    client_info c_info;
    c_info.lid = lid;
    c_info.id = server_info_map[lid].owner;
    revoke_lists.push_back(c_info);
    tprintf("Server acquire: signal a revoke (3) with id: %s \n", c_info.id.c_str());
    pthread_cond_signal(&revoke_threadhold);
    pthread_mutex_unlock(&revoke_mutex);
    ret = lock_protocol::RETRY;
  }else{
    server_info_map[lid].waiting_lists.push_back(id);
    ret = lock_protocol::RETRY;
  }
  pthread_mutex_unlock(&mutex);
  
  return ret;
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
         int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  pthread_mutex_lock(&mutex);
  tprintf("Release: set %llu to FREE\n", lid);
  server_info_map[lid].status = FREE;
  if(server_info_map[lid].waiting_lists.size() > 0){
    tprintf("Release: send retry\n");
    server_info_map[lid].status = RETRYING;
    client_info c_info;
    c_info.lid = lid;
    c_info.id = server_info_map[lid].waiting_lists.front();
    server_info_map[lid].retrying_id = c_info.id;
    pthread_mutex_lock(&retry_mutex);
    retry_lists.push_back(c_info);
    pthread_cond_signal(&retry_threadhold);
    pthread_mutex_unlock(&retry_mutex);
  }
  pthread_mutex_unlock(&mutex);

  return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}



void
lock_server_cache::retry_loop(){
  
  rlock_protocol::status retry_ret;
  tprintf("retry_loop thread: retry_loop running\n");
  while(true){
    pthread_mutex_lock(&retry_mutex);
    tprintf("retry_loop thread: retry awake!\n");
    pthread_cond_wait(&retry_threadhold, &retry_mutex);
    while(retry_lists.size() > 0){
      client_info c_info = retry_lists.front();
      retry_lists.pop_front();
      std::string id = c_info.id;
      handle h(id);
      if(h.safebind()){
        int r;
        tprintf("retry thread: call rpc to retry. lid %llu\n", c_info.lid);
        retry_ret = h.safebind()->call(rlock_protocol::retry, c_info.lid, r);
      }
      if (!h.safebind() || retry_ret != rlock_protocol::OK){
        tprintf("retry RPC failed\n");
      }
    }
    pthread_mutex_unlock(&retry_mutex);
  }
}

void
lock_server_cache::revoke_loop(){
  rlock_protocol::status revoke_ret;
  tprintf("revoke_loop thread: revoke_loop running\n");
  while(true){
    pthread_mutex_lock(&revoke_mutex);
    pthread_cond_wait(&revoke_threadhold, &revoke_mutex);
    tprintf("revoke_loop thread: revoke awake!\n");
    while(revoke_lists.size() > 0){
      tprintf("revoke_loop thread: revoke_loop list size %lu !\n", revoke_lists.size());
      client_info c_info;
      c_info = revoke_lists.front();
      revoke_lists.pop_front();
      std::string id = c_info.id;
      handle h(id);
      int r;
      if(h.safebind()){
        tprintf("revoke_loop thread: call rpc to release. lid %llu\n", c_info.lid);
        revoke_ret = h.safebind()->call(rlock_protocol::revoke, c_info.lid, r);
      }
      if (!h.safebind() || revoke_ret != rlock_protocol::OK){
        tprintf("revoke RPC failed\n");
      }        
    }
    tprintf("revoke_loop thread: revoke_loop over!\n");
    pthread_mutex_unlock(&revoke_mutex);

  }
}
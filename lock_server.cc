// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

pthread_mutex_t mutex;

lock_server::lock_server():
  nacquire (0)
{
  pthread_mutex_init(&mutex, NULL);
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r){
  lock_protocol::status ret;
  pthread_mutex_lock(&mutex);
  map<lock_protocol::lockid_t, lock_status>::iterator it = lock_map.find(lid);
  if(it == lock_map.end()){
    cout << lid << " lid not found, then create and lock \n";
    pthread_cond_init(&threadhold_map[lid], NULL);
  }else{
    while(it->second != lock_server::FREE){
      cout << "cond wait: " << lid << '\n';
      pthread_cond_wait(&threadhold_map[lid], &mutex);
      cout << "cond received: " << lid << '\n';
    }
    cout << "loop out" << '\n';
  }
  lock_map[lid] = lock_server::LOCKED;
  cout << "lock gained" << '\n';
  pthread_mutex_unlock(&mutex);
  
  ret = lock_protocol::OK;
  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r){
  lock_protocol::status ret;

  pthread_mutex_lock(&mutex);
  lock_map[lid] = lock_server::FREE;
  pthread_cond_signal(&threadhold_map[lid]);
  cout << lid << " freed!\n";
  pthread_mutex_unlock(&mutex);

  ret = lock_protocol::OK;
  return ret;
}

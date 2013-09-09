// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

using namespace std;

pthread_mutex_t mutex;
pthread_cond_t threadhold;

lock_server::lock_server():
  nacquire (0)
{
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
  if(lock_map.find(lid) != lock_map.end()){
    while(lock_map[lid] == lock_server::LOCKED){
      pthread_cond_wait(&threadhold, &mutex);
      printf("Signal received!\n");
    } 
  }
  

  lock_map[lid] = lock_server::LOCKED;
  pthread_mutex_unlock(&mutex);
  ret = lock_protocol::OK;
  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r){
  lock_protocol::status ret;
  pthread_mutex_lock(&mutex);
  if(lock_map.find(lid) == lock_map.end()){
    ret = lock_protocol::NOENT;
  }else{
    
    lock_map[lid] = lock_server::FREE;
    pthread_cond_broadcast(&threadhold);
    printf("Thread Hold reached!\n");
    ret = lock_protocol::OK;
  }
  pthread_mutex_unlock(&mutex);
  return ret;
}

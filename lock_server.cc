// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

using namespace std;

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

  /*
  if(lock_map.find(lid) != lock_map.end()){
    while(lock_map[lid] == lock_server::LOCKED){
      pthread_cond_wait(&threadhold_map[lid], &mutex);
      printf("Signal received! \n");
    } 
  }else{
    &threadhold_map[lid] = new pthread_cond_t;
    pthread_cond_init(&threadhold_map[lid], NULL);
  }

  lock_map[lid] = lock_server::LOCKED;
  pthread_mutex_unlock(&mutex);
  */

  std::map<lock_protocol::lockid_t, lock_status>::iterator it = lock_map.find(lid);
  if(it == lock_map.end()){
    std::cout << lid << " lid not found, then create and lock \n";
    lock_map[lid] = lock_server::LOCKED;
    &threadhold_map[lid] = new pthread_cond_t;
    pthread_cond_init(&threadhold_map[lid], NULL);
  }else if(it->second == lock_server::FREE){
    lock_map[lid] = lock_server::LOCKED;
  }else{
    while(it->second != lock_server::FREE){
      std::cout << "cond wait: " << lid << '\n';
      pthread_cond_wait(&threadhold_map[lid], &mutex);
      std::cout << "cond received: " << lid << '\n';
    }
    lock_map[lid] = lock_server::LOCKED;
    std::cout << "loop out and lock gained" << '\n';
  }
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

// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

using namespace std;

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
  if(lock_map.find(lid) == lock_map.end() || lock_map[lid] == false){
    lock_map[lid] = true;
    ret = lock_protocol::OK;
  }else{
    ret = lock_protocol::RETRY;
  }
  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r){
  lock_protocol::status ret;
  if(lock_map.find(lid) == lock_map.end()){
    ret = lock_protocol::NOENT;
  }else{
    lock_map[lid] = false;
    ret = lock_protocol::OK;
  }
  return ret;
}

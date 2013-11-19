// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include <map>
#include <pthread.h>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"

using namespace std;

class lock_server {

 protected:
  int nacquire;
  enum lock_status{
    FREE,
    LOCKED
  };
  
  map<lock_protocol::lockid_t, lock_status> lock_map;
  map<lock_protocol::lockid_t, pthread_cond_t> threadhold_map;

 public:
  lock_server();
  ~lock_server() {};
  lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);
};

#endif 








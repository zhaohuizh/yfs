#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>
#include <list>
#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"


class lock_server_cache {
 private:
  int nacquire;
  rpcc *cl;
  enum lock_status{
    FREE,
    LOCKED,
    REVOKING,
    RETRYING
  };

  struct server_cache_info{
    lock_status status;
    std::string owner;
    std::string retrying_id;
    std::list<std::string> waiting_lists;
  };

  struct client_info{
    std::string id;
    lock_protocol::lockid_t lid;
  };
  
  pthread_mutex_t mutex;
  pthread_mutex_t retry_mutex;
  pthread_cond_t retry_threadhold;
  pthread_mutex_t revoke_mutex;
  pthread_cond_t revoke_threadhold;
  std::map<lock_protocol::lockid_t, server_cache_info> server_info_map;
  std::list<client_info> revoke_lists;
  std::list<client_info> retry_lists;

 public:
  lock_server_cache();
  ~lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
  void retry_loop();
  void revoke_loop();
};

#endif

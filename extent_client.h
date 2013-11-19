// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include <set>
#include <pthread.h>
#include "extent_protocol.h"
#include "rpc.h"

class extent_client {
 private:
  rpcc *cl;

 public:
  extent_client(std::string dst);

  struct extent{
  	std::string content;
  	extent_protocol::attr attribute;
  };

  std::map<extent_protocol::extentid_t, extent> cache_extent_map;
  std::set<extent_protocol::extentid_t> dirty_set;
  pthread_mutex_t cache_mutex;

  extent_protocol::status get(extent_protocol::extentid_t eid, 
			      std::string &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid, 
				  extent_protocol::attr &a);
  extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status remove(extent_protocol::extentid_t eid);

  void flush(extent_protocol::extentid_t eid);
};

#endif 


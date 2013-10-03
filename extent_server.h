// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include <pthread.h>
#include "extent_protocol.h"

class extent_server {

 public:
  extent_server();

  struct extent{
  	std::string content;
  	extent_protocol::attr attribute;
  };

  std::map<extent_protocol::extentid_t, extent> extent_map;
  pthread_mutex_t mutex;

  int put(extent_protocol::extentid_t id, std::string, int &);
  int get(extent_protocol::extentid_t id, std::string &);
  int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
  int remove(extent_protocol::extentid_t id, int &);
};

#endif 








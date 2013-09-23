// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extent_server::extent_server() {
  pthread_mutex_init(&mutex, NULL);
}


int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
  // You fill this in for Lab 2.
  pthread_mutex_lock(&mutex);
  extent_map[id] = buf;
  pthread_mutex_unlock(&mutex);
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  // You fill this in for Lab 2.
  extent_protocol::status ret;
  map<extent_protocol::extentid_t, std::string>::iterator it = extent_map.find(id);
  if(it != extent_map.end()){
    buf = extent_map[id];
    ret = extent_protocol::OK;
  }else{
    ret = extent_protocol::NOENT;
  }
  return ret;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  // You fill this in for Lab 2.
  // You replace this with a real implementation. We send a phony response
  // for now because it's difficult to get FUSE to do anything (including
  // unmount) if getattr fails.
  a.size = 0;
  a.atime = 0;
  a.mtime = 0;
  a.ctime = 0;
  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  // You fill this in for Lab 2.
  extent_protocol::status ret;
  map<extent_protocol::extentid_t, std::string>::iterator it = extent_map.find(id);
  if(it != extent_map.end()){
    pthread_mutex_lock(&mutex);
    extent_map.erase(id);
    pthread_mutex_unlock(&mutex);
    ret = extent_protocol::OK;
  }else{
    ret = extent_protocol::NOENT;
  }
  return ret;
}


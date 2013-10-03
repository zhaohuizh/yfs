// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

extent_server::extent_server() {
  printf("extent_server initial \n");
  pthread_mutex_init(&mutex, NULL);
  printf("mutex created! \n");
}


int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
  // You fill this in for Lab 2.
  printf("PUT extent_server begin\n");
  pthread_mutex_lock(&mutex);
  printf("0 \n");
  std::cout << "BUF=" << buf << std::endl;
  extent_server::extent file_extent;
  printf("0.5 \n");
  file_extent.content = buf;
  printf("1 \n");
  time_t timer = time(NULL);
  printf("2 \n");
  extent_protocol::attr attribute;
  attribute.ctime = timer;
  attribute.mtime = timer;
  attribute.size = buf.size();
  printf("3 \n");
  file_extent.attribute = &attribute;

  printf("4 \n");
  extent_map[id] = file_extent;
  // extent_map[id]->content = buf;
  // time_t timer = time(NULL);
  // extent_map[id]->attribute->ctime = timer;
  // extent_map[id]->attribute->mtime = timer;
  // extent_map[id]->attribute->size = buf.size();
  printf("5 \n");
  pthread_mutex_unlock(&mutex);
  printf("Put succeed!\n");
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  // You fill this in for Lab 2.
  extent_protocol::status ret;
  std::map<extent_protocol::extentid_t, extent>::iterator it = extent_map.find(id);
  if(it != extent_map.end()){
    buf = extent_map[id].content;
    pthread_mutex_lock(&mutex);
    extent_map[id].attribute->atime = time(NULL);
    pthread_mutex_unlock(&mutex);
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
  extent_protocol::status ret;
  std::map<extent_protocol::extentid_t, extent>::iterator it = extent_map.find(id);
  if(it != extent_map.end()){
    a.size = extent_map[id].attribute->size;
    a.atime = extent_map[id].attribute->atime;
    a.mtime = extent_map[id].attribute->mtime;
    a.ctime = extent_map[id].attribute->ctime;
    ret = extent_protocol::OK;
  }else{
    printf("GETATTR: no inum \n");
    ret = extent_protocol::NOENT;
  }
  return ret;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  // You fill this in for Lab 2.
  extent_protocol::status ret;
  std::map<extent_protocol::extentid_t, extent>::iterator it = extent_map.find(id);
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


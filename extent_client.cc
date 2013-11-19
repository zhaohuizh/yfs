// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "tprintf.h"

// The calls assume that the caller holds a lock on the extent

extent_client::extent_client(std::string dst)
{
  sockaddr_in dstsock;
  pthread_mutex_init(&cache_mutex, NULL);
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
  }
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret;
  // ret = cl->call(extent_protocol::get, eid, buf);
  pthread_mutex_lock(&cache_mutex);
  if(cache_extent_map.find(eid) == cache_extent_map.end()){
    if(dirty_set.find(eid) == dirty_set.end()){
      std::string original_buf;
      extent_protocol::attr original_attr;
      tprintf("get content of %llu\n", eid);
      extent_protocol::status get_ret = cl->call(extent_protocol::get, eid, original_buf);
      extent_protocol::status getattr_ret = cl->call(extent_protocol::getattr, eid, original_attr);

      if(get_ret == extent_protocol::OK && getattr_ret == extent_protocol::OK){
      
        cache_extent_map[eid].content = original_buf;
        cache_extent_map[eid].attribute = original_attr;
        time_t timer = time(NULL);
        cache_extent_map[eid].attribute.atime = timer;
      }else{
        printf("extent_client: call server get() or getattr() fails \n");
        // what if getattr_ret fails???
        if(get_ret != extent_protocol::OK){
          ret = get_ret;
        }else{
          ret = getattr_ret;
        }
        
        goto release;
      }
    }else{
      ret = extent_protocol::NOENT;
      goto release;
    }
  }

  buf = cache_extent_map[eid].content;
  tprintf("extent_client: get succeed, return content : %s \n", buf.c_str());
  ret = extent_protocol::OK;

  release:
    pthread_mutex_unlock(&cache_mutex);
    return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret;
  // ret = cl->call(extent_protocol::getattr, eid, attr);
  pthread_mutex_lock(&cache_mutex);
  if(cache_extent_map.find(eid) == cache_extent_map.end()){
    if(dirty_set.find(eid) == dirty_set.end()){
      std::string original_buf;
      extent_protocol::attr original_attr;
      extent_protocol::status get_ret = cl->call(extent_protocol::get, eid, original_buf);
      extent_protocol::status getattr_ret = cl->call(extent_protocol::getattr, eid, original_attr);

      if(get_ret == extent_protocol::OK && getattr_ret == extent_protocol::OK){
      
        cache_extent_map[eid].content = original_buf;
        cache_extent_map[eid].attribute = original_attr;
      
      }else{
        printf("extent_client: call server get() or getattr() fails \n");
        
        if(getattr_ret != extent_protocol::OK){
          ret = getattr_ret;
        }else{
          ret = get_ret;
        }
        
        goto release;
      }
    }else{
      ret = extent_protocol::NOENT;
      goto release;
    }
  }

  attr.size = cache_extent_map[eid].attribute.size;
  attr.atime = cache_extent_map[eid].attribute.atime;
  attr.mtime = cache_extent_map[eid].attribute.mtime;
  attr.ctime = cache_extent_map[eid].attribute.ctime;

  ret  = extent_protocol::OK;

  release:
    pthread_mutex_unlock(&cache_mutex);
    return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  // int r;
  // ret = cl->call(extent_protocol::put, eid, buf, r);
  pthread_mutex_lock(&cache_mutex);
  cache_extent_map[eid].content = buf;
  time_t timer = time(NULL);
  extent_protocol::attr attribute;
  attribute.ctime = timer;
  attribute.mtime = timer;
  attribute.size = buf.size();
  cache_extent_map[eid].attribute = attribute;
  dirty_set.insert(eid);
  pthread_mutex_unlock(&cache_mutex);
  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  // int r;
  // ret = cl->call(extent_protocol::remove, eid, r);
  pthread_mutex_lock(&cache_mutex);
  cache_extent_map.erase(eid);
  dirty_set.insert(eid);
  pthread_mutex_unlock(&cache_mutex);
  return ret;
}

void 
extent_client::flush(extent_protocol::extentid_t eid){
  int r;
  pthread_mutex_lock(&cache_mutex);
  tprintf("Extent_client: flush begin!\n");
  // if(!dirty_set.empty()){
  //   std::set<extent_protocol::extentid_t>::iterator it = dirty_set.begin();
  //   while(it != dirty_set.end()){
  //     tprintf("Extent_client: dirty_set contains %llu !\n", (*it));
  //     ++it;
  //   }
  // }else{
  //   tprintf("Extent_client: dirty set is empty %llu\n", eid);
  // }
  
  if(dirty_set.find(eid) != dirty_set.end()){
    tprintf("Extent_client: data is dirty! %llu\n", eid);
    if(cache_extent_map.find(eid) == cache_extent_map.end()){
      extent_protocol::status ret = cl->call(extent_protocol::remove, eid, r);
      if(ret == extent_protocol::OK){
        tprintf("Extent_client: remove succeed!\n");
      }else{
        tprintf("Extent_client: remove error!\n");
      }
    }else{
      extent_protocol::status ret = cl->call(extent_protocol::put, eid, cache_extent_map[eid].content, r);
      tprintf("Extent_client: put content %s %llu\n", cache_extent_map[eid].content.c_str(), eid);
      if(ret == extent_protocol::OK){
        tprintf("Extent_client: put succeed!\n");
      }else{
        tprintf("Extent_client: put error!\n");
      }
      // remove cache
      tprintf("remove cache %llu\n", eid);
    }
    tprintf("remove dirty set %llu\n", eid);
    dirty_set.erase(eid);
  }
  cache_extent_map.erase(eid);
  //tprintf("Extent_client: after flush! content: %s\n", cache_extent_map[eid]);
  pthread_mutex_unlock(&cache_mutex);
}

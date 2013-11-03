// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include "lock_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdlib>



yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
  lc = new lock_client_cache(lock_dst);
  // create root
  inum root_inum = 0x00000001;
  std::string str("");
  if(ec->put(root_inum, str) == extent_protocol::OK){
    printf("create root succeed!\n");
  }else{
    printf("create root fail!\n");
  }
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
  std::istringstream ist(n);
  unsigned long long finum;
  ist >> finum;
  return finum;
}

std::string
yfs_client::filename(inum inum)
{
  std::ostringstream ost;
  ost << inum;
  return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
  if(inum & 0x80000000)
    return true;
  return false;
}

bool
yfs_client::isdir(inum inum)
{
  return ! isfile(inum);
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
  int r = OK;
  // You modify this function for Lab 3
  // - hold and release the file lock

  // lc->acquire(inum);
  printf("getfile %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }

  fin.atime = a.atime;
  fin.mtime = a.mtime;
  fin.ctime = a.ctime;
  fin.size = a.size;
  printf("getfile %016llx -> sz %llu\n", inum, fin.size);

  release:
  // lc->release(inum);
  return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
  int r = OK;
  // You modify this function for Lab 3
  // - hold and release the directory lock

  // lc->acquire(inum);
  printf("getdir %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }
  din.atime = a.atime;
  din.mtime = a.mtime;
  din.ctime = a.ctime;

  release:
  // lc->release(inum);
  return r;
}


yfs_client::inum
yfs_client::generate_inum(bool isFile){
  inum file_inum;
  file_inum = std::rand();
  if(isFile){
    file_inum |= 0x80000000;
  }else{
    file_inum &= 0x7FFFFFFF;
  }
  return file_inum;
}

int
yfs_client::create(inum par_inum, const char * name, inum & file_inum, bool isFile){
  status ret;
  
  // call extent_client::put to save file
  lc->acquire(par_inum);
  file_inum = yfs_client::generate_inum(isFile);
  std::string inial_content;

  std::string former_content;
  struct yfs_client::dirent file_dirent;

  file_dirent.name = name;
  file_dirent.inum = file_inum;

  // put to save file
  if(ec->put(file_inum, inial_content) != extent_protocol::OK){
    ret = IOERR;
    goto release;
  }
  

  // add entry to parent directory
  
  if(ec->get(par_inum, former_content) != extent_protocol::OK){
    ret = IOERR;
    goto release;
  }

  // update directory content and attributes
  former_content.append(yfs_client::serialize_dirent(file_dirent));

  if(ec->put(par_inum, former_content) != extent_protocol::OK){
    ret = IOERR;
    goto release;
  }

  ret = yfs_client::OK;

  release:
  lc->release(par_inum);
  return ret;
}

int
yfs_client::is_exist(inum par_inum, const char * name, bool & isexist){
  
  yfs_client::status ret;
  std::string dir_content;
  isexist = false;
  std::list<yfs_client::dirent> list;
  std::list<yfs_client::dirent>::iterator it;

  if(ec->get(par_inum, dir_content)!= extent_protocol::OK){
    ret = yfs_client::IOERR;
    goto release;
  }

  list = yfs_client::unserialize(dir_content);
  it = list.begin();
  
  while(it != list.end()){
    if(strcmp(name, (*it).name.c_str()) == 0){
      isexist = true;
      break;
    }
    ++it;
  }

  ret = yfs_client::OK;

  release:
  return ret;
}

int
yfs_client::get_inum(inum par_inum, const char * name, inum & file_inum){

  yfs_client::status ret = yfs_client::OK;
  
  std::string dir_content;
  if(ec->get(par_inum, dir_content)!= extent_protocol::OK){
    ret = IOERR;
    return ret;
  }

  std::list<yfs_client::dirent> list = yfs_client::unserialize(dir_content);
  //std::list<yfs_client::dirent> &list = yfs_client::dir_dirent_map[par_inum];
  
  std::list<yfs_client::dirent>::iterator it = list.begin();
  ret = yfs_client::NOENT;
  while(it != list.end()){
    if(strcmp(name, (*it).name.c_str()) == 0){
      file_inum = (*it).inum;
      ret = yfs_client::OK;
      break;
    }
    ++it;
  }
  
  return ret;
}

std::string
yfs_client::serialize_dirent(yfs_client::dirent dirent){
  std::string result;
  result.append(filename(dirent.inum));
  result.append(":");
  result.append(dirent.name);
  result.append(";");
  return result;
}

std::string
yfs_client::serialize(std::list<yfs_client::dirent> dirent_list){
  std::string result;
  std::list<yfs_client::dirent>::iterator it = dirent_list.begin();
  while(it != dirent_list.end()){
    result.append(filename((*it).inum));
    result.append(":");
    result.append((*it).name);
    result.append(";");
    ++it;
  }
  return result;
}

std::list<yfs_client::dirent>
yfs_client::unserialize(std::string str){
  std::list<yfs_client::dirent> result;
  std::istringstream f(str);
  std::string s;
  while(std::getline(f, s, ';')){
    std::string inum_str = s.substr(0, s.find(":"));
    yfs_client::inum inum = n2i(inum_str);
    std::string name = s.substr(s.find(":") + 1);
    struct yfs_client::dirent dir_ent;
    dir_ent.inum = inum;
    dir_ent.name = name;
    result.push_back(dir_ent);
  }
  return result;
}

int
yfs_client::get_dir_ent(inum par_inum, std::list<dirent> &list){
  status ret;
  // std::map<yfs_client::inum, std::list<dirent> >::iterator it = dir_dirent_map.find(par_inum);
  std::string content;
  if(ec->get(par_inum, content) != extent_protocol::OK){
    ret = IOERR;
    return ret;
  }
  list = unserialize(content);
  ret = OK;
  return ret;
}

int
yfs_client::set_attr_size(inum file_inum, size_t size){
  //acquire the lock
  lc->acquire(file_inum);
  yfs_client::status ret;
  std::string content;
  std::string new_content;
  if(!isfile(file_inum)){
    ret = NOENT;
    goto release;
  }

  if(size >= 0){
    
    extent_protocol::status rr = ec->get(file_inum, content);
    if(ec->get(file_inum, content) != extent_protocol::OK){
      ret = IOERR;
      goto release;
    }
    if(size < content.size()){
      new_content = content.substr(0, size);
    }else if(size > content.size()){
      new_content = content;
      new_content.append(size - content.size(), '\0');
    }else{
      ret = OK;
      goto release;
    }

    if(ec->put(file_inum, new_content) != extent_protocol::OK){
      ret = IOERR;
      goto release;
    }

  }
  ret = OK;

  release:
  lc->release(file_inum);
  return ret;

}

int 
yfs_client::read(inum file_inum, size_t size, off_t off, std::string &buf){
  yfs_client::status ret;
  std::string content;
  if(ec->get(file_inum, content) != extent_protocol::OK){
    ret = IOERR;
    goto release;
  }

  if(off < content.size()){
    buf = content.substr(off, size);
  }

  ret = yfs_client::OK;

  release:
  return ret;

}

int
yfs_client::write(inum file_inum, const char * buf, size_t size, off_t off){
  // acquire the lock client
  lc->acquire(file_inum);
  yfs_client::status ret;
  std::string content;
  std::string added_content;
  std::string new_content;
  if(ec->get(file_inum, content) != extent_protocol::OK){
    ret = IOERR;
    goto release;
  }

  added_content.append(buf, size);

  if(off <= content.size()){
    new_content.append(content.substr(0, off));
    new_content.append(added_content);
    if(off + size < content.size()){
      new_content.append(content.substr(off + size));
    }
  }else{
    new_content.append(content);
    new_content.append(off - content.size(), '\0');
    new_content.append(added_content);
  }

  if(ec->put(file_inum, new_content) != extent_protocol::OK){
    ret = IOERR;
    goto release;
  }

  ret = OK;

  release:
  lc->release(file_inum);
  return ret;
}

int 
yfs_client::unlink(yfs_client::inum par_inum, const char * name){
  // acquire the lock client
  lc->acquire(par_inum);

  yfs_client::status ret;

  yfs_client::inum file_inum;
  
  // get the inum and remove the entry
  std::string dir_content;
  std::string dir_content_update;
  std::list<yfs_client::dirent> list;
  std::list<yfs_client::dirent>::iterator it;
  if(ec->get(par_inum, dir_content)!= extent_protocol::OK){
    ret = IOERR;
    goto release;
  }

  printf("un serialize content\n");
  list = yfs_client::unserialize(dir_content);
  
  it = list.begin();
  while(it != list.end()){
    if(strcmp(name, (*it).name.c_str()) == 0){
      file_inum = (*it).inum;
      if(isdir(file_inum)){
        ret = yfs_client::NOENT;
        goto release;
      }else{
        printf("erase \n");
        it = list.erase(it);
        break;
      }
    }
    ++it;
  }

  // update directory
  dir_content_update = yfs_client::serialize(list);
  printf("new content finish! \n");
  ret = ec->put(par_inum, dir_content_update);
  if(ret != yfs_client::OK){
    ret = yfs_client::IOERR;
    goto release;
  }

  ret = ec->remove(file_inum);
  if(ret != yfs_client::OK){
    ret = yfs_client::IOERR;
    goto release;
  }
  
  ret = yfs_client::OK;

  release:
  lc->release(par_inum);
  return ret;

}
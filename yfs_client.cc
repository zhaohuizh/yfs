// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
// #include <cstdlib>
//#include <string>



yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
  // lc = new lock_client(lock_dst);
  // lc = lock_dst;
  // create root directory
  inum root_inum = 0x00000001;
  std::string str;
  if(ec->put(root_inum, str) == extent_protocol::OK){
    std::list<dirent> root_list;
    dir_dirent_map[root_inum] = root_list;
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
yfs_client::generate_inum(const char * name){
  inum file_inum;
  /*
  while(true){
    file_inum = std::rand();
    printf("random number %016llx\n", file_inum);
    if(isfile(file_inum)){
      break;
    }
  }
  */
  std::string name_str(name);
  file_inum = yfs_client::n2i(name_str);
  return file_inum;
}

int
yfs_client::create_file(inum par_inum, inum file_inum, const char * name){
  status ret;
  
  // call extent_client::put to save file
  // lc->acquire(file_inum);
  std::string inial_content;
  std::string former_content;
  yfs_client::dirent file_dirent;
  if(ec->put(file_inum, inial_content) != extent_protocol::OK){
    ret = IOERR;
    goto release;
  }

  // add entry to parent directory
  
  file_dirent.name = name;
  file_dirent.inum = file_inum;

  
  if(ec->get(par_inum, former_content) != extent_protocol::OK){
    ret = IOERR;
    goto release;
  }


  // std::list<dirent> &dirent_list = dir_dirent_map[par_inum];
  // dirent_list.push_back(file_dirent);

  // update directory content and attributes
  // std::string dir_content = serialize(dirent_list);

  // update directory content and attributes
  former_content.append(yfs_client::serialize_dirent(file_dirent));
  if(ec->put(par_inum, former_content) != extent_protocol::OK){
    ret = IOERR;
    goto release;
  }

  // update the cache map
  dir_dirent_map[par_inum] = unserialize(former_content);
  // another way to do this by add a dirent
  // std::list<dirent> &dirent_list = dir_dirent_map[par_inum];
  // dirent_list.push_back(file_dirent);

  ret = yfs_client::OK;

  release:
  // lc->release(file_inum);
  return ret;
}

bool 
yfs_client::is_exist(inum par_inum, const char * name){
  /*
  std::string dir_content;
  if(ec->get(par_inum, dir_content)!= extent_protocol::OK){
    ret = IOERR;
    return ret;
  }
  std::list<yfs_client::dirent> list = yfs_client::unserialize(dir_content);
  */

  std::list<yfs_client::dirent> &list = yfs_client::dir_dirent_map[par_inum];
  
  std::list<yfs_client::dirent>::iterator it = list.begin();
  while(it != list.end()){
    if(strcmp(name, (*it).name.c_str()) == 0){
      return true;
    }
    ++it;
  }
  return false;
}

yfs_client::inum
yfs_client::get_inum(inum par_inum, const char * name){
  yfs_client::inum file_inum;

  /*
  std::string dir_content;
  if(ec->get(par_inum, dir_content)!= extent_protocol::OK){
    ret = IOERR;
    return ret;
  }
  std::list<yfs_client::dirent> list = yfs_client::unserialize(dir_content);
  */
  std::list<yfs_client::dirent> &list = yfs_client::dir_dirent_map[par_inum];
  std::list<yfs_client::dirent>::iterator it = list.begin();
  while(it != list.end()){
    if(strcmp(name, (*it).name.c_str()) == 0){
      file_inum = (*it).inum;
      break;
    }
    ++it;
  }
  return file_inum;
}

std::string
yfs_client::serialize_dirent(yfs_client::dirent dirent){
  std::string result;
  std::stringstream ss;
  ss << dirent.inum;
  result.append(ss.str());
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
    std::stringstream ss;
    ss << (*it).inum;
    result.append(ss.str());
    result.append(":");
    result.append((*it).name);
    result.append(";");
  }
  return result;
}

std::list<yfs_client::dirent>
yfs_client::unserialize(std::string str){
  std::list<yfs_client::dirent> result;
  std::istringstream f(str);
  std::string s;
  // while(std::getline(f, s, ';')){
  //   std::string inum_str = s.substr(0, s.find(":"));
  //   yfs_client::inum inum = std::strtoull(inum_str.c_str(), NULL, 0);
  //   std::string name = s.substr(s.find(":") + 1);
  //   yfs_client::dirent dir_ent;
  //   dir_ent.inum = inum;
  //   dir_ent.name = name;
  //   result.push_back(dir_ent);
  // }
  return result;
}

int
yfs_client::get_dir_ent(inum par_inum, std::list<dirent> &list){
  status ret;
  std::map<yfs_client::inum, std::list<dirent> >::iterator it = dir_dirent_map.find(par_inum);
  if(it == dir_dirent_map.end()){
    std::string content;
    if(ec->get(par_inum, content) != extent_protocol::OK){
      ret = IOERR;
      return ret;
    }
    list = unserialize(content);
    dir_dirent_map[par_inum] = list;
  }

  list = dir_dirent_map[par_inum];
  ret = OK;
  return ret;
}

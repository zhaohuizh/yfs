// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
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
  // create root directory
  inum root_inum = 0x00000001;
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

  return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
  int r = OK;

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
  std::string inial_content;
  if(ec->put(file_inum, inial_content) != extent_protocol::OK){
    ret = IOERR;
    return ret;
  }
  // add entry to parent directory
  yfs_client::dirent file_dirent;
  file_dirent.name = name;
  file_dirent.inum = file_inum;

  std::list<dirent> &dirent_list = dir_dirent_map[par_inum];
  dirent_list.push_back(file_dirent);

  // update directory content and attributes
  std::string dir_content = serialize(dirent_list);
  if(ec->put(par_inum, dir_content) != extent_protocol::OK){
    ret = IOERR;
    return ret;
  }

  ret = yfs_client::OK;
  return ret;
}

bool 
yfs_client::is_exist(inum par_inum, const char * name){
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
yfs_client::serialize(std::list<dirent> dirent_list){
  std::string result;
  return result;
}


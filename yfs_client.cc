// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdlib>
//#include <string>



yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
  printf("new extent_client \n");
  // lc = new lock_client(lock_dst);
  // lc = lock_dst;
  // create root directory
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
yfs_client::generate_inum(const char * name, bool isFile){
  inum file_inum;
  file_inum = std::rand();
  if(isFile){
    file_inum |= 0x80000000;
  }else{
    file_inum &= 0x7FFFFFFF;
  }
  printf("random number %016llx\n", file_inum);
  return file_inum;
}

int
yfs_client::create_file(inum par_inum, const char * name, inum & file_inum, bool isFile){
  status ret;
  
  // call extent_client::put to save file
  // lc->acquire(file_inum);
  file_inum = yfs_client::generate_inum(name, isFile);
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
  printf("CREATE_FILE: save succeed\n");
  

  // add entry to parent directory
  
  if(ec->get(par_inum, former_content) != extent_protocol::OK){
    ret = IOERR;
    goto release;
  }

  std::cout << "CREATE_FILE: former content " << former_content << std::endl;

  // update directory content and attributes
  former_content.append(yfs_client::serialize_dirent(file_dirent));

  std::cout << "CREATE_FILE: after content " << former_content << std::endl;
  if(ec->put(par_inum, former_content) != extent_protocol::OK){
    ret = IOERR;
    goto release;
  }

  printf("CREATE_FILE: update parent content succeed\n");
  ret = yfs_client::OK;

  release:
  // lc->release(file_inum);
  return ret;
}

bool 
yfs_client::is_exist(inum par_inum, const char * name){
  
  std::string dir_content;
  if(ec->get(par_inum, dir_content)!= extent_protocol::OK){
    printf("IS_EXIST: get dir content fail \n");
  }

  std::cout << "IS_EXIST: get dir content succeed: " << dir_content << std::endl;

  std::list<yfs_client::dirent> list = yfs_client::unserialize(dir_content);
  printf("IS_EXIST: list size %lu\n", list.size());

  // std::list<yfs_client::dirent> &list = yfs_client::dir_dirent_map[par_inum];
  printf("IS_EXIST: iterator before\n");
  std::list<yfs_client::dirent>::iterator it = list.begin();
  printf("IS_EXIST: iterator begin\n");
  while(it != list.end()){
    printf("IS_EXIST: iterator name %s\n", (*it).name.c_str());
    if(strcmp(name, (*it).name.c_str()) == 0){
      printf("IS_EXIST: return true \n");
      return true;
    }
    ++it;
  }
  printf("IS_EXIST: return false\n");
  return false;
}

int
yfs_client::get_inum(inum par_inum, const char * name, inum & file_inum){

  yfs_client::status ret = yfs_client::OK;
  
  std::string dir_content;
  if(ec->get(par_inum, dir_content)!= extent_protocol::OK){
    ret = IOERR;
    return ret;
  }
  std::cout << "GET_INUM: get parent content succeed " << dir_content << ")))" << std::endl;
  std::list<yfs_client::dirent> list = yfs_client::unserialize(dir_content);

  std::cout << "GET_INUM: unserialize succeed with list size " << list.size() << std::endl;
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
yfs_client::set_attr_size(inum file_inum, unsigned int size){
  yfs_client::status ret;
  std::string content;
  std::string new_content;
  if(!isfile(file_inum)){
    ret = NOENT;
    goto release;
  }
  
  if(size > 0){
    /*
    extent_protocol::attr a;
    if(ec->getattr(file_inum, a) != extent_protocol::OK){
      ret = IOERR;
      goto release;
    }

    */

    
    if(!ec->get(file_inum, content) != extent_protocol::OK){
      ret = IOERR;
      goto release;
    }
    if(size < content.size()){
      new_content = content.substr(0, content.size());
    }else if(size > content.size()){
      new_content = content;
      new_content.append(size - content.size(), '\0');
    }
    if(ec->put(file_inum, new_content) != extent_protocol::OK){
      ret = IOERR;
      goto release;
    }

  }
  ret = OK;

  release:
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

  if((size_t)off < content.size()){
    buf = content.substr((size_t)off, size);
  }

  ret = yfs_client::OK;

  release:
  return ret;

}

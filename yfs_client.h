#ifndef yfs_client_h
#define yfs_client_h

#include "extent_client.h"
#include "lock_client.h"
#include <string>
#include <list>
#include "lock_protocol.h"
#include "lock_client_cache.h"

class lock_release_user_impl : public lock_release_user{
  private:
    extent_client* ec;

  public:
    lock_release_user_impl(extent_client* ec);
    // ~lock_release_user_impl();
    void dorelease(lock_protocol::lockid_t);

};

class yfs_client {
  extent_client* ec;
  lock_client_cache* lc;
  lock_release_user_impl* lru;
  
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
  typedef int status;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };

  // std::map<yfs_client::inum, std::list<dirent> > dir_dirent_map;

 private:
  
  static std::string filename(inum);
  static inum n2i(std::string);
  static inum generate_inum(bool);
  
 public:

  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool isdir(inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);
  int create(inum, const char *, inum &, bool);

  int is_exist(inum, const char *, bool &);
  int get_inum(inum, const char *, inum &);
  //inum generate_inum(const char *);
  int get_dir_ent(inum, std::list<dirent> &);
  int set_attr_size(inum, size_t);
  int read(inum, size_t, off_t, std::string &);
  int write(inum, const char *, size_t, off_t);
  int unlink(inum, const char *);

  std::string serialize(std::list<dirent>);
  std::string serialize_dirent(yfs_client::dirent);
  std::list<dirent> unserialize(std::string);
};

#endif 

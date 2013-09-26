#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>
#include <map>
#include <list>


class yfs_client {
  extent_client *ec;
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

  std::map<yfs_client::inum, std::list<dirent> > dir_dirent_map;

 private:
  static std::string filename(inum);
  static inum n2i(std::string);
  static std::string serialize(std::list<dirent>);
  

 public:

  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool isdir(inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);
  int create_file(inum, inum, const char *);

  bool is_exist(inum, const char *);
  inum get_inum(inum, const char *);
  inum generate_inum(const char *);
};

#endif 

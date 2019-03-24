#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include "file_provider.pb.h"
#include "mdwrkapi.hpp"

#define LOG_SYS_ERR               \
  do {                            \
    std::cout << strerror(errno); \
  } while (0)

using namespace file_provider;
using std::make_pair;
using std::string;
using std::unordered_map;

class FileProvider {
 public:
  FileProvider(const string &provider_name, const string &workspace)
      : provider_name_(provider_name), workspace_(workspace) {}

  void run() {
    mdwrk session("tcp://broker:5555", provider_name_, 1);
    zmsg *rsp = nullptr;
    while (1) {
      zmsg *req = session.recv(rsp);
      if (!req) {
        break;  //  Worker was interrupted
      }
      handle(req, rsp);
    }
  }

 private:
  void handle(zmsg *&z_req, zmsg *&z_rsp) {
    Request req;
    req.ParseFromArray(z_req->body(), z_req->body_size());
    // req.ParseFromString(string(z_req->body()));
    Response rsp;
    if (req.command() == Request_CommandType_LIST) {
      handle_list(req, rsp);
    } else {
      handle_fetch(req, rsp);
    }
    delete z_req;
    string rsp_packet = rsp.SerializeAsString();
    z_rsp = new zmsg(rsp_packet.c_str(), rsp_packet.size());
  }

  void handle_list(const Request &req, Response &rsp) {
    DIR *dp;
    struct dirent *ep;
    dp = opendir((workspace_ + '/' + req.path()).c_str());
    if (!dp) {
      LOG_SYS_ERR;
      return;
    }
    while (ep = readdir(dp))
      if (strcmp(ep->d_name, ".") && strcmp(ep->d_name, ".."))
        rsp.add_filename(ep->d_name);
    closedir(dp);
  }

  void handle_fetch(const Request &req, Response &rsp) {
    // assert
    rsp.set_chunk_start(req.chunk_start());
    size_t chunk_size = req.chunk_size();
    char *buf = new char[chunk_size];
    read_one_chunk(workspace_ + '/' + req.path(), buf, req.chunk_start(),
                   chunk_size);
    rsp.set_chunk_data(buf, chunk_size);
    rsp.set_chunk_size(chunk_size);
    delete[] buf;
  }

  char *read_one_chunk(const string &path, char *buf, size_t chunk_start,
                       size_t &chunk_size) {
    FILE *file;
    auto iter = files_.find(path);
    if (iter != files_.end()) {
      file = iter->second;
    } else {
      file = fopen(path.c_str(), "rb");
      if (!file) {
        LOG_SYS_ERR;
        return nullptr;
      }
      iter = files_.emplace(make_pair(path, file)).first;
    }
    if (fseek(file, chunk_start, SEEK_SET) < 0) {
      LOG_SYS_ERR;
      return nullptr;
    }
    size_t nread = fread(buf, 1, chunk_size, file);
    if (nread < 0) {
      LOG_SYS_ERR;
      return nullptr;
    }
    if (nread < chunk_size) {
      chunk_size = nread;
      fclose(file);
      files_.erase(iter);
    }
    return buf;
  }

 private:
  string provider_name_, workspace_;
  unordered_map<string, FILE *> files_;
};

int main(int argc, char *argv[]) {
  FileProvider provider(argv[1], argv[2]);
  provider.run();
  return 0;
}

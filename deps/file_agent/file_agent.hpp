#include "file_provider.pb.h"
#include "logger.hpp"
#include "mdcliapi_async.hpp"
#include <memory>
#include <stdio.h>
#include <string>
#include <sys/types.h>
#include <vector>
#include <zmsg.hpp>

using std::string;

class FileAgent {
public:
  FileAgent(const string &workspace, const string &broker_addr,
            const string &provider_name, bool verbose = false,
            int timeout = 2500, int pipe_size = 10)
      : session_(broker_addr, verbose), workspace_(workspace),
        pipe_size_(pipe_size) {
    session_.set_timeout(timeout);
  }

  bool list(const string &path, std::vector<string> &filenames,
            int timeout = 2500) {
    file_provider::Request req;
    req.set_command(file_provider::Request_CommandType_LIST);
    req.set_path(path);
    string req_str = req.SerializeAsString();
    zmsg *z_req = new zmsg(req_str.c_str(), req_str.size());
    session_.send(provider_name_, z_req);
    zmsg *z_rsp = session_.recv();
    if (!z_rsp) {
      LOG(ERROR) << "list timeout";
      return false;
    }
    file_provider::Response rsp;
    rsp.ParseFromArray(z_rsp->body(), z_rsp->body_size());
    if (rsp.result() == file_provider::Response::ERROR) {
      LOG(ERROR) << "list error";
      return false;
    }
    for (int i = 0; i < rsp.filename_size(); ++i) {
      filenames.push_back(rsp.filename(i));
    }
    return true;
  }

  bool fetch(const string &path, size_t chunk_size = 250000) {
    FILE *file = fopen((workspace_ + path).c_str(), "wb");

    size_t credit = pipe_size_;
    size_t total_bytes = 0;
    size_t chunks = 0;
    size_t chunk_start = 0;

    std::unique_ptr<zmsg> z_rsp;
    while (true) {
      while (credit) {
        file_provider::Request req;
        req.set_command(file_provider::Request_CommandType_FETCH);
        req.set_path(path);
        req.set_chunk_start(chunk_start);
        req.set_chunk_size(chunk_size);
        string req_str = req.SerializeAsString();
        zmsg *z_req = new zmsg(req_str.c_str(), req_str.size());
        session_.send(provider_name_, z_req);
        chunk_start += chunk_size;
        --credit;
      }
      z_rsp.reset(session_.recv());
      if (!z_rsp) {
        LOG(ERROR) << "fetch timeout";
        return false;
      }
      file_provider::Response rsp;
      rsp.ParseFromArray(z_rsp->body(), z_rsp->body_size());
      if (rsp.result() == file_provider::Response::ERROR) {
        LOG(ERROR) << "fetch error";
        return false;
      }
      if (fseek(file, rsp.chunk_start(), SEEK_SET) < 0) {
        LOG_SYS_ERR;
        return false;
      }
      size_t actural_size = rsp.chunk_data().size();
      if (fwrite(rsp.chunk_data().c_str(), 1, actural_size, file) <
          actural_size) {
        LOG_SYS_ERR;
        return false;
      }
      ++chunks;
      ++credit;
      total_bytes += actural_size;
      // last chunk received
      if (actural_size < chunk_size)
        break;
    }
    VLOG(1) << "fetched " << path << ". " << chunks << " chunks received, "
            << total_bytes << " bytes.";
    fclose(file);
    return true;
  }

private:
  mdcli session_;
  string workspace_, provider_name_;
  size_t pipe_size_;
};
#include "file_provider.pb.h"
#include "logger.hpp"
#include "mdcliapi_async.hpp"
#include "uuid4.h"
#include <fcntl.h>
#include <memory>
#include <stdio.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
#include <zmsg.hpp>

using std::string;

class FileAgent {
public:
  FileAgent(const string &workspace, const string &broker_addr,
            const string &provider_name, bool verbose = false,
            int timeout = 3000, int pipe_size = 2)
      : session_(broker_addr, verbose), workspace_(workspace),
        provider_name_(provider_name), pipe_size_(pipe_size) {
    session_.set_timeout(timeout);
  }

  bool list(const string &path, std::vector<string> &filenames) {
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
    bool fetch_done = false;
    string full_path = workspace_ + '/' + path;
    string dirname = full_path.substr(0, full_path.find_last_of('/'));
    if (mkdir(dirname.c_str(), 0755) < 0 && errno != EEXIST) {
      LOG_SYS_ERR;
      return false;
    }
    FILE *file_p = fopen(full_path.c_str(), "w+");
    if (!file_p) {
      LOG_SYS_ERR;
      return false;
    }
    // file destructed before fetch_done
    auto deleter = [&fetch_done, full_path](FILE *ptr) {
      if (fclose(ptr) == EOF)
        LOG_SYS_ERR;
      if (!fetch_done)
        if (unlink(full_path.c_str()) < 0)
          LOG_SYS_ERR;
    };
    std::unique_ptr<FILE, decltype(deleter)> file(file_p, deleter);

    size_t credit = pipe_size_;
    size_t total_bytes = 0;
    size_t chunks = 0;
    long chunk_start = 0;

    if (uuid4_init() == UUID4_EFAILURE) {
      LOG(ERROR) << "uuid4_init fails.";
      return false;
    }
    uuid4_generate(uuid_buf_);
    string fetch_id = uuid_buf_;

    std::unique_ptr<zmsg> z_rsp;
    while (1) {
      while (credit) {
        file_provider::Request req;
        req.set_request_id(fetch_id);
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
      // TCP guarantees FIFO
      z_rsp.reset(session_.recv());
      if (!z_rsp) {
        LOG(ERROR) << "fetch timeout";
        return false;
      }
      file_provider::Response rsp;
      rsp.ParseFromArray(z_rsp->body(), z_rsp->body_size());
      // discard mismatched rsp, i.e. empty chunk from previous fetch
      if (rsp.request_id() != fetch_id)
        continue;
      if (rsp.result() == file_provider::Response::ERROR) {
        LOG(ERROR) << "fetch error";
        return false;
      }
      // last chunk is the final chunk
      if (rsp.chunk_data().empty())
        break;
      if (fseek(file.get(), rsp.chunk_start(), SEEK_SET) < 0) {
        LOG_SYS_ERR;
        return false;
      }
      size_t actural_size = rsp.chunk_data().size();
      if (fwrite(rsp.chunk_data().c_str(), 1, actural_size, file.get()) <
          actural_size) {
        LOG_SYS_ERR;
        return false;
      }
      ++chunks;
      ++credit;
      total_bytes += actural_size;
      // the final chunk received
      if (actural_size < chunk_size)
        break;
    }
    VLOG(1) << "fetched " << path << ". " << chunks << " chunks received, "
            << total_bytes << " bytes.";
    fetch_done = true;
    return true;
  }

private:
  mdcli session_;
  string workspace_, provider_name_;
  size_t pipe_size_;
  char uuid_buf_[UUID4_LEN];
};
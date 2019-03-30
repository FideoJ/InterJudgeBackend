#include "file_provider.pb.h"
#include "logger.hpp"
#include "mdwrkapi.hpp"
#include "uuid4.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unordered_map>
#include <utility>

using std::string;

DEFINE_string(broker_addr, "tcp://broker:5555", "broker address");
DEFINE_string(provider_name, "", "file provider name");
DEFINE_string(workspace, "./workspace", "workspace");
DEFINE_bool(verbose, false, "verbose");

class FileProvider {
public:
  FileProvider(const string &broker_addr, const string &provider_name,
               const string &workspace, bool verbose = false)
      : session_(broker_addr, provider_name, verbose), workspace_(workspace) {}

  void run() {
    zmsg *z_rsp = nullptr;
    while (1) {
      zmsg *z_req = session_.recv(z_rsp);
      //  interrupted
      if (!z_req)
        break;
      handle(z_req, z_rsp);
    }
  }

private:
  void handle(zmsg *&z_req, zmsg *&z_rsp) {
    file_provider::Request req;
    req.ParseFromArray(z_req->body(), z_req->body_size());
    file_provider::Response rsp;
    switch (req.command()) {
    case file_provider::Request::LIST:
      handle_list_command(req, rsp);
      break;
    case file_provider::Request::FETCH:
      handle_fetch_command(req, rsp);
      break;
    default:
      rsp.set_result(file_provider::Response::ERROR);
      LOG(ERROR) << "unknown command";
    }
    delete z_req;
    string rsp_str = rsp.SerializeAsString();
    z_rsp = new zmsg(rsp_str.c_str(), rsp_str.size());
  }

  void handle_list_command(const file_provider::Request &req,
                           file_provider::Response &rsp) {
    DIR *dp = opendir((workspace_ + '/' + req.path()).c_str());
    if (!dp) {
      LOG_SYS_ERR;
      rsp.set_result(file_provider::Response::ERROR);
      return;
    }
    struct dirent *ep;
    while (ep = readdir(dp)) {
      if (strcmp(ep->d_name, ".") && strcmp(ep->d_name, ".."))
        rsp.add_filename(ep->d_name);
    }
    rsp.set_result(file_provider::Response::SUCCESS);
    closedir(dp);
  }

  void handle_fetch_command(const file_provider::Request &req,
                            file_provider::Response &rsp) {
    size_t chunk_size = req.chunk_size();
    if (chunk_size > max_chunk_size_ ||
        !read_one_chunk(workspace_ + '/' + req.path(), req.chunk_start(),
                        chunk_size)) {
      rsp.set_result(file_provider::Response::ERROR);
    }
    rsp.set_chunk_start(req.chunk_start());
    rsp.set_chunk_data(buf_, chunk_size);
    rsp.set_result(file_provider::Response::SUCCESS);
  }

  bool read_one_chunk(const string &path, size_t chunk_start,
                      size_t &chunk_size) {
    FILE *file;
    auto iter = files_.find(path);
    if (iter != files_.end()) {
      file = iter->second;
    } else {
      file = fopen(path.c_str(), "rb");
      if (!file) {
        LOG_SYS_ERR;
        return false;
      }
      iter = files_.emplace(std::make_pair(path, file)).first;
    }
    if (fseek(file, chunk_start, SEEK_SET) < 0) {
      LOG_SYS_ERR;
      return false;
    }
    chunk_size = fread(buf_, 1, chunk_size, file);
    if (ferror(file)) {
      LOG_SYS_ERR;
      return false;
    }
    if (feof(file)) {
      fclose(file);
      files_.erase(iter);
    }
    return true;
  }

private:
  mdwrk session_;
  string workspace_;
  std::unordered_map<string, FILE *> files_;
  static constexpr size_t max_chunk_size_ = 250000;
  char buf_[max_chunk_size_];
};

int main(int argc, char *argv[]) {
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();
  if (FLAGS_provider_name.empty()) {
    if (uuid4_init() == UUID4_EFAILURE)
      LOG(FATAL) << "uuid4_init fails.";
    char buf[UUID4_LEN];
    uuid4_generate(buf);
    FLAGS_provider_name = buf;
  }
  if (mkdir(FLAGS_workspace.c_str(), 0755) < 0)
    LOG_FATAL_SYS_ERR;

  FileProvider provider(FLAGS_broker_addr, FLAGS_provider_name, FLAGS_workspace,
                        FLAGS_verbose);
  provider.run();
  return 0;
}

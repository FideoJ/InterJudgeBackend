#include "compiler_gpp.pb.h"
#include "file_agent.hpp"
#include "mdwrkapi.hpp"
#include <iostream>
#include <string.h>
#include <string>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
extern "C" {
#include "sandbox.h"
}

using std::string;

DEFINE_string(workspace, "./workspace", "workspace");
DEFINE_string(file_provider, "", "local file provider");
DEFINE_string(compile_script, "./compile.sh", "compile script location");
DEFINE_string(broker_addr, "tcp://broker:5555", "broker address");
DEFINE_string(service_name, "compiler_gpp", "service name");
DEFINE_bool(verbose, false, "verbose");

class CompilerGpp {
private:
  enum class _PathType { SUBMISSION = 0, PROBLEM };

public:
  CompilerGpp(const string &workspace, const string &file_provider,
              const string &compile_script, const string &broker_addr,
              const string &service_name = "compiler_gpp", bool verbose = false)
      : session_(broker_addr, service_name, verbose), verbose_(verbose),
        broker_addr_(broker_addr), workspace_(workspace),
        file_provider_(file_provider), compile_script_(compile_script) {
    if (access(compile_script.c_str(), X_OK) < 0)
      LOG_FATAL_SYS_ERR;
    if (mkdir(workspace_.c_str(), 0755) < 0 && errno != EEXIST)
      LOG_FATAL_SYS_ERR;
    string sub_dir = workspace_ + "/submission";
    if (mkdir(sub_dir.c_str(), 0755) < 0 && errno != EEXIST)
      LOG_FATAL_SYS_ERR;
    string prob_dir = workspace_ + "/problem";
    if (mkdir(prob_dir.c_str(), 0755) < 0 && errno != EEXIST)
      LOG_FATAL_SYS_ERR;
  }

  void run() {
    zmsg *z_rsp = nullptr;
    while (1) {
      zmsg *z_req = session_.recv(z_rsp);
      //  interrupted
      if (!z_req) {
        break;
      }
      handle(z_req, z_rsp);
    }
  }

private:
  void handle(zmsg *&z_req, zmsg *&z_rsp) {
    compiler_gpp::Request req;
    req.ParseFromArray(z_req->body(), z_req->body_size());
    compiler_gpp::Response rsp;
    handle_compiling(req, rsp);
    rsp.set_request_id(req.request_id());
    rsp.set_file_provider(file_provider_);
    string rsp_str = rsp.SerializeAsString();
    delete z_req;
    z_rsp = new zmsg(rsp_str.c_str(), rsp_str.size());
  }

  void handle_compiling(const compiler_gpp::Request &req,
                        compiler_gpp::Response &rsp) {
    // get src and header files
    FileAgent agent(workspace_, broker_addr_, req.file_provider(), verbose_);
    for (int idx = 0; idx < req.sub_src_filename_size(); ++idx) {
      if (!ensure_file(_PathType::SUBMISSION, req.sub_src_filename(idx), req,
                       &agent)) {
        rsp.set_result(compiler_gpp::Response::SYSTEM_ERROR);
        return;
      }
    }
    for (int idx = 0; idx < req.sub_header_filename_size(); ++idx) {
      if (!ensure_file(_PathType::SUBMISSION, req.sub_header_filename(idx), req,
                       &agent)) {
        rsp.set_result(compiler_gpp::Response::SYSTEM_ERROR);
        return;
      }
    }
    for (int idx = 0; idx < req.prob_src_filename_size(); ++idx) {
      if (!ensure_file(_PathType::PROBLEM, req.prob_src_filename(idx), req,
                       &agent)) {
        rsp.set_result(compiler_gpp::Response::SYSTEM_ERROR);
        return;
      }
    }
    for (int idx = 0; idx < req.prob_header_filename_size(); ++idx) {
      if (!ensure_file(_PathType::PROBLEM, req.prob_header_filename(idx), req,
                       &agent)) {
        rsp.set_result(compiler_gpp::Response::SYSTEM_ERROR);
        return;
      }
    }

    struct result ret = {};
    do_compiling(&ret, req);
    set_compiling_result(&ret, req, rsp);
  }

  string path_of(const string &workspace, _PathType path_type, int id,
                 const string &filename) {
    string path;
    if (!workspace.empty()) {
      path += workspace;
      path += '/';
    }
    switch (path_type) {
    case _PathType::SUBMISSION:
      path += "submission/";
      break;
    case _PathType::PROBLEM:
      path += "problem/";
      break;
    default:
      return "";
    }
    path += std::to_string(id);
    path += '/';
    path += filename;
    return path;
  }

  bool ensure_file(_PathType path_type, const string &filename,
                   const compiler_gpp::Request &req, FileAgent *agent) {
    int id = path_type == _PathType::SUBMISSION ? req.sub_id() : req.prob_id();
    string path = path_of("", path_type, id, filename);
    if (req.file_provider() != file_provider_)
      return agent && agent->fetch(path);

    string full_path = workspace_ + '/' + path;
    if (access(full_path.c_str(), R_OK) < 0) {
      if (errno == ENOTDIR || errno == ENOENT) {
        return agent && agent->fetch(path);
      } else {
        LOG_SYS_ERR;
        return false;
      }
    }
    return true;
  }

  void do_compiling(struct result *ret, const compiler_gpp::Request &req) {
    struct config cfg;
    cfg.max_cpu_time = 10 * 1000;
    cfg.max_real_time = 15 * 1000;
    cfg.memory_limit_check_only = 0;
    cfg.max_memory = 512 * 1024 * 1024;
    cfg.max_process_number = 200;
    cfg.max_output_size = 20 * 1024 * 1024;
    cfg.max_stack = 32 * 1024 * 1024;
    cfg.seccomp_rule_name = nullptr;
    cfg.uid = -1;
    cfg.gid = -1;

    cfg.exe_path = (char *)compile_script_.c_str();
    cfg.input_path = nullptr;

    string output_path =
        path_of(workspace_, _PathType::SUBMISSION, req.sub_id(), "compile.log");
    cfg.output_path = (char *)output_path.c_str();
    cfg.error_path = cfg.output_path;

    string log_path =
        path_of(workspace_, _PathType::SUBMISSION, req.sub_id(), "sandbox.log");
    cfg.log_path = (char *)log_path.c_str();

    cfg.env[0] = nullptr;
    cfg.args[0] = (char *)"compile.sh";

    string include_dir1 =
        path_of(workspace_, _PathType::SUBMISSION, req.sub_id(), "");
    string include_dir2 =
        path_of(workspace_, _PathType::PROBLEM, req.prob_id(), "");
    cfg.args[1] = (char *)include_dir1.c_str();
    cfg.args[2] = (char *)include_dir2.c_str();
    cfg.args[3] = (char *)include_dir1.c_str();

    int argc = 4;
    std::vector<string> src_path_vec;
    for (int idx = 0; idx < req.sub_src_filename_size(); ++idx) {
      src_path_vec.emplace_back(path_of(workspace_, _PathType::SUBMISSION,
                                        req.sub_id(),
                                        req.sub_src_filename(idx)));
      cfg.args[argc + idx] = (char *)src_path_vec[idx].c_str();
    }
    argc += req.sub_src_filename_size();
    for (int idx = 0; idx < req.prob_src_filename_size(); ++idx) {
      src_path_vec.emplace_back(path_of(workspace_, _PathType::PROBLEM,
                                        req.prob_id(),
                                        req.prob_src_filename(idx)));
      cfg.args[argc + idx] = (char *)src_path_vec[idx].c_str();
    }
    argc += req.prob_src_filename_size();
    cfg.args[argc] = nullptr;

    ::run(&cfg, ret);
    VLOG(1) << ret->cpu_time << " " << ret->real_time << " " << ret->memory
            << " " << ret->signal << " " << ret->exit_code << " " << ret->error
            << " " << ret->result;
  }

  void set_compiling_result(const struct result *ret,
                            const compiler_gpp::Request &req,
                            compiler_gpp::Response &rsp) {
    rsp.set_result(compiler_gpp::Response::SYSTEM_ERROR);
    if (ret->result == ::SYSTEM_ERROR)
      return;
    if (ret->result == ::RUNTIME_ERROR && ret->exit_code == 1) {
      string compile_log = path_of(workspace_, _PathType::SUBMISSION,
                                   req.sub_id(), "compile.log");
      FILE *file = fopen(compile_log.c_str(), "r");
      if (!file) {
        LOG_SYS_ERR;
        return;
      }
      size_t nread = fread(log_buf_, 1, max_log_size, file);
      if (ferror(file)) {
        LOG_SYS_ERR;
        return;
      }
      if (!feof(file)) {
        strncpy(log_buf_ + max_log_size - 4, "...", 4);
      } else {
        log_buf_[nread] = '\0';
      }
      rsp.set_result(compiler_gpp::Response::FAIL);
      rsp.set_detail(log_buf_);
    } else if (ret->result == ::SUCCESS) {
      rsp.set_result(compiler_gpp::Response::SUCCESS);
    } else {
      rsp.set_result(compiler_gpp::Response::FAIL);
      rsp.set_detail("Malicious code detected.");
    }
  }

private:
  mdwrk session_;
  bool verbose_;
  string broker_addr_, workspace_, file_provider_, compile_script_;
  static constexpr size_t max_log_size = 10 * 1024;
  char log_buf_[max_log_size];
};

int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();
  if (FLAGS_file_provider.empty())
    LOG(FATAL) << "local file provider not set.";
  CompilerGpp compiler(FLAGS_workspace, FLAGS_file_provider,
                       FLAGS_compile_script, FLAGS_broker_addr,
                       FLAGS_service_name, FLAGS_verbose);
  compiler.run();
  return 0;
}

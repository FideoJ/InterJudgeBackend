#include "file_agent.hpp"
#include "logger.hpp"
#include "mdwrkapi.hpp"
#include "tester.pb.h"
#include "thread_pool.hpp"
#include <ctype.h>
#include <functional>
#include <future>
#include <gflags/gflags.h>
#include <stdio.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>
extern "C" {
#include "sandbox.h"
}

using std::string;

DEFINE_string(workspace, "./workspace", "workspace");
DEFINE_string(file_provider, "", "local file provider");
DEFINE_string(broker_addr, "tcp://broker:5555", "broker address");
DEFINE_string(service_name, "tester", "service name");
DEFINE_bool(verbose, false, "verbose");
DEFINE_uint64(thread_pool_size, 0, "thread pool size");

class Tester {
private:
  enum class _PathType { SUBMISSION = 0, PROBLEM };

public:
  Tester(const string &workspace, const string &file_provider,
         const string &broker_addr, const string &service_name = "tester",
         bool verbose = false, size_t thread_pool_size = 8)
      : session_(broker_addr, service_name, verbose), verbose_(verbose),
        broker_addr_(broker_addr), workspace_(workspace),
        file_provider_(file_provider), thread_pool_(thread_pool_size) {
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
    tester::Request req;
    req.ParseFromArray(z_req->body(), z_req->body_size());
    tester::Response rsp;
    handle_testing(req, rsp);
    rsp.set_request_id(req.request_id());
    rsp.set_file_provider(file_provider_);
    string rsp_str = rsp.SerializeAsString();
    delete z_req;
    z_rsp = new zmsg(rsp_str.c_str(), rsp_str.size());
  }

  void handle_testing(const tester::Request &req, tester::Response &rsp) {
    // get exectable file
    FileAgent agent(workspace_, broker_addr_, req.exec_file_provider(),
                    verbose_);
    if (!ensure_file(_PathType::SUBMISSION, "submission", req,
                     req.exec_file_provider(), &agent)) {
      rsp.set_result(tester::Response::SYSTEM_ERROR);
      return;
    };
    string exec_file =
        path_of(workspace_, _PathType::SUBMISSION, req.sub_id(), "submission");
    if (chmod(exec_file.c_str(), 0755) < 0) {
      LOG_SYS_ERR;
      rsp.set_result(tester::Response::SYSTEM_ERROR);
      return;
    }
    // fetch one at a time to avoid requesting too quickly
    for (int idx = 0; idx < req.test_case_id_size(); ++idx) {
      string in_filename = std::to_string(req.test_case_id(idx)) + ".in";
      string expected_out_filename =
          std::to_string(req.test_case_id(idx)) + ".out";
      if (!(ensure_file(_PathType::PROBLEM, in_filename, req,
                        req.test_file_provider(), &agent) &&
            ensure_file(_PathType::PROBLEM, expected_out_filename, req,
                        req.test_file_provider(), &agent))) {
        rsp.set_result(tester::Response::SYSTEM_ERROR);
        return;
      }
    }
    // run each test case in thread pool
    int test_case_id_size = req.test_case_id_size();
    std::vector<std::future<void>> future_vec;
    std::vector<struct result> ret_vec(test_case_id_size);
    for (int idx = 0; idx < req.test_case_id_size(); ++idx) {
      future_vec.emplace_back(thread_pool_.enqueue(std::bind(
          &Tester::do_testing, this, &ret_vec[idx], std::cref(req), idx)));
    }
    // read results and set rsp
    // init rsp result as the best one
    rsp.set_result(tester::Response::ACCEPTED);
    for (int idx = 0; idx < req.test_case_id_size(); ++idx) {
      future_vec[idx].get();
      struct result *ret = &ret_vec[idx];
      auto test_case = rsp.add_test_case();
      test_case->set_test_case_id(req.test_case_id(idx));
      test_case->set_cpu_time(ret->cpu_time);
      test_case->set_memory(ret->memory);
      test_case->set_exit_code(ret->exit_code);
      test_case->set_signal(ret->signal);
      switch (ret->result) {
      case ::CPU_TIME_LIMIT_EXCEEDED:
      case ::REAL_TIME_LIMIT_EXCEEDED:
        test_case->set_result(tester::Response::TIME_LIMIT_EXCEEDED);
        break;
      case ::MEMORY_LIMIT_EXCEEDED:
        test_case->set_result(tester::Response::MEMORY_LIMIT_EXCEEDED);
        break;
      case ::RUNTIME_ERROR:
        test_case->set_result(tester::Response::RUNTIME_ERROR);
        break;
      case ::SYSTEM_ERROR:
        test_case->set_result(tester::Response::SYSTEM_ERROR);
        break;
      default:
        // exit normally
        string actural_out_file =
            path_of(workspace_, _PathType::SUBMISSION, req.sub_id(),
                    std::to_string(req.test_case_id(idx)) + ".actural");
        string expected_out_file =
            path_of(workspace_, _PathType::PROBLEM, req.prob_id(),
                    std::to_string(req.test_case_id(idx)) + ".out");
        test_case->set_result(compare(actural_out_file, expected_out_file));
      }
      // set rsp result as the WORST result of test cases
      if (test_case->result() > rsp.result())
        rsp.set_result(test_case->result());
    }
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
                   const tester::Request &req, const string &file_provider,
                   FileAgent *agent) {
    int id = path_type == _PathType::SUBMISSION ? req.sub_id() : req.prob_id();
    string path = path_of("", path_type, id, filename);
    // if (file_provider != file_provider_)
    //   return agent && agent->fetch(path);

    string full_path = workspace_ + '/' + path;
    if (access(full_path.c_str(), R_OK) < 0) {
      if ((errno == ENOTDIR || errno == ENOENT) && agent) {
        if (!agent->fetch(path)) {
          // check if someone has fetched
          sleep(1);
          return access(full_path.c_str(), R_OK) == 0;
        }
      } else {
        LOG_SYS_ERR;
        return false;
      }
    }
    return true;
  }

  void do_testing(struct result *ret, const tester::Request &req, int idx) {
    // string in_filename = std::to_string(req.test_case_id(idx)) + ".in";
    // string expected_out_filename =
    //     std::to_string(req.test_case_id(idx)) + ".out";
    // FileAgent agent(workspace_, broker_addr_, req.test_file_provider(),
    //                 verbose_);
    // if (!(ensure_file(_PathType::PROBLEM, in_filename, req,
    //                   req.test_file_provider(), &agent) &&
    //       ensure_file(_PathType::PROBLEM, expected_out_filename, req,
    //                   req.test_file_provider(), &agent))) {
    //   ret->result = ::SYSTEM_ERROR;
    //   return;
    // }
    run_test_case(ret, req, idx);
  }

  void run_test_case(struct result *ret, const tester::Request &req, int idx) {
    struct config cfg;
    cfg.max_cpu_time = req.max_cpu_time();
    cfg.max_real_time = req.max_cpu_time() * 2;
    cfg.memory_limit_check_only = 0;
    cfg.max_memory = req.max_memory();
    cfg.max_process_number = 200;
    cfg.max_output_size = 1024 * 1024;
    cfg.max_stack = 32 * 1024 * 1024;
    string exec_file =
        path_of(workspace_, _PathType::SUBMISSION, req.sub_id(), "submission");
    string in_file = path_of(workspace_, _PathType::PROBLEM, req.prob_id(),
                             std::to_string(req.test_case_id(idx)) + ".in");
    string actural_out_file =
        path_of(workspace_, _PathType::SUBMISSION, req.sub_id(),
                std::to_string(req.test_case_id(idx)) + ".actural");
    cfg.exe_path = (char *)exec_file.c_str();
    cfg.input_path = (char *)in_file.c_str();
    cfg.output_path = (char *)actural_out_file.c_str();
    cfg.error_path = (char *)actural_out_file.c_str();
    cfg.args[0] = nullptr;
    cfg.env[0] = nullptr;
    string log_path =
        path_of(workspace_, _PathType::SUBMISSION, req.sub_id(), "sandbox.log");
    cfg.log_path = (char *)log_path.c_str();
    cfg.seccomp_rule_name = (char *)"c_cpp";
    // nobody user
    cfg.uid = 65534;
    cfg.gid = 65534;
    ::run(&cfg, ret);
  }

  tester::Response::ResultType compare(const string &actural,
                                       const string &expected) {
    _CharStream s1(actural), s2(expected);
    char ch1, ch2;
    bool ret1, ret2;
    while (1) {
      ret1 = s1.next_char(ch1);
      ret2 = s2.next_char(ch2);
      if (s1.error() || s2.error())
        return tester::Response::SYSTEM_ERROR;
      if (!ret1 && !ret2)
        return tester::Response::ACCEPTED;
      if (!ret1 || !ret2 || ch1 != ch2)
        break;
    }
    while (1) {
      ret1 = s1.next_non_space_char(ch1);
      ret2 = s2.next_non_space_char(ch2);
      if (s1.error() || s2.error())
        return tester::Response::SYSTEM_ERROR;
      if (!ret1 && !ret2)
        return tester::Response::PRESENTATION_ERROR;
      if (!ret1 || !ret2 || ch1 != ch2)
        return tester::Response::WRONG_ANSWER;
    }
  }

private:
  class _CharStream {
  public:
    _CharStream(const string &path) {
      file_ = fopen(path.c_str(), "r");
      if (!file_) {
        LOG_SYS_ERR;
        error_ = true;
      } else {
        nread_ = fread(buf_, 1, chunk_size_, file_);
        if (ferror(file_)) {
          LOG_SYS_ERR;
          error_ = true;
        }
      }
    }

    ~_CharStream() {
      if (file_ && fclose(file_) == EOF)
        LOG_SYS_ERR;
    }

    bool error() const { return error_; }

    bool next_char(char &ch) {
      if (error_)
        return false;
      if (idx_ == nread_) {
        if (feof(file_))
          return false;
        nread_ = fread(buf_, 1, chunk_size_, file_);
        if (ferror(file_)) {
          LOG_SYS_ERR;
          error_ = true;
          return false;
        }
        if (nread_ == 0)
          return false;
        idx_ = 0;
      }
      ch = buf_[idx_];
      ++idx_;
      return true;
    }

    bool next_non_space_char(char &ch) {
      bool has_next_char;
      while ((has_next_char = next_char(ch)) && isspace(ch))
        ;
      return has_next_char;
    }

  private:
    FILE *file_;
    static constexpr size_t chunk_size_ = 4096;
    char buf_[chunk_size_];
    size_t nread_;
    size_t idx_ = 0;
    bool error_ = false;
  };

private:
  mdwrk session_;
  bool verbose_;
  string broker_addr_, workspace_, file_provider_;
  ThreadPool thread_pool_;
};

int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();
  if (FLAGS_file_provider.empty())
    LOG(FATAL) << "local file provider not set.";
  size_t thread_pool_size = FLAGS_thread_pool_size > 0
                                ? FLAGS_thread_pool_size
                                : (std::thread::hardware_concurrency() > 0
                                       ? std::thread::hardware_concurrency()
                                       : 2);
  Tester tester(FLAGS_workspace, FLAGS_file_provider, FLAGS_broker_addr,
                FLAGS_service_name, FLAGS_verbose, thread_pool_size);
  tester.run();
  return 0;
}

#include "mdwrkapi.hpp"
#include <iostream>
extern "C" {
#include "sandbox.h"
}

class tester {
public:
  void run() {
    mdwrk session("tcp://broker:5555", "tester", 1);
    zmsg *rsp = nullptr;
    while (1) {
      zmsg *req = session.recv(rsp);
      if (!req) {
        break; //  Worker was interrupted
      }
      handle(req, rsp);
    }
  }

private:
  void handle(zmsg *&req, zmsg *&rsp) {
    struct result ret = test();
    std::cout << ret.result << " " << ret.error << std::endl;
    if (ret.result == SUCCESS && ret.error == SUCCESS) {
      rsp = new zmsg("tests passed.");
    } else {
      rsp = new zmsg("tests failed.");
    }
    delete req;
    req = nullptr;
  }

  struct result test() {
    struct config cfg;
    cfg.max_cpu_time = 1000;
    cfg.max_real_time = 5000;
    cfg.max_memory = 128 * 1024 * 1024;
    cfg.max_process_number = 200;
    cfg.max_output_size = 10000;
    cfg.max_stack = 32 * 1024 * 1024;
    cfg.exe_path = "submission";
    cfg.input_path = "1.in";
    cfg.output_path = "1.out";
    cfg.error_path = "1.out";
    cfg.args[0] = NULL;
    cfg.env[0] = NULL;
    cfg.log_path = "judger.log";
    cfg.seccomp_rule_name = "c_cpp";
    cfg.uid = 65534;
    cfg.gid = 65534;
    struct result ret = {};
    ::run(&cfg, &ret);
    return ret;
  }
};

int main() {
  tester tester;
  tester.run();
  return 0;
}

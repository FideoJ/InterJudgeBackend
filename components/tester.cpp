#include "mdwrkapi.hpp"
#include <iostream>
#include <stdexcept>
#include <stdio.h>
#include <string>

std::string exec(const char *cmd) {
  char buffer[1024];
  std::string result = "";
  FILE *pipe = popen(cmd, "r");
  if (!pipe)
    throw std::runtime_error("popen() failed!");
  try {
    while (fgets(buffer, sizeof buffer, pipe) != NULL) {
      result += buffer;
    }
  } catch (...) {
    pclose(pipe);
    throw;
  }
  pclose(pipe);
  return result;
}

int main(int argc, char *argv[]) {
  int verbose = (argc > 1 && strcmp(argv[1], "-v") == 0);
  mdwrk session("tcp://localhost:5555", "Tester", verbose);

  zmsg *reply = 0;
  char sys_comand[101];
  while (1) {
    zmsg *request = session.recv(reply);
    if (request == 0) {
      break; //  Worker was interrupted
    }
    snprintf(sys_comand, 100, "%s", request->body());
    std::string ret = exec(sys_comand);
    if (!ret.compare(sys_comand)) {
      reply = new zmsg("Tests passed.");
    } else {
      reply = new zmsg("Tests Failed.");
    }
    delete request;
  }
  return 0;
}

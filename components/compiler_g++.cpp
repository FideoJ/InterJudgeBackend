#include "mdwrkapi.hpp"

int main(int argc, char *argv[]) {
  int verbose = (argc > 1 && strcmp(argv[1], "-v") == 0);
  mdwrk session("tcp://localhost:5555", "Compiler:g++", verbose);

  zmsg *reply = 0;
  char sys_comand[101];
  while (1) {
    zmsg *request = session.recv(reply);
    if (request == 0) {
      break;  //  Worker was interrupted
    }
    snprintf(sys_comand, 100, "g++ %s -std=c++11", request->body());
    int ret = system(sys_comand);
    if (!ret) {
      reply = new zmsg("Compilation passed.");
    } else {
      reply = new zmsg("Compilation failed.");
    }
    delete request;
  }
  return 0;
}

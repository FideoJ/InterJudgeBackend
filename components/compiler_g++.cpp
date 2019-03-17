#include "mdwrkapi.hpp"

int main(int argc, char *argv[]) {
  mdwrk session("tcp://broker:5555", "Compiler:g++", 1);

  zmsg *reply = 0;
  char sys_comand[101];
  while (1) {
    zmsg *request = session.recv(reply);
    if (request == 0) {
      break; //  Worker was interrupted
    }
    snprintf(sys_comand, 100, "g++ %s -std=c++11 -o submission",
             request->body());
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

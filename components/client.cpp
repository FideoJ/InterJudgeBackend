#include "mdcliapi.hpp"
#include <string>

int main(int argc, char *argv[]) {
  // int verbose = (argc > 1 && strcmp(argv[1], "-v") == 0);

  mdcli session("tcp://broker:5555", 1);
  session.set_timeout(10000);

  int count;
  for (count = 0; count < 1; count++) {
    zmsg *request = new zmsg(argv[1]);
    zmsg *reply = session.send("Compiler:g++", request);
    if (reply) {
      std::string com_ret = reply->body();
      delete reply;
      if (com_ret.compare("Compilation passed.")) {
        break; // Compilation failed.
      }
      zmsg *request = new zmsg("dummy");
      zmsg *reply = session.send("Tester", request);
      if (reply) {
        delete reply;
      } else {
        break; //  Interrupt or failure
      }
    } else {
      break; //  Interrupt or failure
    }
  }
  std::cout << count << " requests/replies processed" << std::endl;
  return 0;
}

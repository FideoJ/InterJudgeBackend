#include <stdio.h>
#include <sys/types.h>
#include <string>
#include <vector>
#include <zmsg.hpp>
#include "file_provider.pb.h"
#include "mdcliapi2.hpp"

using std::string;
using std::vector;
using namespace file_provider;

class FileAgent {
  //  private:
 public:
  // FileAgent() {}

  bool list(const string &path, vector<string> &filenames) {
    Request req;
    req.set_command(Request_CommandType_LIST);
    req.set_path(path);
    string req_packet = req.SerializeAsString();
    zmsg *z_req = new zmsg(req_packet.c_str(), req_packet.size());
    mdcli session("tcp://broker:5555", 1);
    session.send("cool", z_req);
    zmsg *z_rsp = session.recv();
    if (!z_rsp) return false;
    Response rsp;
    rsp.ParseFromArray(z_rsp->body(), z_rsp->body_size());
    for (int i = 0; i < rsp.filename_size(); ++i) {
      filenames.push_back(rsp.filename(i));
      printf("%s\n", filenames[i].c_str());
    }
    return true;
  }

  bool fetch(string path, size_t chunk_size) {
    mdcli session("tcp://broker:5555", 1);
    FILE *file = fopen("/tmp/test.txt", "w");

    size_t credit = pipe_size_;
    size_t total = 0;        //  Total bytes received
    size_t chunks = 0;       //  Total chunks received
    size_t chunk_start = 0;  //  Offset of next chunk request

    // retry
    while (true) {
      while (credit) {
        Request req;
        req.set_command(Request_CommandType_FETCH);
        req.set_path(path);
        req.set_chunk_start(chunk_start);
        req.set_chunk_size(chunk_size);
        string req_packet = req.SerializeAsString();
        zmsg *z_req = new zmsg(req_packet.c_str(), req_packet.size());
        session.send("cool", z_req);
        chunk_start += chunk_size;
        credit--;
      }
      zmsg *z_rsp = session.recv();
      if (!z_rsp) return false;
      Response rsp;
      rsp.ParseFromArray(z_rsp->body(), z_rsp->body_size());
      fseek(file, rsp.chunk_start(), SEEK_SET);
      size_t size = rsp.chunk_data().size();
      fwrite(rsp.chunk_data().c_str(), 1, size, file);
      delete z_rsp;
      chunks++;
      credit++;
      total += size;
      if (size < chunk_size) break;  //  Last chunk received; exit
    }
    printf("%zd chunks received, %zd bytes\n", chunks, total);
    fclose(file);
    return true;
  }

 private:
  size_t pipe_size_ = 10;
};
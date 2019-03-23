#include "file_provider.pb.h"
#include <string>
#include <vector>

using std::string;
using std::vector;
using namespace file_provider;

class FileFetcher {
private:
  string get_dirname(const Request &req) {
    string dirname;
    if (!custom_path_.empty()) {
      dirname = custom_path_;
    } else {
      dirname =
          path_type_ == Request_PathType_PROBLEM ? "problems/" : "submissions/";
      dirname += std::to_string(id_);
    }
    return dirname;
  }

  string get_path(const Request &req) {
    if (req.has_custom_path()) {
      return workspace_ + '/' + req.custom_path();
    } else {
      return get_dirname(req) + '/' + req.filename();
    }
  }

  bool list(vector &filenames) {}

  bool fetch(string filename) { Request req; }

private:
  Request_PathType path_type_;
  int id_;
  string workspace_, custom_path_;
}
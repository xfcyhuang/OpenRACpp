#pragma once
import std;

namespace ora::net {

using HttpProgressFn = std::function<void(long long, long long, int)>;

struct HttpResult {
  bool b_ok = false;
  int int4_status = 0;
  std::string str_body;
  std::string str_filename;
};

HttpResult HttpGetSync(const std::string& str_url,
                       const HttpProgressFn& fn_progress = nullptr);

bool HttpPostSync(const std::string& str_url, const std::string& str_body,
                  std::string& str_response_out);

}

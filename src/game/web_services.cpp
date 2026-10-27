#include "game/web_services.hpp"

#include "net/http_sync.hpp"

namespace ora::game {

std::string EscapeDataString(std::string_view sv_value) {
  static constexpr char kHexUpper[] = "0123456789ABCDEF";
  std::string str_out;
  for (const unsigned char u1_byte : sv_value) {
    const bool b_unreserved =
        (u1_byte >= 'A' && u1_byte <= 'Z') ||
        (u1_byte >= 'a' && u1_byte <= 'z') ||
        (u1_byte >= '0' && u1_byte <= '9') || u1_byte == '-' ||
        u1_byte == '_' || u1_byte == '.' || u1_byte == '~';
    if (b_unreserved) {
      str_out.push_back(static_cast<char>(u1_byte));
    } else {
      str_out.push_back('%');
      str_out.push_back(kHexUpper[u1_byte >> 4]);
      str_out.push_back(kHexUpper[u1_byte & 0xF]);
    }
  }
  return str_out;
}

std::string BuildHttpQuery(
    std::string_view sv_url,
    std::initializer_list<std::pair<std::string_view, std::string_view>>
        vec_parameters) {
  std::string str_out{sv_url};
  str_out.push_back('?');
  for (const auto& [sv_name, sv_value] : vec_parameters) {
    str_out.append(sv_name);
    str_out.push_back('=');
    str_out.append(EscapeDataString(sv_value));
    str_out.push_back('&');
  }
  return str_out;
}

void WebServices::CheckModVersion() {

  const std::string str_query_url =
      BuildHttpQuery(VersionCheck,
                     {{"protocol", "1"},
                      {"engine", str_engine_version_},
                      {"mod", str_mod_id_},
                      {"version", str_mod_version_}});

  std::thread{[this, str_query_url]() mutable {
    const net::HttpResult result_get = net::HttpGetSync(str_query_url);
    if (!result_get.b_ok)
      return;

    auto kind_status = ModVersionStatus::Latest;
    if (result_get.str_body == "outdated")
      kind_status = ModVersionStatus::Outdated;
    else if (result_get.str_body == "unknown")
      kind_status = ModVersionStatus::Unknown;
    else if (result_get.str_body == "playtest")
      kind_status = ModVersionStatus::PlaytestAvailable;

    const auto fn_assign = [this, kind_status]() {
      kind_status_.store(kind_status, std::memory_order_relaxed);
    };
    if (fn_run_after_tick_)
      fn_run_after_tick_(fn_assign);
    else
      fn_assign();
  }}.detach();
}

}

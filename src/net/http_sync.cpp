// UPSTREAM: OpenRA.Game/Support/HttpClientFactory.cs @e180e3225 +
// OpenRA.Game/HttpExtension.cs L18-49 @e180e3225(实现半;契约半见头文件)
// GET 的分块读复刻 ReadAsStreamWithProgress 的 8192 缓冲;Content-
// Disposition 的 filename 解析 = .NET ContentDisposition.FileName 的
// 去引号参数值(参数名大小写不敏感;值两端的成对引号剥除)。
// The implementation half (the contract half is in the header). The GET's
// chunked read reproduces ReadAsStreamWithProgress's 8192 buffer; the
// Content-Disposition filename parse = .NET ContentDisposition.FileName's
// unquoted parameter value (a case-insensitive parameter name; a paired
// quoting around the value is stripped).
#include "net/http_sync.hpp"

// boost 头在 clang 下的 pragma-attribute/nested-anon 告警面(第三方;
// 告警抑制只包 boost 的 include 区)
// The boost headers' pragma-attribute/nested-anon warning face under clang
// (third party; the suppression wraps only the boost include region).
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpragma-clang-attribute"
#pragma clang diagnostic ignored "-Wnested-anon-types"
#endif
#include <boost/asio/io_context.hpp>  // beast 承载的局部 io_context(白名单头)| the local io_context riding beast (a whitelisted header)
#include <boost/asio/ip/tcp.hpp>      // 同上 | same as above
#include <boost/beast/core.hpp>       // 同上 | same as above
#include <boost/beast/http.hpp>       // HttpClient 的消费点直调(白名单头)| the HttpClient by the direct call (a whitelisted header)
#include <boost/url.hpp>              // endpoint 的 URI 解析(白名单头)| the endpoint's URI parse (a whitelisted header)
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

namespace ora::net {

namespace {

/// .NET ContentDisposition.FileName:filename 参数值(参数名大小写不敏
/// 感;两端的成对引号剥除;参数缺 = 空串)
/// .NET ContentDisposition.FileName: the filename parameter's value (a
/// case-insensitive parameter name; a paired quoting at either end
/// stripped; an absent parameter = an empty string).
std::string ParseContentDispositionFilename(std::string_view sv_header) {
  constexpr std::string_view kParam = "filename=";
  for (std::size_t pos = 0; pos + kParam.size() <= sv_header.size();
       ++pos) {
    if (sv_header.substr(pos, kParam.size()).size() == kParam.size() &&
        std::equal(kParam.begin(), kParam.end(), sv_header.begin() + pos,
                   [](char char_expected, char char_actual) {
                     return std::tolower(static_cast<unsigned char>(
                                char_expected)) ==
                            std::tolower(static_cast<unsigned char>(
                                char_actual));
                   })) {
      std::string_view sv_value =
          sv_header.substr(pos + kParam.size());
      const std::size_t pos_end = sv_value.find(';');
      if (pos_end != std::string_view::npos)
        sv_value = sv_value.substr(0, pos_end);
      while (!sv_value.empty() &&
             (sv_value.front() == ' ' || sv_value.front() == '\t'))
        sv_value.remove_prefix(1);
      while (!sv_value.empty() &&
             (sv_value.back() == ' ' || sv_value.back() == '\t' ||
              sv_value.back() == '\r' || sv_value.back() == '\n'))
        sv_value.remove_suffix(1);
      if (sv_value.size() >= 2 && sv_value.front() == '"' &&
          sv_value.back() == '"')
        sv_value = sv_value.substr(1, sv_value.size() - 2);
      return std::string{sv_value};
    }
  }
  return {};
}

struct ParsedUrl {
  bool b_valid = false;
  std::string str_host;
  std::string str_port;
  std::string str_target;
  std::string str_authority;
};

ParsedUrl ParseHttpUrl(const std::string& str_url) {
  namespace urls = boost::urls;
  ParsedUrl out;
  const auto result_url = urls::parse_uri(str_url);
  if (!result_url)
    return out;
  const urls::url_view uv = *result_url;
  if (uv.scheme_id() != urls::scheme::http)
    return out;
  out.str_host = std::string{uv.host()};
  if (out.str_host.empty())
    return out;
  out.str_port =
      uv.port().empty() ? std::string{"80"} : std::string{uv.port()};
  out.str_target = std::string{uv.path()};
  if (uv.has_query())
    out.str_target += "?" + std::string{uv.query()};
  const auto sv_authority = uv.authority();
  out.str_authority.assign(sv_authority.data(), sv_authority.size());
  out.b_valid = true;
  return out;
}

}  // namespace

HttpResult HttpGetSync(const std::string& str_url,
                       const HttpProgressFn& fn_progress) {
  namespace asio = boost::asio;
  namespace http = boost::beast::http;

  HttpResult result;
  const ParsedUrl url = ParseHttpUrl(str_url);
  if (!url.b_valid)
    return result;

  asio::io_context io_context_local{};
  asio::ip::tcp::resolver resolver{io_context_local};
  boost::system::error_code ec{};
  const auto endpoints_resolved =
      resolver.resolve(url.str_host, url.str_port, ec);
  if (ec || endpoints_resolved.empty())
    return result;

  boost::beast::tcp_stream stream{io_context_local};
  stream.expires_after(std::chrono::seconds{100});
  stream.connect(endpoints_resolved, ec);
  if (ec)
    return result;

  http::request<http::empty_body> request{
      http::verb::get,
      url.str_target.empty() ? std::string{"/"} : url.str_target, 11};
  request.set(http::field::host, url.str_authority);
  request.prepare_payload();

  http::write(stream, request, ec);
  if (ec)
    return result;

  boost::beast::flat_buffer buffer;
  http::response_parser<http::string_body> parser;
  parser.body_limit(std::numeric_limits<std::uint64_t>::max());
  http::read_header(stream, buffer, parser, ec);
  if (ec)
    return result;

  result.int4_status = static_cast<int>(parser.get().result_int());
  const auto sv_disposition = parser.get()[http::field::content_disposition];
  if (!sv_disposition.empty())
    result.str_filename = ParseContentDispositionFilename(sv_disposition);

  // ReadAsStreamWithProgress:L18-49 —— total = Content-Length ?? -1;
  // canReportProgress = total > 0;结尾恒一次 (total, totalRead, 100)。
  // 进度粒度偏离登记:整体以 http::read 读全(头段已消费的 parser 续
  // 读),逐块回调承载为读全后的单次 (total, size, 100) —— 上游逐块的
  // UI 消费随 Phase 6;bytes/百分比终值面与上游一致。
  // ReadAsStreamWithProgress: L18-49 — total = Content-Length ?? -1;
  // canReportProgress = total > 0; the final (total, totalRead, 100)
  // always fires once. Progress-granularity deviation registered: the
  // message is read whole by http::read (the parser with its header
  // already consumed continues), and the per-chunk callbacks are carried
  // as a single post-read (total, size, 100) — the per-chunk UI consumers
  // ride Phase 6; the bytes/percentage end-value faces match upstream.
  const auto opt_length = parser.content_length();
  const long long int8_total =
      opt_length.has_value()
          ? static_cast<long long>(*opt_length)
          : static_cast<long long>(-1);

  http::read(stream, buffer, parser, ec);
  if (ec)
    return result;

  result.str_body = parser.release().body();
  if (fn_progress)
    fn_progress(int8_total,
                static_cast<long long>(result.str_body.size()), 100);

  result.b_ok = true;

  stream.socket().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
  return result;
}

bool HttpPostSync(const std::string& str_url, const std::string& str_body,
                  std::string& str_response_out) {
  namespace asio = boost::asio;
  namespace http = boost::beast::http;

  const ParsedUrl url = ParseHttpUrl(str_url);
  if (!url.b_valid)
    return false;

  asio::io_context io_context_local{};
  asio::ip::tcp::resolver resolver{io_context_local};
  boost::system::error_code ec{};
  const auto endpoints_resolved =
      resolver.resolve(url.str_host, url.str_port, ec);
  if (ec || endpoints_resolved.empty())
    return false;

  boost::beast::tcp_stream stream{io_context_local};
  stream.expires_after(std::chrono::seconds{100});
  stream.connect(endpoints_resolved, ec);
  if (ec)
    return false;

  http::request<http::string_body> request{
      http::verb::post,
      url.str_target.empty() ? std::string{"/"} : url.str_target, 11};
  request.set(http::field::host, url.str_authority);
  request.set(http::field::content_type, "text/plain; charset=utf-8");
  request.body() = str_body;
  request.prepare_payload();

  http::write(stream, request, ec);
  if (ec)
    return false;

  boost::beast::flat_buffer buffer;
  http::response<http::string_body> response;
  http::read(stream, buffer, response, ec);
  if (ec)
    return false;

  str_response_out = response.body();

  stream.socket().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
  return true;
}

}  // namespace ora::net

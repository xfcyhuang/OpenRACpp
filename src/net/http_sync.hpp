// UPSTREAM: OpenRA.Game/Support/HttpClientFactory.cs @1f594e320 +
// OpenRA.Game/HttpExtension.cs L18-49 + System.Net.Http.HttpClient 的
// GetAsync/PostAsync 语义面 @1f594e320(消费点直调的同步承载)
// 上游 HttpClient 的同步直调形态(boost::beast + boost::url;../cpp26.md
// §二 Boost 补位 —— HTTP 为 std 未落地面):HttpGetSync = GetAsync +
// HttpCompletionOption.ResponseHeadersRead + ReadAsStreamWithProgress(
// Content-Length>0 时逐块回调,结尾恒一次 (total, totalRead, 100));POST =
// PostAsync + ReadAsStringAsync。两形均为纯同步 —— 调用方(引擎侧)在专属
// 线程调用以保持上游 await 的非阻塞面。URI 非法/https(纯同步 beast 无
// TLS 后端)/DNS/连接/读写任一失败 = 传输层失败;状态码不参与成败判断
// (上游无 EnsureSuccessStatusCode 的面)。
// The synchronous direct-call form of the upstream HttpClient (boost::beast
// + boost::url; ../cpp26.md §2's Boost fill-in — HTTP is a face std has
// not landed): HttpGetSync = GetAsync +
// HttpCompletionOption.ResponseHeadersRead + ReadAsStreamWithProgress
// (per-chunk callbacks when Content-Length > 0, plus one final
// (total, totalRead, 100)); the POST = PostAsync + ReadAsStringAsync. Both
// are purely synchronous — the callers (engine-side) invoke on a dedicated
// thread to keep upstream's non-blocking await face. An invalid URI / https
// (plain synchronous beast carries no TLS backend) / any DNS/connect/
// read/write failure = a transport failure; the status code takes no part
// in success (upstream has no EnsureSuccessStatusCode on this face).
#pragma once
import std;

namespace ora::net {

/// HttpExtension.OnProgress:total(无 Content-Length = -1)/累计已读/百分比
/// HttpExtension.OnProgress: the total (-1 without Content-Length)/the
/// bytes read so far/the percentage.
using HttpProgressFn = std::function<void(long long, long long, int)>;

/// GetAsync 的结果:传输层成败 + 状态码 + body + Content-Disposition 的
/// filename 参数(.NET ContentDisposition.FileName 的去引号值;头缺 = 空)
/// The GetAsync result: the transport outcome + the status + the body +
/// the Content-Disposition filename parameter (.NET ContentDisposition.
/// FileName's unquoted value; an absent header = empty).
struct HttpResult {
  bool b_ok = false;
  int int4_status = 0;
  std::string str_body;
  std::string str_filename;
};

/// GET + 进度读(HttpExtension.ReadAsStreamWithProgress 语义)
/// GET + the progress read (HttpExtension.ReadAsStreamWithProgress
/// semantics).
HttpResult HttpGetSync(const std::string& str_url,
                       const HttpProgressFn& fn_progress = nullptr);

/// POST(StringContent 单参默认媒体型;任意状态码读 body;.NET HttpClient
/// 默认 100s 超时)
/// POST (the single-argument StringContent's default media type; any
/// status code still reads the body; .NET HttpClient's default 100s
/// timeout).
bool HttpPostSync(const std::string& str_url, const std::string& str_body,
                  std::string& str_response_out);

}  // namespace ora::net

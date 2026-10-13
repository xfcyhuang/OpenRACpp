# Boost 1.92(闭包子集)

本目录是工程实际引用的 Boost 1.92.0(b2-nodocs 源码树)的**精确闭包**,
于 2026-10-13 以 `clang++ -M` 对全部消费翻译单元(sha1_160/regex/beast/
asio/url 的直接与传递 include)枚举生成,共 1256 个头文件 + Boost.URL
的 19 个编译态 src(及其伴生 hpp)。上游完整源码树不在本仓库;闭包外的
Boost 组件若被后续批次引用,须从完整树补拷并重跑闭包校验。

组件:boost::hash2(SHA-1 承载,D234)/ boost::regex(正则承载,D235)/
boost::beast + boost::asio + boost::url(HTTP 同步承载,D236/D237)。

许可证:Boost Software License 1.0(见 LICENSE_1_0.txt)。

This directory is the exact closure of Boost 1.92.0 (the b2-nodocs source
tree) actually referenced by the project, enumerated on 2026-10-13 with
`clang++ -M` over every consuming translation unit (the direct and
transitive includes of sha1_160/regex/beast/asio/url): 1256 headers plus
Boost.URL's 19 compiled src files (with their companion hpps). The full
upstream source tree lives outside this repository; a later batch pulling
in Boost components beyond this closure must copy them in from the full
tree and re-run the closure check.

Components: boost::hash2 (the SHA-1 carrier, D234) / boost::regex (the
regex carrier, D235) / boost::beast + boost::asio + boost::url (the
synchronous-HTTP carrier, D236/D237).

License: Boost Software License 1.0 (see LICENSE_1_0.txt).

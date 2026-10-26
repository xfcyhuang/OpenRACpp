// UPSTREAM: OpenRA.Game/Settings.cs @1f594e320 L500-538(SanitizedName/
//          SanitizedPlayerName 全文;上游 Settings.cs 静态 —— C++ 依
//          lib 层属(client/server 双消费)置于 net;SanitizedServerName
//          仍随 server_settings)
//          The whole of Settings.cs L500-538 (SanitizedName/
//          SanitizedPlayerName; upstream Settings.cs statics — placed in
//          net per the C++ lib layering (both the client and the server
//          consume them); SanitizedServerName stays with server_settings).
#pragma once
import std;

namespace ora::net {

/// SanitizedName(L500-513):MiniYAML/JSON 保留字符剥离;空入 = 空串
/// SanitizedName (L500-513): the MiniYAML/JSON reserved-character strip;
/// empty in = empty out.
std::string SanitizedName(std::string_view str_dirty);

/// SanitizedPlayerName(L524-538):禁名(Open/Closed)与空白回落默认名 +
/// 16 字符(码点位)截断
/// SanitizedPlayerName (L524-538): the forbidden names (Open/Closed) and
/// the whitespace fallback to the default name + the 16-character (code-
/// point position) truncation.
std::string SanitizedPlayerName(std::string_view str_dirty,
                                std::string_view str_default_name);

}  // namespace ora::net

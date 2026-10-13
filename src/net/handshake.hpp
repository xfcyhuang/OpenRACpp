// UPSTREAM: OpenRA.Game/Network/Handshake.cs @e180e3225 L17-90(握手请求/
//          响应的 yaml 线格式:Handshake 节按名装载,Response 另挂 Client
//          节 = FieldSaver.Save(Session.Client);Serialize 的显式字段序
//          Mod/Version/Password/Fingerprint/AuthSignature/OrdersProtocol
//          逐字)
//          Handshake.cs L17-90 (the yaml wire format of the handshake
//          request/response: the Handshake node loads by name, the Response
//          additionally carries the Client node = FieldSaver.Save(
//          Session.Client); Serialize's explicit field order Mod/Version/
//          Password/Fingerprint/AuthSignature/OrdersProtocol verbatim).
#pragma once
import std;

#include "net/session.hpp"


namespace ora::net {

/// HandshakeRequest(Handshake.cs L17-35)
/// HandshakeRequest (Handshake.cs L17-35).
struct HandshakeRequest {
  std::string Mod;
  std::string Version;
  std::string AuthToken;  // null → "" | null as ""

  /// Deserialize(L23-28):首节点的 Handshake 值按名装载;失败 = nullopt
  /// Deserialize (L23-28): the first node's value loads by name; a parse
  /// failure yields nullopt.
  static std::optional<HandshakeRequest> Deserialize(
      std::string_view str_data, std::string_view str_name);

  /// Serialize(L30-34):[("Handshake", FieldSaver.Save)] 的 WriteToString
  /// Serialize (L30-34): WriteToString of [("Handshake", FieldSaver.Save)].
  std::string Serialize() const;
};

/// HandshakeResponse(Handshake.cs L37-89);OrdersProtocol 缺省 7 =
/// 上游"新服务器可探测旧客户端"注释语义
/// HandshakeResponse (Handshake.cs L37-89); OrdersProtocol defaults to 7 —
/// upstream's "newer servers can detect older clients" note.
struct HandshakeResponse {
  std::string Mod;
  std::string Version;
  std::string Password;
  int OrdersProtocol = 7;

  std::string Fingerprint;
  std::string AuthSignature;

  SessionClient Client{};

  /// Deserialize(L54-76):Handshake/Client 双节按名装载;失败 = nullopt
  /// Deserialize (L54-76): the Handshake/Client nodes load by name; a
  /// parse failure yields nullopt.
  static std::optional<HandshakeResponse> Deserialize(
      std::string_view str_data, std::string_view str_name);

  /// Serialize(L78-88):显式字段序的 Handshake 节 + Client 节
  /// Serialize (L78-88): the explicit-field-order Handshake node + the
  /// Client node.
  std::string Serialize() const;
};

}  // namespace ora::net

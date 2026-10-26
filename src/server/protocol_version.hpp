// UPSTREAM: OpenRA.Game/Server/ProtocolVersion.cs @1f594e320 L73-81(握手
//          协议版本 = 7(同 net/network_connection.hpp 的
//          kHandshakeProtocolVersion,双锚点合一处)+ 订单协议版本 = 21;
//          常量语义照抄,注释块描述的线格式见各实现文件的字段序)
//          ProtocolVersion.cs L73-81 (the handshake protocol version = 7,
//          sharing net/network_connection.hpp's kHandshakeProtocolVersion,
//          now unified here, + the orders protocol version = 21).
#pragma once

namespace ora::server {

/// ProtocolVersion.Handshake(L75;握手协议版本;上游注释:不兼容变更会
/// 破坏运行时 mod 切换,最后手段才改)
/// ProtocolVersion.Handshake (L75; upstream note: incompatible changes
/// break runtime mod switching, change only as a last resort).
inline constexpr int kHandshakeProtocol = 7;

/// ProtocolVersion.Orders(L80;握手完成后的服务器/世界订单协议版本)
/// ProtocolVersion.Orders (L80; the server/world orders protocol version
/// applied after the handshake completes).
inline constexpr int kOrdersProtocol = 21;

}  // namespace ora::server

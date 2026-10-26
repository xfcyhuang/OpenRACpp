// UPSTREAM: OpenRA.Game/Network/UnitOrders.cs @1f594e320 L48-431(第十六批
//          客户端接线:ProcessOrder 全分支 —— Message/FluentMessage/
//          DisableChatEntry/StartKickVote/EndKickVote/Chat/StartGame/
//          SaveTraitData/GameSaved/PauseGame/HandshakeRequest/ServerError/
//          AuthenticationError/SyncInfo/SyncLobbyClients/SyncLobbySlots/
//          SyncLobbyGlobalSettings/SyncConnectionQuality/SyncMapPool/
//          GenerateMap + default→ResolveOrder;TextNotificationsManager 全
//          承载;Game/Settings/ModData/MapCache 的静态直调面以注入族承载)
//          The batch-16 client wiring: ProcessOrder's every branch —
//          Message/FluentMessage/DisableChatEntry/StartKickVote/EndKickVote/
//          Chat/StartGame/SaveTraitData/GameSaved/PauseGame/HandshakeRequest/
//          ServerError/AuthenticationError/SyncInfo/SyncLobbyClients/
//          SyncLobbySlots/SyncLobbyGlobalSettings/SyncConnectionQuality/
//          SyncMapPool/GenerateMap + default→ResolveOrder; the whole
//          TextNotificationsManager carriage; the Game/Settings/ModData/
//          MapCache static calls ride the injection family.
#pragma once
import std;

#include "core/color.hpp"
#include "net/order.hpp"

namespace ora::map {
class MapPreview;
}  // namespace ora::map

namespace ora::net {

class OrderManager;

/// UnitOrders.ChatMessageMaxLength(L50)
/// UnitOrders.ChatMessageMaxLength (L50).
inline constexpr int kChatMessageMaxLength = 2500;

/// HandshakeRequest 分支的客户端装配面(上游 Game.ModData.Manifest /
/// Game.Settings.Player / CurrentServerSettings.Password / Game.
/// LocalPlayerProfile 四静态面的合一注入;fingerprint 空 = 上游 null)
/// The HandshakeRequest branch's client assembly face (the single-point
/// injection of upstream's four statics — Game.ModData.Manifest /
/// Game.Settings.Player / CurrentServerSettings.Password /
/// Game.LocalPlayerProfile; an empty fingerprint = upstream's null).
struct ClientJoinFace {
  std::string str_mod_id;
  std::string str_mod_version;
  std::string str_player_name;
  core::Color color_player{};
  std::string str_password;
  std::string str_fingerprint;

  /// profile.Sign(authToken);null = 上游 Fingerprint==null 的跳过路径
  /// profile.Sign(authToken); null = upstream's skip path when the
  /// Fingerprint is null.
  std::function<std::string(std::string_view)> fn_sign;
};

/// UnitOrders.ProcessOrder(L48-415)
void ProcessOrder(OrderManager& order_manager, sim::World* world,
                  int client_id, const Order& order);

/// UnitOrders.KickVoteTarget(L48-49)+ Clear(L426-430)
/// UnitOrders.KickVoteTarget (L48-49) + Clear (L426-430).
std::optional<int> KickVoteTarget();
void SetKickVoteTarget(std::optional<int> int4_target);
void ClearUnitOrdersStatics();

/// StartGame 分支的 Game.StartGame 注入面(上游直接调 Game 静态;C++ 分层
/// 下由 Game 装配注册 —— 缺省静默,order 流继续)
/// The StartGame branch's Game.StartGame injection face (upstream calls
/// the Game static directly; under C++ layering the Game assembly
/// registers it — silent by default, the order stream continues).
void SetStartGameHandler(
    std::function<void(OrderManager&, const std::string&)> fn_start_game);
void ClearStartGameHandler();

/// Game.SyncLobbyInfo(LobbyInfoChanged 事件)注入面(缺省 no-op)
/// The Game.SyncLobbyInfo injection face (the LobbyInfoChanged event;
/// default no-op).
void SetLobbyInfoChangedHandler(std::function<void()> fn_changed);
void ClearLobbyInfoChangedHandler();

/// Game.RunTime 注入面(DisableChatEntry 的 ChatDisabledUntil 算术用;
/// 缺省恒 0)
/// The Game.RunTime injection face (for DisableChatEntry's
/// ChatDisabledUntil arithmetic; default 0).
void SetRuntimeMsFn(std::function<std::int64_t()> fn_runtime_ms);

/// HandshakeRequest 分支的客户端装配面(缺省空 = 未装配;空面下该分支
/// 早退 —— 上游 Game.ModData 缺席即不可达)
/// The HandshakeRequest branch's client assembly face (default empty =
/// unassembled; under the empty face the branch early-outs — unreachable
/// upstream without Game.ModData).
void SetClientJoinFace(ClientJoinFace face_join);
const ClientJoinFace& ClientJoinFaceOf();

/// GenerateMap 分支的 MapCache 注入面(uid → preview;缺省 null = 分支
/// 早退)
/// The GenerateMap branch's MapCache injection face (uid → preview;
/// default null = the branch early-outs).
void SetMapCacheAtFn(std::function<ora::map::MapPreview*(
                         const std::string& str_uid)> fn_at);

}  // namespace ora::net

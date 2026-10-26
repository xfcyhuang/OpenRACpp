// UPSTREAM: OpenRA.Game/Server/TraitInterfaces.cs @1f594e320 L14-62(九个
//          服务器 trait 接口 + ServerTrait 基类 + DebugServerTrait;接口
//          分发面 = 服务器事件循环的 WithInterface<T> 查询 —— C++ 侧以
//          注册期 dynamic_cast 装配的按接口向量承载,LobbyCommands 批
//          接首个真实现)
//          TraitInterfaces.cs L14-62 (the nine server-trait interfaces +
//          the ServerTrait base + DebugServerTrait; the interface
//          dispatch face = the event loop's WithInterface<T> queries —
//          carried C++-side as per-interface vectors assembled by
//          registration-time dynamic_cast, with the LobbyCommands batch
//          supplying the first real implementation).
#pragma once
import std;

#include "server/game_information.hpp"
#include "server/server_settings.hpp"


namespace ora::server {

class Server;
class ServerConnection;

/// ServerTrait(TraitInterfaces.cs L28)
/// ServerTrait (TraitInterfaces.cs L28).
class ServerTrait {
 public:
  virtual ~ServerTrait() = default;
};

/// IInterpretCommand(TraitInterfaces.cs L18;返回 true = 已处理)
/// IInterpretCommand (TraitInterfaces.cs L18; true = handled).
class IInterpretCommand {
 public:
  virtual ~IInterpretCommand() = default;
  virtual bool InterpretCommand(Server& server, ServerConnection& conn,
                                net::SessionClient* p_client,
                                std::string_view str_cmd) = 0;
};

/// INotifySyncLobbyInfo(TraitInterfaces.cs L19)
class INotifySyncLobbyInfo {
 public:
  virtual ~INotifySyncLobbyInfo() = default;
  virtual void LobbyInfoSynced(Server& server) = 0;
};

/// INotifyServerStart(TraitInterfaces.cs L20)
class INotifyServerStart {
 public:
  virtual ~INotifyServerStart() = default;
  virtual void ServerStarted(Server& server) = 0;
};

/// INotifyServerEmpty(TraitInterfaces.cs L21)
class INotifyServerEmpty {
 public:
  virtual ~INotifyServerEmpty() = default;
  virtual void ServerEmpty(Server& server) = 0;
};

/// INotifyServerShutdown(TraitInterfaces.cs L22)
class INotifyServerShutdown {
 public:
  virtual ~INotifyServerShutdown() = default;
  virtual void ServerShutdown(Server& server) = 0;
};

/// IStartGame(TraitInterfaces.cs L23)
class IStartGame {
 public:
  virtual ~IStartGame() = default;
  virtual void GameStarted(Server& server) = 0;
};

/// IClientJoined(TraitInterfaces.cs L24)
class IClientJoined {
 public:
  virtual ~IClientJoined() = default;
  virtual void ClientJoined(Server& server, ServerConnection& conn) = 0;
};

/// IEndGame(TraitInterfaces.cs L25)
class IEndGame {
 public:
  virtual ~IEndGame() = default;
  virtual void GameEnded(Server& server) = 0;
};

/// ITick(TraitInterfaces.cs L26)
class ITick {
 public:
  virtual ~ITick() = default;
  virtual void Tick(Server& server) = 0;
};

/// DebugServerTrait(TraitInterfaces.cs L30-62;命令回显 + 六事件控制台
/// 打印 —— 服务器诊断 trait 的参照实现)
/// DebugServerTrait (TraitInterfaces.cs L30-62; the command echo + the
/// six-event console prints — the reference diagnostic server trait).
class DebugServerTrait final : public ServerTrait,
                               public IInterpretCommand,
                               public IStartGame,
                               public INotifySyncLobbyInfo,
                               public INotifyServerStart,
                               public INotifyServerShutdown,
                               public IEndGame {
 public:
  bool InterpretCommand(Server& server, ServerConnection& conn,
                        net::SessionClient* p_client,
                        std::string_view str_cmd) override;

  void GameStarted(Server& server) override;
  void LobbyInfoSynced(Server& server) override;
  void ServerStarted(Server& server) override;
  void ServerShutdown(Server& server) override;
  void GameEnded(Server& server) override;
};

}  // namespace ora::server

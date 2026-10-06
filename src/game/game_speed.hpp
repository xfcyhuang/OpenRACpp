// UPSTREAM: OpenRA.Game/GameSpeed.cs @b6fc03f L17-62(逐语义重写)
//          Verbatim-semantics rewrite.
//
// 机制对照 / Mechanism mapping:
//  - GameSpeed 的 [FieldLoader.Require] → 缺字段 MissingFieldsException 等价
//    (LoadSpeeds 的包装异常文本逐字)
//    The [FieldLoader.Require] of GameSpeed → the MissingFieldsException
//    equivalent (LoadSpeeds' wrapper exception text verbatim).
#pragma once
import std;

#include "yaml/mini_yaml.hpp"

namespace ora::game {

/// GameSpeed(GameSpeed.cs L17-28)
/// GameSpeed (GameSpeed.cs L17-28).
class GameSpeed final {
 public:
  /// FieldLoader.Load<GameSpeed>(yaml):Name/Timestep/OrderLatency 全 Required
  std::string Name;         // L21
  std::int32_t Timestep{0}; // L24
  std::int32_t OrderLatency{0};  // L27
};

/// GameSpeeds(GameSpeed.cs L30-61):IGlobalModData 等价
/// GameSpeeds (GameSpeed.cs L30-61): the IGlobalModData equivalent.
class GameSpeeds final {
 public:
  GameSpeeds() = default;

  /// mod.yaml "GameSpeeds" 节点构造(LoadSpeeds 语义)
  explicit GameSpeeds(const yaml::MiniYaml& y);

  std::string DefaultSpeed;  // L33
  std::vector<std::pair<std::string, GameSpeed>> Speeds{};  // 插入序字典

  /// Speeds[name](L196 的索引面;缺失 KeyNotFoundException 等价)
  const GameSpeed* Find(std::string_view str_name) const;
};

}  // namespace ora::game

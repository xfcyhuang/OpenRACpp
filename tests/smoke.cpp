import std;

#include "game/game.hpp"
#include "mods/common/common.hpp"
#include "mods/cnc/cnc.hpp"
#include "mods/d2k/d2k.hpp"
#include "mods/ra/ra.hpp"
#include "platforms/platforms.hpp"
#include "server/server.hpp"
#include "utility/utility.hpp"
#include "test_util.hpp"
using ora::test::check;

namespace {

constexpr int kExpectedGameTag{1};
constexpr int kExpectedCommonTag{2};
constexpr int kExpectedCncTag{3};
constexpr int kExpectedD2kTag{4};
constexpr int kExpectedRaTag{5};
constexpr int kExpectedPlatformsTag{6};
constexpr int kExpectedServerTag{7};
constexpr int kExpectedUtilityTag{8};

static_assert(sizeof(int) == 4, "int4 类型前缀约定依赖 4 字节 int");

}

int main() {
  check(ora::game::game_tag() == kExpectedGameTag, "ora::game::game_tag");
  check(ora::game::game_name() == "OpenRA.Game", "ora::game::game_name");
  check(ora::mods::common::common_tag() == kExpectedCommonTag, "ora::mods::common::common_tag");
  check(ora::mods::common::common_name() == "OpenRA.Mods.Common", "ora::mods::common::common_name");
  check(ora::mods::cnc::cnc_tag() == kExpectedCncTag, "ora::mods::cnc::cnc_tag");
  check(ora::mods::cnc::cnc_name() == "OpenRA.Mods.Cnc", "ora::mods::cnc::cnc_name");
  check(ora::mods::d2k::d2k_tag() == kExpectedD2kTag, "ora::mods::d2k::d2k_tag");
  check(ora::mods::d2k::d2k_name() == "OpenRA.Mods.D2k", "ora::mods::d2k::d2k_name");
  check(ora::mods::ra::ra_tag() == kExpectedRaTag, "ora::mods::ra::ra_tag");
  check(ora::mods::ra::ra_name() == "mods/ra", "ora::mods::ra::ra_name");
  check(ora::platforms::platforms_tag() == kExpectedPlatformsTag, "ora::platforms::platforms_tag");
  check(ora::platforms::platforms_name() == "OpenRA.Platforms.Default", "ora::platforms::platforms_name");
  check(ora::server::server_tag() == kExpectedServerTag, "ora::server::server_tag");
  check(ora::server::server_name() == "OpenRA.Server", "ora::server::server_name");
  check(ora::utility::utility_tag() == kExpectedUtilityTag, "ora::utility::utility_tag");
  check(ora::utility::utility_name() == "OpenRA.Utility", "ora::utility::utility_name");

  return ora::test::test_exit_code("smoke");
}

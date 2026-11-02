import std;

#include "game/game.hpp"
#include "mods/common/common.hpp"
#include "mods/cnc/cnc.hpp"
#include "mods/d2k/d2k.hpp"
#include "mods/ra/ra.hpp"
#include "platforms/platforms.hpp"
#include "server/server.hpp"
#include "utility/utility.hpp"

namespace {

constexpr int kExpectedGameTag{1};
constexpr int kExpectedCommonTag{2};
constexpr int kExpectedCncTag{3};
constexpr int kExpectedD2kTag{4};
constexpr int kExpectedRaTag{5};
constexpr int kExpectedPlatformsTag{6};
constexpr int kExpectedServerTag{7};
constexpr int kExpectedUtilityTag{8};
constexpr int kCheckCount{16};

static_assert(sizeof(int) == 4, "int4 类型前缀约定依赖 4 字节 int");

void check(bool b_ok, std::string_view str_what, int& int4_failures) {
  if (!b_ok) {
    ++int4_failures;
    std::println("smoke: check failed: {}", str_what);
  }
}

}

int main() {
  int int4_failures{0};

  check(ora::game::game_tag() == kExpectedGameTag, "ora::game::game_tag", int4_failures);
  check(ora::game::game_name() == "OpenRA.Game", "ora::game::game_name", int4_failures);
  check(ora::mods::common::common_tag() == kExpectedCommonTag, "ora::mods::common::common_tag", int4_failures);
  check(ora::mods::common::common_name() == "OpenRA.Mods.Common", "ora::mods::common::common_name", int4_failures);
  check(ora::mods::cnc::cnc_tag() == kExpectedCncTag, "ora::mods::cnc::cnc_tag", int4_failures);
  check(ora::mods::cnc::cnc_name() == "OpenRA.Mods.Cnc", "ora::mods::cnc::cnc_name", int4_failures);
  check(ora::mods::d2k::d2k_tag() == kExpectedD2kTag, "ora::mods::d2k::d2k_tag", int4_failures);
  check(ora::mods::d2k::d2k_name() == "OpenRA.Mods.D2k", "ora::mods::d2k::d2k_name", int4_failures);
  check(ora::mods::ra::ra_tag() == kExpectedRaTag, "ora::mods::ra::ra_tag", int4_failures);
  check(ora::mods::ra::ra_name() == "mods/ra", "ora::mods::ra::ra_name", int4_failures);
  check(ora::platforms::platforms_tag() == kExpectedPlatformsTag, "ora::platforms::platforms_tag", int4_failures);
  check(ora::platforms::platforms_name() == "OpenRA.Platforms.Default", "ora::platforms::platforms_name", int4_failures);
  check(ora::server::server_tag() == kExpectedServerTag, "ora::server::server_tag", int4_failures);
  check(ora::server::server_name() == "OpenRA.Server", "ora::server::server_name", int4_failures);
  check(ora::utility::utility_tag() == kExpectedUtilityTag, "ora::utility::utility_tag", int4_failures);
  check(ora::utility::utility_name() == "OpenRA.Utility", "ora::utility::utility_name", int4_failures);

  if (int4_failures != 0) {
    std::println("smoke: {}/{} checks failed", int4_failures, kCheckCount);
    return 1;
  }
  std::println("smoke: all {} checks passed", kCheckCount);
  return 0;
}

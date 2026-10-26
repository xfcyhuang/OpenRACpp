// UPSTREAM: Phase 5 第八批验收:RenderSprites/WithSpriteBody(+Facing)/
//          WithInfantryBody/WithSpriteTurret/WithMakeAnimation(+Overlay)+
//          ProximityCapturable 族 + FieldSaver/Settings + ReplayRecorder/
//          ReplayConnection/SyncReport 起步(真 ra 全链:真序列集装配 →
//          e1 动画状态机 → 2tnk 炮塔面 → TENT make 动画 → 邻近捕获换主)
//          The Phase 5 batch-8 acceptance: the RenderSprites/WithSpriteBody
//          family + WithInfantryBody/WithSpriteTurret/WithMakeAnimation
//          (+Overlay) + the ProximityCapturable family + FieldSaver/
//          Settings + the ReplayRecorder/ReplayConnection/SyncReport
//          start (the real ra full chain: the real sequence-set assembly →
//          e1's animation state machine → the 2tnk turret face → TENT's
//          make animation → the proximity capture's owner change).
import std;

#include <cstdio>

#include "game/game.hpp"
#include "gfx/sprite_font.hpp"
#include "gfx/chrome_provider.hpp"
#include "gfx/sheet.hpp"
#include "mods/music_playlist.hpp"
#include "mods/lobby_logics.hpp"
#include "mods/settings_logics.hpp"
#include "ui/widgets/hotkey_entry_widget.hpp"
#include "mods/lobby_utils.hpp"
#include "mods/server_list_logic.hpp"
#include "mods/asset_browser_logic.hpp"
#include "mods/game_info_logics.hpp"
#include "mods/server_creation_logic.hpp"
#include "net/nat.hpp"
#include "mods/video_player_widget.hpp"
#include "platform/ifont.hpp"
#include "ui/ui.hpp"
#include "ui/widgets/checkbox_widget.hpp"
#include "ui/widgets/color_block_widget.hpp"
#include "ui/widgets/drop_down_button_widget.hpp"
#include "ui/widgets/image_widget.hpp"
#include "ui/widgets/label_widget.hpp"
#include "ui/widgets/progress_bar_widget.hpp"
#include "ui/widgets/client_tooltip_region_widget.hpp"
#include "ui/widgets/logic_ticker_widget.hpp"
#include "ui/widgets/map_preview_widget.hpp"
#include "ui/widgets/password_field_widget.hpp"
#include "ui/widgets/scroll_item_widget.hpp"
#include "ui/widgets/scroll_panel_widget.hpp"
#include "ui/widgets/slider_widget.hpp"
#include "ui/widgets/text_field_widget.hpp"
#include "gfx/actor_preview.hpp"
#include "ui/widget.hpp"
#include "ui/widgets/actor_preview_widget.hpp"
#include "game/manifest.hpp"
#include "game/mod_data.hpp"
#include "game/settings.hpp"
#include "render_sequences_fixture.hpp"
#include "gfx/palette.hpp"
#include "gfx/renderable.hpp"
#include "gfx/sequence_set.hpp"
#include "gfx/sprite_loader.hpp"
#include "gfx/viewport.hpp"
#include "gfx/world_renderer.hpp"
#include "map/map.hpp"
#include "map/map_cache.hpp"
#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "sim/trait_registry.hpp"
#include "meta/field_saver.hpp"
#include "mods/body_orientation.hpp"
#include "mods/create_map_players.hpp"
#include "mods/mobile.hpp"
#include "mods/production_bar.hpp"
#include "mods/proximity_capturable.hpp"
#include "mods/render_sprites.hpp"
#include "mods/selection_decorations.hpp"
#include "mods/sequence_loader_factory.hpp"
#include "mods/sprite_effect.hpp"
#include "mods/turreted.hpp"
#include "mods/unit_order_generator.hpp"
#include "mods/viewport_controller.hpp"
#include "mods/with_damage_overlay.hpp"
#include "mods/with_death_animation.hpp"
#include "mods/with_decoration.hpp"
#include "mods/with_decoration_pips.hpp"
#include "mods/with_infantry_body.hpp"
#include "mods/with_make_animation.hpp"
#include "mods/with_sprite_body.hpp"
#include "mods/with_sprite_turret.hpp"
#include "mods/building.hpp"
#include "mods/building_utils.hpp"
#include "mods/line_build.hpp"
#include "mods/mission_objectives.hpp"
#include "mods/order_generators.hpp"
#include "mods/repairable_building.hpp"
#include "mods/sellable.hpp"
#include "mods/place_building.hpp"
#include "mods/aircraft.hpp"
#include "mods/air_activities.hpp"
#include "mods/air_support.hpp"
#include "mods/attack_aircraft.hpp"
#include "mods/airstrike_power.hpp"
#include "mods/cargo.hpp"
#include "mods/carryall.hpp"
#include "mods/cargo_activities.hpp"
#include "mods/parachutable.hpp"
#include "mods/passenger.hpp"
#include "mods/paratroopers_power.hpp"
#include "mods/with_parachute.hpp"
#include "mods/primary_building.hpp"
#include "mods/player_resources.hpp"
#include "mods/production_icon_overlay.hpp"
#include "mods/ingame_hud_logics.hpp"
#include "mods/power.hpp"
#include "mods/production_support.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/color_block_widget.hpp"
#include "ui/widgets/image_widget.hpp"
#include "ui/widgets/label_widget.hpp"
#include "ui/widgets/label_with_tooltip_widget.hpp"
#include "ui/widgets/logic_ticker_widget.hpp"
#include "ui/widgets/resource_bar_widget.hpp"
#include "mods/production_palette_widget.hpp"
#include "mods/production_tabs_widget.hpp"
#include "mods/production_type_button_widget.hpp"
#include "mods/world_interaction_controller.hpp"
#include "mods/radar.hpp"
#include "mods/radar_widget.hpp"
#include "mods/support_powers.hpp"
#include "mods/support_power_effects.hpp"
#include "mods/concrete_support_powers.hpp"
#include "mods/ingame_menu_logic.hpp"
#include "mods/menu_logics.hpp"
#include "mods/support_powers_widget.hpp"
#include "ui/widgets/menu_button_widget.hpp"
#include "mods/player_statistics.hpp"
#include "mods/tooltip.hpp"
#include "net/order.hpp"
#include "net/order_io.hpp"
#include "net/connection_target.hpp"
#include "net/replay_recorder.hpp"
#include "net/sync_report.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/player.hpp"
#include "sim/selectable_exts.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/world.hpp"
#include "ui/selection_utils.hpp"
#include "ui/ui.hpp"
#include "mods/browser_logics.hpp"
#include "net/game_save.hpp"
#include "server/game_information.hpp"

namespace {
std::string_view TestMapFilter() {
  const char* str_env = std::getenv("ORA_TEST_MAP_FILTER");
  if (str_env != nullptr)
    return std::string_view{str_env};
  return "Siberian-Pass";
}
}  // namespace


namespace ora::fmt {
std::vector<std::byte> LcwEncodeForRenderTest(std::vector<std::byte> vec_frame);
}

namespace ora::gen {
void RegisterGeneratedAll();
}


static int g_failures = 0;

template <class T>
void CheckEq(const T& a, const T& b, std::string_view what) {
  if (!(a == b)) {
    std::println("FAIL: {} ({} != {})", what, a, b);
    std::fflush(stdout);
    ++g_failures;
  }
}

void Check(bool ok, std::string_view what) {
  if (!ok) {
    std::println("FAIL: {}", what);
    std::fflush(stdout);
    ++g_failures;
  }
}

class TestPower final : public ora::mods::SupportPower {
 public:
  TestPower(const ora::sim::ActorInitializer& init,
            ora::mods::SupportPowerInfoData info_data)
      : ora::mods::SupportPower(init, std::move(info_data)) {}

  static constexpr ora::gen::TypeId kTypeId =
      ora::gen::TypeId::OpenRA_Mods_Common_Traits_DirectionalSupportPower;
  ora::gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<TestPower, ora::mods::SupportPower>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts()
      const override {
    return kTraitUpcasts;
  }
};

class RecordingViewportStub final : public ora::gfx::IViewportSurface {
 public:
  ora::int2 WorldToViewPx(ora::int2 int2_world) override { return int2_world; }
  ora::int2 WorldToViewPx(const ora::core::Vector3& vec_world) override {
    return ora::int2{static_cast<std::int32_t>(vec_world.X),
                     static_cast<std::int32_t>(vec_world.Y)};
  }
  ora::Rectangle GetScissorBounds(bool) override { return ora::Rectangle{}; }
  ora::int2 TopLeft() override { return ora::int2{}; }
  ora::int2 BottomRight() override { return ora::int2{}; }
  ora::CPos ViewToWorld(ora::int2 int2_view) override {
    return ora::CPos{int2_view.X / 1024, int2_view.Y / 1024};
  }
  ora::int2 ViewToWorldPx(ora::int2 int2_view) override { return int2_view; }
  ora::WPos CenterPosition() const override { return wpos_center; }
  void Center(const ora::WPos& wpos_pos) override {
    b_center_called = true;
    wpos_last_center = wpos_pos;
  }
  void Scroll(ora::core::Vector2 vec_delta, bool) override {
    b_scroll_called = true;
    vec_last_scroll = vec_delta;
  }
  void AdjustZoom(float fp4_dz) override {
    b_zoom_called = true;
    fp4_last_zoom_delta = fp4_dz;
  }
  void AdjustZoom(float fp4_dz, ora::int2) override {
    b_zoom_anchored_called = true;
    fp4_last_zoom_delta = fp4_dz;
  }
  ora::gfx::ScrollDirection GetBlockedDirections() const override {
    return kind_blocked;
  }

  ora::WPos wpos_center{};
  ora::WPos wpos_last_center{};
  ora::core::Vector2 vec_last_scroll{};
  float fp4_last_zoom_delta = 0.0f;
  ora::gfx::ScrollDirection kind_blocked = ora::gfx::ScrollDirection::None;
  bool b_center_called = false;
  bool b_scroll_called = false;
  bool b_zoom_called = false;
  bool b_zoom_anchored_called = false;
};

int main(int argc, char** argv) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 2) {
    std::println("usage: render_test <upstream-root>");
    return 1;
  }

  ora::gen::RegisterGeneratedAll();
  ora::game::RegisterGameLoaders();
  ora::sim::RegisterWorldTraits();
  ora::mods::RegisterCommonTraits();

  // qboi facings 解析钩子(测试接线 8;同 attack/condition 批的注入面)
  // The qboi facings-resolution hook (the test wires 8; attack/condition
  // batches' injection face).
  ora::mods::SetBodyOrientationFacingsResolver(
      [](const ora::game::ActorInfo&, const std::string&) { return 8; });

  const char* str_upstream_root = argv[1];
  std::println("== render_test ==");

  using namespace ora;
  using ora::testfx::InstallSyntheticSequences;

  // ———— 装配:ModData + 真 ra 序列集 ————
  // ———— Assembly: ModData + the real ra sequence set ————

  game::InstalledMods mods_installed{std::string{str_upstream_root} + "/mods"};
  const game::Manifest* manifest = mods_installed.Find("ra");
  Check(manifest != nullptr, "ra mod found");
  if (manifest == nullptr)
    return g_failures != 0;
  game::ModData mod_data{*manifest, mods_installed, str_upstream_root};

  game::Game game_{game::Game::Deps{.ptr_mod_data = &mod_data}};
  game_.JoinLocal();

  map::MapCache cache{mod_data.ManifestRef(), mod_data.ModFiles()};
  cache.LoadMaps(mod_data, TestMapFilter());
  std::string str_uid;
  for (map::MapPreview* p : cache.Previews())
    if (p->Status() == map::MapStatus::Available) {
      str_uid = p->Uid();
      std::println("map: {} ({})", p->Title(), p->Path());
      break;
    }
  Check(!str_uid.empty(), "an available ra map exists");
  if (str_uid.empty())
    return g_failures != 0;

  map::MapPreview& preview = cache.At(str_uid);
  auto map_world = preview.ToMap();
  Check(map_world != nullptr, "MapPreview.ToMap constructs");
  if (map_world == nullptr)
    return g_failures != 0;

  // 序列装配(共享夹具:真 ra 序列表 + 合成资产副本)
  // The sequence assembly (the shared fixture: ra's real sequence tables
  // + synthetic asset copies).
  InstallSyntheticSequences(*map_world, *manifest, mod_data,
                                     str_upstream_root);

  auto world = std::make_unique<sim::World>(
      *map_world, mod_data, *game_.OrderManagerFace(),
      sim::WorldType::Regular);
  world->LoadComplete(nullptr);
  Check(!world->Players().empty(), "map players created");
  if (world->Players().empty())
    return g_failures != 0;

  // 两个战斗玩家
  // Two combatants.
  std::vector<sim::Player*> vec_combatants;
  for (sim::Player* p : world->Players())
    if (!p->NonCombatant())
      vec_combatants.push_back(p);
  while (vec_combatants.size() < 2) {
    std::size_t index = vec_combatants.size();
    if (index >= world->Players().size())
      index = 0;
    sim::Player* fallback = world->Players()[index];
    if (std::find(vec_combatants.begin(), vec_combatants.end(), fallback) !=
        vec_combatants.end())
      break;
    vec_combatants.push_back(fallback);
  }
  sim::Player* player_a = vec_combatants[0];
  sim::Player* player_b = vec_combatants[1];
  player_a->EnemyPlayersMask =
      player_a->EnemyPlayersMask.Union(player_b->PlayerMask);
  player_b->EnemyPlayersMask =
      player_b->EnemyPlayersMask.Union(player_a->PlayerMask);

  const Rectangle bounds = world->Map().Bounds();
  const int cx = bounds.Left() + bounds.Width / 2;
  const int cy = bounds.Top() + bounds.Height / 2;
  auto spawn = [&](const std::string& str_type, sim::Player* owner,
                   CPos cell) {
    sim::TypeDictionary dict;
    sim::LocationInit location{cell};
    sim::OwnerInit owner_init{owner};
    dict.Add(&location);
    dict.Add(&owner_init);
    return world->CreateActor(true, str_type, dict);
  };

  // WorldRenderer + player 调色板(RenderSprites 的 CachePalette 消费)
  // The WorldRenderer + the player palettes (RenderSprites's CachePalette
  // consumer).
  gfx::WorldRenderer wr{*world, gfx::WorldRenderer::Desc{}};
  {
    // 视口桩(注释族消费 WorldToViewPx/Zoom;上游恒有真 Viewport)
    // The viewport stub (the annotation family consumes WorldToViewPx/Zoom;
    // upstream always has the real Viewport).
    class ViewportStub final : public gfx::IViewportSurface {
     public:
      int2 WorldToViewPx(int2 int2_world) override { return int2_world; }
      int2 WorldToViewPx(const core::Vector3& vec_world) override {
        return int2{static_cast<std::int32_t>(vec_world.X),
                    static_cast<std::int32_t>(vec_world.Y)};
      }
      Rectangle GetScissorBounds(bool) override { return Rectangle{}; }
      int2 TopLeft() override { return int2{}; }
      int2 BottomRight() override { return int2{}; }
      CPos ViewToWorld(int2 int2_view) override {
        return CPos{int2_view.X / 1024, int2_view.Y / 1024};
      }
      int2 ViewToWorldPx(int2 int2_view) override { return int2_view; }
      WPos CenterPosition() const override { return wpos_center; }
      void Center(const WPos& wpos_pos) override {
        b_center_called = true;
        wpos_last_center = wpos_pos;
      }
      void Scroll(core::Vector2 vec_delta, bool) override {
        b_scroll_called = true;
        vec_last_scroll = vec_delta;
      }
      void AdjustZoom(float fp4_dz) override {
        b_zoom_called = true;
        fp4_last_zoom_delta = fp4_dz;
      }
      void AdjustZoom(float fp4_dz, int2) override {
        b_zoom_anchored_called = true;
        fp4_last_zoom_delta = fp4_dz;
      }
      gfx::ScrollDirection GetBlockedDirections() const override {
        return kind_blocked;
      }

      WPos wpos_center{};
      WPos wpos_last_center{};
      core::Vector2 vec_last_scroll{};
      float fp4_last_zoom_delta = 0.0f;
      gfx::ScrollDirection kind_blocked = gfx::ScrollDirection::None;
      bool b_center_called = false;
      bool b_scroll_called = false;
      bool b_zoom_called = false;
      bool b_zoom_anchored_called = false;
    };
    static ViewportStub viewport_stub;
    wr.SetViewport(&viewport_stub);
  }
  {
    // 灰阶基底(色移无关;引用存在性即所测面)
    // A grayscale base (no color shift; the reference's existence is the
    // tested face).
    std::vector<std::uint32_t> vec_argb(256);
    for (int i = 0; i < 256; ++i)
      vec_argb[static_cast<std::size_t>(i)] =
          0xFF000000u | (static_cast<std::uint32_t>(i) * 0x010101u);
    class VectorPalette final : public gfx::IPalette {
     public:
      explicit VectorPalette(std::vector<std::uint32_t> vec)
          : vec_{std::move(vec)} {}
      std::uint32_t At(std::int32_t index) const override {
        return vec_[static_cast<std::size_t>(index)];
      }
      void CopyToArray(std::span<std::byte> vec_destination,
                       std::int32_t int4_destination_offset) const override {
        std::memcpy(vec_destination.data() + int4_destination_offset * 4,
                    vec_.data(), 256 * 4);
      }

     private:
      std::vector<std::uint32_t> vec_;
    };
    for (const sim::Player* p : world->Players()) {
      // ra 的玩家调色板基名两系(PlayerColorPalette 的 BasePalette:
      // player / player-noshadow)
      // ra's two player-palette base names (PlayerColorPalette's
      // BasePalette: player / player-noshadow).
      for (const std::string_view sv_base : {"player", "player-noshadow"})
        wr.AddPalette(std::string{sv_base} + p->InternalName(),
                      gfx::ImmutablePalette{VectorPalette{vec_argb}}, false);
    }
    // 装饰/死亡效果的世界级调色板(chrome = pips·装饰;effect = 碾压)
    // The world-scope palettes of the decoration/death family (chrome =
    // the pips & decorations; effect = the crushed).
    for (const std::string_view sv_base : {"chrome", "effect"})
      wr.AddPalette(std::string{sv_base},
                    gfx::ImmutablePalette{VectorPalette{vec_argb}}, false);
  }

  // ———— 1. e1:WithInfantryBody 动画状态机 ————
  // ———— 1. e1: WithInfantryBody's animation state machine ————

  sim::Actor* e1 = spawn("e1", player_a, CPos{cx, cy});
  Check(e1 != nullptr, "e1 constructed");
  if (e1 != nullptr) {
    // e1 合法双份(无名 + WithInfantryBody@PARACHUTE)—— 取首份
    // e1 legitimately carries two (the unnamed + WithInfantryBody@PARACHUTE)
    // — take the first.
    std::vector<mods::WithInfantryBody*> vec_infantry_bodies =
        e1->TraitsImplementing<mods::WithInfantryBody>();
    CheckEq(vec_infantry_bodies.size(), std::size_t{2},
            "e1 carries the base + PARACHUTE bodies");
    mods::WithInfantryBody* infantry_body =
        vec_infantry_bodies.empty() ? nullptr : vec_infantry_bodies.front();
    Check(infantry_body != nullptr, "e1 WithInfantryBody mounted");
    auto* render_sprites = e1->TraitOrDefault<mods::RenderSprites>();
    Check(render_sprites != nullptr, "e1 RenderSprites mounted");

    if (infantry_body != nullptr) {
      Check(render_sprites->GetImage(*e1) == "e1",
            "RenderSprites.GetImage = actor name lowercased");
      const std::string str_e1_stand =
          std::string{infantry_body->DefaultAnimation().CurrentSequence()
                          ->Name()};
      Check(str_e1_stand == "stand" || str_e1_stand == "stand2",
            "e1 starts on a stand sequence (stand/stand2 pick)");
    }

    // Render:RenderSprites 出真 sprite 项
    // Render: RenderSprites yields real sprite items.
    if (render_sprites != nullptr) {
      std::vector<gfx::RenderItem> vec_items;
      e1->Render(wr, vec_items);
      Check(!vec_items.empty(), "e1 renders at least one item");
      Check(vec_items[0].ptr_sprite != nullptr,
            "render item carries a real sprite");
    }

    // ScreenBounds:非空矩形
    // ScreenBounds: a non-empty rectangle.
    const std::vector<Rectangle> vec_bounds = e1->ScreenBounds(wr);
    Check(!vec_bounds.empty(), "e1 ScreenBounds non-empty");
    if (!vec_bounds.empty())
      Check(!vec_bounds[0].IsEmpty(), "e1 ScreenBounds[0] non-empty rect");

    // run/stand 状态机(移动类型位直驱;Tick 的两分支)
    // The run/stand state machine (the movement-type bit drives it;
    // Tick's two branches).
    if (infantry_body != nullptr) {
      auto* mobile = e1->TraitOrDefault<mods::Mobile>();
      Check(mobile != nullptr, "e1 Mobile mounted");
      if (mobile != nullptr) {
        mobile->SetCurrentMovementTypes(
            sim::MovementType::Horizontal);
        infantry_body->Tick(*e1);
        Check(std::string{infantry_body->DefaultAnimation()
                              .CurrentSequence()
                              ->Name()} == "run",
              "horizontal movement switches to run");

        mobile->SetCurrentMovementTypes(sim::MovementType::None);
        infantry_body->Tick(*e1);
        Check(std::string{infantry_body->DefaultAnimation()
                              .CurrentSequence()
                              ->Name()} == "stand",
              "halt returns to stand");
      }
    }
  }

  // ———— 2. 2tnk:WithSpriteTurret ————
  // ———— 2. 2tnk: WithSpriteTurret ————

  sim::Actor* tnk = spawn("2tnk", player_a, CPos{cx + 4, cy});
  Check(tnk != nullptr, "2tnk constructed");
  if (tnk != nullptr) {
    auto* turret_sprite = tnk->TraitOrDefault<mods::WithSpriteTurret>();
    Check(turret_sprite != nullptr, "2tnk WithSpriteTurret mounted");
    auto* turreted = tnk->TraitOrDefault<mods::Turreted>();
    Check(turreted != nullptr, "2tnk Turreted mounted");

    if (turret_sprite != nullptr && turreted != nullptr) {
      Check(std::string{turret_sprite->DefaultAnimation()
                            .CurrentSequence()
                            ->Name()} == "turret",
            "turret animation starts on the turret sequence");

      // 炮塔朝向被钳到精灵 facings(2tnk turret = 32)
      // The turret facings clamp to the sprite's (2tnk turret = 32).
      CheckEq(turreted->QuantizedFacings(),
              turret_sprite->DefaultAnimation().CurrentSequence()->Facings(),
              "QuantizedFacings clamped to the turret-sequence facings");
      CheckEq(turreted->QuantizedFacings(), 32, "2tnk turret facings = 32");

      // TurretOffset(recoil 求和 + LocalToWorld 旋转)
      // TurretOffset (the recoil sum + the LocalToWorld rotation).
      Check(turret_sprite->TurretOffset(*tnk) == turreted->Position(*tnk),
            "zero-recoil TurretOffset = Turreted.Position");

      // 炮塔渲染项
      // The turret render item.
      std::vector<gfx::RenderItem> vec_items;
      tnk->Render(wr, vec_items);
      Check(vec_items.size() >= 1, "2tnk renders (body + turret)");
    }

    // 车体 = WithFacingSpriteBody(facing 面)
    // The hull = WithFacingSpriteBody (the facing face).
    auto* body_sprite = tnk->TraitOrDefault<mods::WithSpriteBody>();
    Check(body_sprite != nullptr, "2tnk WithFacingSpriteBody as body");
    if (body_sprite != nullptr)
      Check(std::string{body_sprite->DefaultAnimation()
                            .CurrentSequence()
                            ->Name()} == "idle",
            "2tnk body starts on idle");
  }

  // ———— 3. TENT:WithMakeAnimation(make → idle 全程)————
  // ———— 3. TENT: WithMakeAnimation (the make → idle full course) ————

  sim::Actor* tent = spawn("tent", player_a, CPos{cx - 4, cy - 2});
  Check(tent != nullptr, "tent constructed");
  if (tent != nullptr) {
    auto* make = tent->TraitOrDefault<mods::WithMakeAnimation>();
    Check(make != nullptr, "tent WithMakeAnimation mounted");
    auto* body = tent->TraitOrDefault<mods::WithSpriteBody>();
    Check(body != nullptr, "tent WithSpriteBody mounted");

    if (make != nullptr && body != nullptr) {
      Check(std::string{body->DefaultAnimation().CurrentSequence()->Name()} ==
                "make",
            "tent born on the make sequence");

      // make 全程后回 idle(帧长 × 40ms;帧末任务在 world.Tick 帧末 drain)
      // The full make course then back to idle (frame length × 40ms; the
      // frame-end task drains at world.Tick's frame end).
      const std::int32_t int4_frames =
          body->DefaultAnimation().CurrentSequence()->Length();
      Check(int4_frames > 0, "tent make sequence has frames");
      const std::int32_t int4_ticks =
          int4_frames + 2;  // 尾帧 + after 回调一拍 | the tail frame + one
                            // beat for the after callback.
      for (std::int32_t i = 0; i < int4_ticks; ++i)
        world->Tick();
      Check(std::string{body->DefaultAnimation().CurrentSequence()->Name()} ==
                "idle",
            "tent settles on idle after make");
    }
  }

  // ———— 4. ProximityCapturable:邻近换主(真 ActorMap 触发器链)————
  // ———— 4. ProximityCapturable: the proximity owner change (the real
  //      ActorMap trigger chain) ————

  {
    // ra 规则无 ProximityCapturable 承载 actor —— 手动挂 trait(e1 的
    // ^Infantry 带 ProximityCaptor: Infantry)
    // ra's rules carry no ProximityCapturable actor — mount the trait by
    // hand (^Infantry carries ProximityCaptor: Infantry on e1).
    sim::Actor* victim = spawn("tent", player_a, CPos{cx + 12, cy + 6});
    Check(victim != nullptr, "victim tent constructed");
    if (victim != nullptr) {
      mods::ProximityCapturableBaseInfoData data_pc;
      data_pc.bitset_captor_types =
          mods::ProximityCapturableBaseInfoData::DefaultCaptorTypes();
      data_pc.b_must_be_clear = false;
      data_pc.b_sticky = false;
      data_pc.b_permanent = false;
      sim::TypeDictionary dict_pc;
      sim::OwnerInit owner_init_pc{victim->Owner()};
      sim::LocationInit location_pc{
          victim->Trait<sim::IOccupySpace>()->TopLeft()};
      dict_pc.Add(&owner_init_pc);
      dict_pc.Add(&location_pc);
      sim::ActorInitializer init_pc{*victim, dict_pc};
      auto* proximity = world->Arena().Create<mods::ProximityCapturable>(
          init_pc, std::move(data_pc), WDist::FromCells(5));
      victim->AddTrait(proximity);
      proximity->AddedToWorld(*victim);

      // player_b 的 e1 从 5c 外走进入域:期待帧末换主
      // player_b's e1 walks in from beyond 5c: the frame-end owner change
      // is expected.
      sim::Actor* captor = spawn("e1", player_b, CPos{cx + 12, cy});
      Check(captor != nullptr, "captor e1 constructed");
      if (captor != nullptr) {
        auto* mobile = captor->TraitOrDefault<mods::Mobile>();
        Check(mobile != nullptr, "captor Mobile mounted");
        if (mobile != nullptr) {
          sim::Activity* move =
              mobile->MoveTo(CPos{cx + 12, cy + 5}, 0, nullptr, false,
                             std::nullopt);
          captor->QueueActivity(move);

          sim::Player* victim_owner = victim->Owner();
          Check(victim_owner == player_a, "victim starts owned by A");
          for (int i = 0; i < 600 && victim->Owner() == victim_owner; ++i)
            world->Tick();
          Check(victim->Owner() != victim_owner &&
                    victim->Owner() == player_b,
                "proximity capture flips the owner to B");

          // 离域还原(非 Sticky)
          // Leaving reverts (non-Sticky).
          sim::Activity* leave =
              mobile->MoveTo(CPos{cx + 12, cy - 6}, 0, nullptr, false,
                             std::nullopt);
          captor->QueueActivity(leave);
          for (int i = 0; i < 800 && victim->Owner() == player_b; ++i)
            world->Tick();
          Check(victim->Owner() == player_a,
                "leaving the area reverts to A (non-sticky)");
        }
      }
    }
  }

  // ———— 5. Settings/FieldSaver:差异保存 ————
  // ———— 5. Settings/FieldSaver: the save-differences protocol ————

  {
    yaml::StringPool pool;
    game::Settings settings;
    Check(settings.Module("Player") != nullptr, "Player section present");
    Check(settings.Module("Game") != nullptr, "Game section present");

    // 全默认 → 空差异
    // All defaults → the empty difference.
    Check(settings.Module("Player")->Commit(pool).empty(),
          "default Player saves no nodes");

    // 改名 → 单节点差异
    // A rename → the single-node difference.
    const std::vector<const meta::FieldDesc*> vec_fields =
        meta::CollectFields(settings.Module("Player")->Record().record_desc());
    std::size_t sz_name_slot = vec_fields.size();
    for (std::size_t i = 0; i < vec_fields.size(); ++i)
      if (vec_fields[i]->str_name == "Name")
        sz_name_slot = i;
    settings.Module("Player")->Record().Slot(sz_name_slot) =
        meta::GenericValue::Of(std::string{"NewName"});
    const std::vector<yaml::MiniYamlNode> vec_diff =
        settings.Module("Player")->Commit(pool);
    CheckEq(vec_diff.size(), std::size_t{1}, "only Name differs");
    if (vec_diff.size() == 1) {
      Check(*vec_diff[0].Key == "Name", "diff node key = Name");
      Check(std::string_view{*vec_diff[0].Value.Value} == "NewName",
            "diff node value = NewName");
    }

    // SaveRecord/SaveRecordDifferences 的 FieldSaver 面
    // The FieldSaver faces SaveRecord/SaveRecordDifferences.
    yaml::MiniYaml yaml_saved = meta::SaveRecord(settings.Module("Player")->Record(), pool);
    Check(!yaml_saved.Nodes.empty(), "SaveRecord yields the field nodes");

    // 回读:节点 → 记录
    // The reload: the nodes → the record.
    game::Settings settings_loaded;
    settings_loaded.Load(settings.Save(pool), pool);
    Check(settings_loaded.Module("Player") != nullptr &&
              std::get_if<std::string>(
                  &settings_loaded.Module("Player")->Record().Slot(
                      sz_name_slot)
                      .val) != nullptr &&
              *std::get_if<std::string>(
                   &settings_loaded.Module("Player")->Record().Slot(
                       sz_name_slot)
                       .val) == "NewName",
          "settings reload round-trips the rename");
  }

  // ———— 6. ReplayRecorder/ReplayConnection:三段包序 ————
  // ———— 6. ReplayRecorder/ReplayConnection: the three-part packet order
  //      ————

  {
    // 开局包:frame 0 的 "StartGame" order(OrderIO 序列化)
    // The game-start packet: frame 0's "StartGame" order (OrderIO
    // serialized).
    const net::Order order_start{"StartGame", nullptr, false};
    net::OrderPacket packet_start{std::vector<const net::Order*>{&order_start}};
    const std::vector<std::uint8_t> vec_start =
        packet_start.Serialize(0);

    net::ReplayRecorder recorder{[] { return std::string{"test"}; }};

    // 开局前包(进 preStart 缓冲)
    // A pre-start packet (into the pre-start buffer).
    const std::vector<std::uint8_t> vec_pre{0x11, 0x22, 0x33};
    recorder.Receive(7, vec_pre);
    recorder.Receive(1, vec_start);
    const std::vector<std::uint8_t> vec_post{0xAA, 0xBB};
    recorder.ReceiveFrame(1, 3, vec_post);
    net::ReplayMetadata meta_render{"FinalGameTick: 3"};
    recorder.Metadata = &meta_render;
    recorder.Dispose();

    // 读回:ctor 的 chunk/有效性面(包序逐字节回读随 replay_test 第十一批)
    // The read-back: the ctor's chunk/validity faces (the byte-exact
    // packet read-back rides batch 11's replay_test).
    net::ReplayConnection connection{recorder.Bytes()};
    Check(connection.IsValid(), "game-start packet marks replay valid");
    Check(!connection.HasLobbyInfo(), "no SyncInfo in stream");
    CheckEq(connection.TickCount(), 3, "tick count = frame 3");
    Check(connection.Metadata() != nullptr, "metadata tail parsed");
  }

  // ———— 7. SyncReport:环形记账 ————
  // ———— 7. SyncReport: the ring bookkeeping ————

  {
    net::SyncReport sync_report{*game_.OrderManagerFace()};
    game_.OrderManagerFace()->SetWorld(world.get());

    const std::vector<sim::Actor*> vec_sync_actors =
        world->ActorsHavingTrait<sim::ISync>();
    Check(!vec_sync_actors.empty(), "sync actors present");
    sync_report.UpdateSyncReport(vec_sync_actors,
                                 world->SyncedEffects());

    std::vector<std::string> vec_lines;
    const int int4_frame = game_.OrderManagerFace()->NetFrameNumber();
    sync_report.DumpSyncReport(int4_frame,
                               [&](std::string_view line) {
                                 vec_lines.emplace_back(line);
                               });
    bool b_has_traits_header = false;
    for (const auto& line : vec_lines)
      if (line.find("Synced Traits:") != std::string::npos)
        b_has_traits_header = true;
    Check(b_has_traits_header, "dump carries the Synced Traits section");
    Check(sync_report.RecordedFrames().size() ==
              net::SyncReport::kNumSyncReports,
          "the 7-report ring");
  }

  // ———— 8. 第九批:选择注释/装饰/死亡动画/损伤覆盖/生产条 ————
  // ———— 8. Batch 9: the selection annotations/decoration/death/damage/
  //      production faces ————

  {
    // 8a. e1 的选择注释(^Selectable:SelectionDecorations + 控制组装饰)
    // 8a. e1's selection annotations.
    std::vector<sim::IRenderAnnotations*> vec_annotations =
        e1 != nullptr ? e1->TraitsImplementing<sim::IRenderAnnotations>()
                      : std::vector<sim::IRenderAnnotations*>{};
    Check(!vec_annotations.empty(),
          "e1 carries IRenderAnnotations (SelectionDecorations)");
    if (e1 != nullptr && !vec_annotations.empty()) {
      sim::IRenderAnnotations* annotations = vec_annotations.front();

      // 未选中 + Standard → 零项 | Unselected + Standard → zero items.
      std::vector<gfx::RenderItem> vec_items;
      annotations->RenderAnnotations(*e1, wr, vec_items);
      Check(vec_items.empty(),
            "unselected e1 renders no annotations (Standard bars)");

      // 选中 → 选择框 + 血条(两个 Custom 项)
      // Selected → the box + bars (two Custom items).
      world->Selection()->Add(e1);
      annotations->RenderAnnotations(*e1, wr, vec_items);
      int int4_custom = 0;
      for (const gfx::RenderItem& item : vec_items)
        if (item.kind == gfx::RenderableKind::Custom)
          ++int4_custom;
      Check(int4_custom >= 2,
            "selected e1 renders the selection box + bars annotations");

      // 编组 → 控制组 pip | Grouped → the control-group pip.
      world->ControlGroups()->AddToControlGroup(e1, 3);
      vec_items.clear();
      annotations->RenderAnnotations(*e1, wr, vec_items);
      int int4_ui_sprites = 0;
      for (const gfx::RenderItem& item : vec_items)
        if (item.kind == gfx::RenderableKind::UISprite)
          ++int4_ui_sprites;
      Check(int4_ui_sprites >= 1,
            "the control-group decoration renders the group pip");
      world->ControlGroups()->RemoveFromControlGroup(e1);
      world->Selection()->Remove(e1);
    }

    // 8b. spy 的条件装饰初始禁用 | 8b. spy's conditional decoration.
    sim::Actor* spy = spawn("spy", player_a, CPos{cx - 8, cy + 3});
    Check(spy != nullptr, "spy constructed");
    if (spy != nullptr) {
      mods::WithDecoration* decoration_spy = nullptr;
      for (sim::IDecoration* decoration :
           spy->TraitsImplementing<sim::IDecoration>())
        if ((decoration_spy =
                 dynamic_cast<mods::WithDecoration*>(decoration)) != nullptr)
          break;
      Check(decoration_spy != nullptr, "spy WithDecoration@disguise mounted");
      if (decoration_spy != nullptr)
        Check(decoration_spy->IsTraitDisabled(),
              "the disguise decoration starts disabled (RequiresCondition)");
    }

    // 8c. e1 死亡动画(DefaultDeath → SpriteEffect)
    // 8c. e1's death animation.
    sim::Actor* victim = spawn("e1", player_a, CPos{cx + 8, cy + 2});
    Check(victim != nullptr, "death victim e1 constructed");
    if (victim != nullptr) {
      Check(victim->TraitOrDefault<mods::WithDeathAnimation>() != nullptr,
            "victim mounts WithDeathAnimation (^Infantry)");
      const std::size_t sz_effects_before = world->Effects().size();
      static sim::Damage damage_death{0};
      const std::vector<std::string> vec_death_types{"DefaultDeath"};
      damage_death.Value = 100000;
      damage_death.DamageTypes = core::BitSet<sim::DamageType>::FromRawBits(
          meta::BitsOf("OpenRA.Traits.DamageType", vec_death_types));
      // AutoTarget.Damaged 直读 e.Attacker(上游无空判)—— 传活攻击者
      // Upstream reads e.Attacker unguarded — pass a live attacker.
      victim->Trait<sim::IHealth>()->InflictDamage(*victim, e1,
                                                   damage_death, true);
      world->Tick();  // 帧末:效果入世界 | frame end: the effect enters
      bool b_death_effect = false;
      for (sim::IEffect* effect : world->Effects())
        if (auto* sprite_effect =
                dynamic_cast<mods::SpriteEffect*>(effect)) {
          b_death_effect = true;
          Check(sprite_effect->ImageNameForTest() == "e1",
                "the death effect carries the e1 image");
          const std::string str_sequence =
              sprite_effect->SequenceNameForTest();
          Check(str_sequence == "die1" || str_sequence == "die2" ||
                    str_sequence == "die3",
                "the death sequence = die + DefaultDeath suffix");
        }
      Check(b_death_effect,
            "killing e1 spawns the death SpriteEffect (frame end)");
      Check(world->Effects().size() > sz_effects_before,
            "the death effect entered the world's effect list");
    }

    // 8d. jeep 重损烟雾链 | 8d. jeep's heavy-damage smoke chain.
    sim::Actor* jeep = spawn("jeep", player_a, CPos{cx - 6, cy + 4});
    Check(jeep != nullptr, "jeep constructed");
    if (jeep != nullptr) {
      auto* overlay = jeep->TraitOrDefault<mods::WithDamageOverlay>();
      Check(overlay != nullptr, "jeep WithDamageOverlay mounted (^vehicle)");
      if (overlay != nullptr) {
        Check(!overlay->IsPlayingAnimation(),
              "the overlay stays idle while undamaged");
        sim::IHealth* health = jeep->Trait<sim::IHealth>();
        static sim::Damage damage_smoke{0};
        damage_smoke.Value = health->MaxHP() * 3 / 5;  // 60% → Heavy
        health->InflictDamage(*jeep, e1, damage_smoke, true);
        Check(overlay->IsPlayingAnimation(),
              "heavy damage starts the smoke chain immediately "
              "(InitialDelay 0)");
      }
    }

    // 8e. spen 生产条绑定经典 Ship 队列 | 8e. spen's production bar.
    sim::Actor* spen = spawn("spen", player_a, CPos{cx - 2, cy + 8});
    Check(spen != nullptr, "spen constructed");
    if (spen != nullptr) {
      auto* bar = spen->TraitOrDefault<mods::ProductionBar>();
      Check(bar != nullptr, "spen ProductionBar mounted");
      if (bar != nullptr) {
        Check(bar->BoundQueueForTest() != nullptr,
              "the bar binds the classic Ship queue");
        bar->Tick(*spen);
        CheckEq(bar->GetValue(), 0.0f, "the empty queue reads value 0");
        Check(!bar->DisplayWhenEmpty(), "DisplayWhenEmpty = false");
      }
    }

    // 8f. harv 存量 pips | 8f. harv's stores pips.
    sim::Actor* harv = spawn("harv", player_a, CPos{cx + 6, cy - 6});
    Check(harv != nullptr, "harv constructed");
    if (harv != nullptr) {
      auto* pips =
          harv->TraitOrDefault<mods::WithStoresResourcesPipsDecoration>();
      Check(pips != nullptr, "harv WithStoresResourcesPips mounted");
      if (pips != nullptr) {
        Check(pips->RequiresSelection(),
              "the stores pips require selection");
        CheckEq(harv->TraitsImplementing<sim::IDecoration>().size(),
                std::size_t{2},
                "harv carries the stores pips + the control-group pip");
      }
    }
  }

  {
    const game::ActorInfo* info_e1 = world->Map().Rules().FindActor("e1");
    Check(info_e1 != nullptr, "p6b8: e1 info");
    const game::ActorInfo* info_2tnk = world->Map().Rules().FindActor("2tnk");
    Check(info_2tnk != nullptr, "p6b8: 2tnk info");
    if (info_e1 == nullptr || info_2tnk == nullptr)
      return g_failures != 0;

    sim::TypeDictionary dict;
    sim::OwnerInit owner_init{player_a};
    sim::FactionInit faction_init{player_a->Faction().InternalName};
    dict.Add(&owner_init);
    dict.Add(&faction_init);

    gfx::ActorPreviewInitializer init_default(*info_e1, wr, dict);
    Check(init_default.World().Map().Sequences() != nullptr,
          "p6b8: initializer world reaches the sequence set");
    CheckEq(init_default.GetFacing()().Angle, 0, "p6b8: default facing 0");
    CheckEq(init_default.GetOrientation()().Yaw.Angle, 0,
            "p6b8: default orientation yaw 0");
    Check(init_default.GetDamageState() == sim::DamageState::Undamaged,
          "p6b8: no HealthInit = Undamaged");

    sim::FacingInit facing_init{WAngle{298}};
    dict.Add(&facing_init);
    CheckEq(init_default.GetFacing()().Angle, 298, "p6b8: FacingInit 298");

    std::int64_t int8_dynamic_facing = 128;
    sim::DynamicFacingInit dynamic_init{
        [&int8_dynamic_facing] { return WAngle{static_cast<int>(int8_dynamic_facing)}; }};
    dict.Add(&dynamic_init);
    CheckEq(init_default.GetFacing()().Angle, 128,
            "p6b8: DynamicFacingInit overrides FacingInit");
    int8_dynamic_facing = 512;
    CheckEq(init_default.GetFacing()().Angle, 512,
            "p6b8: dynamic facing is live");
    dict.Remove(&dynamic_init);
    dict.Remove(&facing_init);

    sim::HealthInit health_dead{0};
    dict.Add(&health_dead);
    Check(init_default.GetDamageState() == sim::DamageState::Dead,
          "p6b8: hp 0 = Dead");
    dict.Remove(&health_dead);
    sim::HealthInit health_critical{24};
    dict.Add(&health_critical);
    Check(init_default.GetDamageState() == sim::DamageState::Critical,
          "p6b8: hp 24 = Critical");
    dict.Remove(&health_critical);
    sim::HealthInit health_heavy{49};
    dict.Add(&health_heavy);
    Check(init_default.GetDamageState() == sim::DamageState::Heavy,
          "p6b8: hp 49 = Heavy");
    dict.Remove(&health_heavy);
    sim::HealthInit health_medium{74};
    dict.Add(&health_medium);
    Check(init_default.GetDamageState() == sim::DamageState::Medium,
          "p6b8: hp 74 = Medium");
    dict.Remove(&health_medium);
    sim::HealthInit health_light{99};
    dict.Add(&health_light);
    Check(init_default.GetDamageState() == sim::DamageState::Light,
          "p6b8: hp 99 = Light");
    dict.Remove(&health_light);

    Rectangle rect_e1_preview = Rectangle::Empty();
    auto vec_e1 = gfx::RenderActorPreviewOf(*info_e1, init_default);
    CheckEq(vec_e1.size(), std::size_t{1},
            "p6b8: e1 preview = 1 (parachute body disabled by default)");
    if (vec_e1.size() == 1) {
      std::vector<gfx::RenderItem> vec_items;
      vec_e1.front()->RenderUI(wr, int2{10, 10}, 1.0f, vec_items);
      Check(!vec_items.empty(), "p6b8: e1 RenderUI yields items");
      bool b_sprite_item = false;
      for (const gfx::RenderItem& item : vec_items)
        if (item.kind == gfx::RenderableKind::UISprite)
          b_sprite_item = true;
      Check(b_sprite_item, "p6b8: e1 preview item is a sprite");

      std::vector<Rectangle> vec_bounds;
      vec_e1.front()->ScreenBounds(wr, WPos::Zero(), vec_bounds);
      Check(vec_bounds.size() == 1 && vec_bounds[0].Width > 0 &&
                vec_bounds[0].Height > 0,
            "p6b8: e1 ScreenBounds positive");
      rect_e1_preview = vec_bounds[0];

      vec_e1.front()->Tick();
      vec_e1.front()->Tick();
      Check(true, "p6b8: e1 preview Tick");
    }

    gfx::ActorPreviewInitializer init_2tnk(*info_2tnk, wr, dict);
    auto vec_2tnk = gfx::RenderActorPreviewOf(*info_2tnk, init_2tnk);
    CheckEq(vec_2tnk.size(), std::size_t{2},
            "p6b8: 2tnk preview = facing body + turret");
    if (vec_2tnk.size() == 2) {
      std::vector<gfx::RenderItem> vec_body;
      vec_2tnk[0]->Render(wr, WPos::Zero(), vec_body);
      Check(!vec_body.empty(), "p6b8: 2tnk body Render yields items");
      std::vector<gfx::RenderItem> vec_turret;
      vec_2tnk[1]->Render(wr, WPos::Zero(), vec_turret);
      Check(!vec_turret.empty(), "p6b8: 2tnk turret Render yields items");
    }

    ui::WidgetArgs args_preview;
    args_preview.Add("worldRenderer", static_cast<gfx::WorldRenderer*>(&wr));
    ui::ActorPreviewWidget widget_preview(args_preview);
    widget_preview.bounds_widget.int4_x = 4;
    widget_preview.bounds_widget.int4_y = 6;
    widget_preview.bounds_widget.int4_width = 64;
    widget_preview.bounds_widget.int4_height = 48;
    widget_preview.SetPreview(*info_e1, dict);
    Check(widget_preview.IdealPreviewSize().X > 0 &&
              widget_preview.IdealPreviewSize().Y > 0,
          "p6b8: widget IdealPreviewSize positive");
    CheckEq(widget_preview.PreviewOffset().X,
            rect_e1_preview.Left() + widget_preview.IdealPreviewSize().X / 2,
            "p6b8: widget PreviewOffset.X = topLeft + width/2");
    CheckEq(widget_preview.PreviewOffset().Y,
            rect_e1_preview.Top() + widget_preview.IdealPreviewSize().Y / 2,
            "p6b8: widget PreviewOffset.Y = topLeft + height/2");
    widget_preview.b_animate = true;
    widget_preview.Tick();
    widget_preview.PrepareRenderables();
    Check(true, "p6b8: widget PrepareRenderables");
    auto widget_clone = widget_preview.Clone();
    Check(widget_clone->TypeName() == std::string_view{"ActorPreviewWidget"},
          "p6b8: widget clone type");
  }

  // ———— 第九批:输入装配(SelectableExts/SelectionUtils/WIC/VCW)————
  // ———— Batch nine: the input assembly (SelectableExts/SelectionUtils/
  //      WIC/VCW) ————
  {
    world->SetRenderPlayer(player_a);
    world->ScreenMapFace()->SetActorBoundsSources(
        [&wr](sim::Actor& a) { return a.MouseBounds(wr); },
        [&wr](sim::Actor& a) { return a.ScreenBounds(wr); });

    sim::Actor* e1_ally1 = spawn("e1", player_a, CPos{cx + 5, cy});
    sim::Actor* e1_ally2 = spawn("e1", player_a, CPos{cx + 8, cy});
    sim::Actor* e1_enemy = spawn("e1", player_b, CPos{cx + 11, cy});
    Check(e1_ally1 != nullptr && e1_ally2 != nullptr && e1_enemy != nullptr,
          "p6b9: three e1 combatants spawned");
    for (int i = 0; i < 5; i++) {
      world->Tick();
      world->TickRender();
    }

    const int int4_base_priority =
        sim::SelectionPriority(*e1_ally1, Modifiers::None);
    Check(sim::SelectionPriority(*e1_enemy, Modifiers::None) ==
              int4_base_priority - 90,
          "p6b9: enemy actor priority = base - 3*30");

    const sim::ActorBoundsPair pair_near{
        e1_ally1, Polygon{Rectangle::FromLTRB(-16, -16, 16, 16)}};
    const sim::ActorBoundsPair pair_far{
        e1_ally2, Polygon{Rectangle::FromLTRB(484, -16, 516, 16)}};
    const sim::ActorBoundsPair vec_pairs[] = {pair_near, pair_far};
    Check(sim::WithHighestSelectionPriority(vec_pairs, int2{0, 0},
                                            Modifiers::None) == e1_ally1,
          "p6b9: nearest bounds wins the priority key");
    const sim::ActorBoundsPair vec_pairs_tie[] = {pair_near, pair_near};
    Check(sim::WithHighestSelectionPriority(vec_pairs_tie, int2{0, 0},
                                            Modifiers::None) == e1_ally1,
          "p6b9: equal keys keep the first entry");

    sim::Actor* vec_subset_in[] = {e1_ally1, e1_ally2, e1_enemy};
    const std::vector<sim::Actor*> vec_subset =
        sim::SubsetWithHighestSelectionPriority(vec_subset_in, Modifiers::None);
    Check(vec_subset.size() == 2 &&
              std::find(vec_subset.begin(), vec_subset.end(), e1_ally1) !=
                  vec_subset.end() &&
              std::find(vec_subset.begin(), vec_subset.end(), e1_ally2) !=
                  vec_subset.end(),
          "p6b9: subset keeps the max-priority group only");

    const std::vector<sim::Player*> vec_include =
        ui::GetPlayersToIncludeInSelection(*world);
    Check(vec_include.size() == 1 && vec_include[0] == player_a,
          "p6b9: viewer-only players to include");

    const int2 px_ally1 = wr.ScreenPxPosition(e1_ally1->CenterPosition());
    const int2 px_ally2 = wr.ScreenPxPosition(e1_ally2->CenterPosition());
    const std::vector<sim::Actor*> vec_point = ui::SelectActorsInBoxWithDeadzone(
        *world, px_ally1, px_ally1, Modifiers::None);
    Check(vec_point.size() == 1 && vec_point[0] == e1_ally1,
          "p6b9: deadzone point query picks the single ally");
    const std::vector<sim::Actor*> vec_box = ui::SelectActorsInBoxWithDeadzone(
        *world, px_ally1, px_ally2, Modifiers::None);
    Check(vec_box.size() == 2,
          "p6b9: box query returns both allies (same priority group)");

    Check(world->OrderGenerator()->ActionButton() == MouseButton::Right,
          "p6b9: default generator ActionButton = modern Right");

    ui::WidgetArgs args_wic;
    args_wic.Add("world", world.get());
    args_wic.Add("worldRenderer", &wr);
    mods::WorldInteractionControllerWidget widget_wic(args_wic);
    const auto mi_click = [&](MouseInputEvent kind_event, int2 xy) {
      return MouseInput{.Event = kind_event,
                        .Button = MouseButton::Left,
                        .Location = xy,
                        .Delta = int2{0, 0},
                        .ModifierFlags = Modifiers::None,
                        .MultiTapCount = 0};
    };
    Check(widget_wic.HandleMouseInput(mi_click(MouseInputEvent::Down, px_ally1)),
          "p6b9: WIC consumes left down");
    Check(widget_wic.HandleMouseInput(mi_click(MouseInputEvent::Up, px_ally1)),
          "p6b9: WIC consumes left up");
    Check(world->Selection()->Contains(e1_ally1),
          "p6b9: left click selects the ally");
    Check(widget_wic.HandleMouseInput(mi_click(MouseInputEvent::Down, px_ally2)) &&
              widget_wic.HandleMouseInput(mi_click(MouseInputEvent::Up, px_ally2)) &&
              world->Selection()->Contains(e1_ally2) &&
              !world->Selection()->Contains(e1_ally1),
          "p6b9: second click re-selects (combine replaces on click)");
    widget_wic.Draw();
    Check(world->Selection()->RolloverContains(e1_ally2),
          "p6b9: Draw sets the rollover");

    Check(widget_wic.HandleKeyPress(KeyInput{.Event = KeyInputEvent::Up,
                                             .Key = Keycode::LSHIFT,
                                             .ModifierFlags = Modifiers::None,
                                             .MultiTapCount = 0,
                                             .UnicodeChar = u'\0',
                                             .IsRepeat = false}) == false,
          "p6b9: shift-up ignored without queued command");

    const std::string* ptr_cursor_wic = widget_wic.GetCursor(px_ally2);
    Check(ptr_cursor_wic != nullptr && *ptr_cursor_wic == "select",
          "p6b9: WIC GetCursor over selectable ally = select");

    RecordingViewportStub viewport_recording;
    wr.SetViewport(&viewport_recording);
    mods::ViewportControllerWidget widget_vcw(args_wic);
    widget_vcw.hk_zoom_in = ui::HotkeyReference(
        [] { return ui::Hotkey{Keycode::Z, Modifiers::None}; });
    widget_vcw.hk_zoom_out = ui::HotkeyReference(
        [] { return ui::Hotkey{Keycode::X, Modifiers::None}; });
    widget_vcw.hk_jump_to_top_edge = ui::HotkeyReference(
        [] { return ui::Hotkey{Keycode::W, Modifiers::None}; });
    widget_vcw.HandleKeyPress(KeyInput{.Event = KeyInputEvent::Down,
                                       .Key = Keycode::Z,
                                       .ModifierFlags = Modifiers::None,
                                       .MultiTapCount = 0,
                                       .UnicodeChar = u'z',
                                       .IsRepeat = false});
    Check(viewport_recording.b_zoom_called &&
              viewport_recording.fp4_last_zoom_delta == 0.25f,
          "p6b9: ZoomInKey AdjustZoom(+0.25)");
    widget_vcw.HandleKeyPress(KeyInput{.Event = KeyInputEvent::Down,
                                       .Key = Keycode::X,
                                       .ModifierFlags = Modifiers::None,
                                       .MultiTapCount = 0,
                                       .UnicodeChar = u'x',
                                       .IsRepeat = false});
    Check(viewport_recording.fp4_last_zoom_delta == -0.25f,
          "p6b9: ZoomOutKey AdjustZoom(-0.25)");
    viewport_recording.wpos_center = WPos{4096, 4096, 0};
    widget_vcw.HandleKeyPress(KeyInput{.Event = KeyInputEvent::Down,
                                       .Key = Keycode::W,
                                       .ModifierFlags = Modifiers::None,
                                       .MultiTapCount = 0,
                                       .UnicodeChar = u'w',
                                       .IsRepeat = false});
    Check(viewport_recording.b_center_called &&
              viewport_recording.wpos_last_center == WPos{4096, 0, 0},
          "p6b9: JumpToTopEdgeKey centers (x, 0)");

    ui::Ui::Deps deps_ui_input;
    std::map<std::string, ui::Hotkey> map_hotkeys_input;
    map_hotkeys_input.emplace("Save01", ui::Hotkey{Keycode::F1, Modifiers::None});
    map_hotkeys_input.emplace("Save02", ui::Hotkey{Keycode::F2, Modifiers::None});
    map_hotkeys_input.emplace("Restore01",
                              ui::Hotkey{Keycode::F3, Modifiers::None});
    map_hotkeys_input.emplace("Restore02",
                              ui::Hotkey{Keycode::F4, Modifiers::None});
    deps_ui_input.fn_hotkey_lookup =
        [&map_hotkeys_input](std::string_view sv_name) {
          const auto it = map_hotkeys_input.find(std::string{sv_name});
          if (it != map_hotkeys_input.end())
            return ui::HotkeyReference(
                [hk = it->second] { return hk; });
          return ui::HotkeyReferenceFromNameOrCode(sv_name);
        };
    deps_ui_input.fn_window_size = [] { return int2{1280, 720}; };
    ui::Ui::Instance().Initialize(std::move(deps_ui_input));

    Check(widget_vcw.LoadFieldOrProperty("BookmarkSaveKeyPrefix", "Save") &&
              widget_vcw.LoadFieldOrProperty("BookmarkRestoreKeyPrefix",
                                             "Restore") &&
              widget_vcw.LoadFieldOrProperty("BookmarkKeyCount", "2"),
          "p6b9: bookmark fields load");
    ui::WidgetArgs args_vcw_init;
    widget_vcw.Initialize(args_vcw_init);
    viewport_recording.wpos_center = WPos{8192, 8192, 0};
    widget_vcw.HandleKeyPress(KeyInput{.Event = KeyInputEvent::Down,
                                       .Key = Keycode::F1,
                                       .ModifierFlags = Modifiers::None,
                                       .MultiTapCount = 0,
                                       .UnicodeChar = u'\0',
                                       .IsRepeat = false});
    viewport_recording.wpos_center = WPos{0, 0, 0};
    widget_vcw.HandleKeyPress(KeyInput{.Event = KeyInputEvent::Down,
                                       .Key = Keycode::F3,
                                       .ModifierFlags = Modifiers::None,
                                       .MultiTapCount = 0,
                                       .UnicodeChar = u'\0',
                                       .IsRepeat = false});
    Check(viewport_recording.wpos_last_center == WPos{8192, 8192, 0},
          "p6b9: bookmark restore centers on the saved position");

    gfx::Viewport::int2_last_mouse_pos =
        int2{(cx + 20) * 1024, cy * 1024};
    ui::Ui::Instance().SetMouseOverWidget(&widget_vcw);
    widget_vcw.UpdateCameraTick();
    Check(widget_vcw.kind_tooltip_type == mods::WorldTooltipType::Unexplored,
          "p6b9: pre-explore tooltip type = Unexplored");
    gfx::Viewport::int2_last_mouse_pos = int2{0, 0};
    widget_vcw.UpdateCameraTick();
    const std::string* ptr_cursor_edge = widget_vcw.GetCursor(int2{0, 0});
    Check(ptr_cursor_edge != nullptr && *ptr_cursor_edge == "scroll-tl",
          "p6b9: edge scroll cursor = scroll-tl at the top-left corner");
    viewport_recording.kind_blocked = gfx::ScrollDirection::Up;
    ptr_cursor_edge = widget_vcw.GetCursor(int2{0, 0});
    gfx::Viewport::int2_last_mouse_pos = int2{0, 360};
    widget_vcw.UpdateCameraTick();
    viewport_recording.kind_blocked =
        gfx::ScrollDirection::Left | gfx::ScrollDirection::Up;
    ptr_cursor_edge = widget_vcw.GetCursor(int2{0, 360});
    Check(ptr_cursor_edge != nullptr && *ptr_cursor_edge == "scroll-l-blocked",
          "p6b9: blocked edge cursor gains the -blocked suffix");
    viewport_recording.kind_blocked = gfx::ScrollDirection::None;
    ui::Ui::Instance().SetMouseOverWidget(nullptr);

    if (player_a->GetShroud() != nullptr)
      player_a->GetShroud()->ExploreAll();
    widget_vcw.UpdateCameraTick();
    Check(widget_vcw.kind_tooltip_type == mods::WorldTooltipType::None,
          "p6b9: explored tooltip falls through to None");

    wr.SetViewport(nullptr);
  }

  // ———— 第十批:生产 palette 全链(BuildingUtils/LineBuild/PlaceBuilding/
  //      PrimaryBuilding/ProductionIconOverlay/palette widget)————
  // ———— Batch ten: the production-palette chain (BuildingUtils/LineBuild/
  //      PlaceBuilding/PrimaryBuilding/ProductionIconOverlay/the palette
  //      widget) ————
  {
    mods::DeveloperMode* dev_mode =
        player_a->PlayerActor()->TraitOrDefault<mods::DeveloperMode>();
    Check(dev_mode != nullptr, "p6b10: player DeveloperMode resolved");
    if (dev_mode != nullptr)
      dev_mode->AllTech = true;

    const game::ActorInfo* tent_info = map_world->Rules().FindActor("tent");
    Check(tent_info != nullptr, "p6b10: tent ActorInfo found");
    const meta::RecordObject* rec_tent_building =
        tent_info != nullptr
            ? mods::FindTraitInfoRecord(*tent_info, "BuildingInfo")
            : nullptr;
    Check(rec_tent_building != nullptr, "p6b10: tent BuildingInfo record");
    mods::BuildingInfoData tent_bi =
        rec_tent_building != nullptr
            ? mods::BuildingInfoData::Parse(*rec_tent_building)
            : mods::BuildingInfoData{};

    CPos site_tent{cx, cy};
    bool b_found_site = false;
    for (int dy = 0; dy < 10 && !b_found_site; dy++)
      for (int dx = 0; dx < 10 && !b_found_site; dx++) {
        const CPos c{cx + dx, cy + dy};
        if (mods::CanPlaceBuilding(*world, c, tent_info, tent_bi, nullptr)) {
          site_tent = c;
          b_found_site = true;
        }
      }
    Check(b_found_site, "p6b10: clear site found for TENT");
    Check(!mods::CanPlaceBuilding(*world, site_tent + CVec{-500, 0},
                                  tent_info, tent_bi, nullptr),
          "p6b10: off-map site rejected");

    sim::Actor* tent_existing = spawn("tent", player_a, site_tent);
    world->Tick();
    Check(tent_existing != nullptr, "p6b10: existing TENT spawned");
    Check(!mods::CanPlaceBuilding(*world, site_tent, tent_info, tent_bi,
                                  nullptr),
          "p6b10: occupied site not placeable");
    Check(!mods::CanPlaceBuilding(*world, site_tent, tent_info, tent_bi,
                                  tent_existing),
          "p6b10: building influence blocks even with the occupant ignored");

    // ———— LineBuild:SBAG 连线(cell 展开方向 + 节点连接)————
    // ———— LineBuild: the SBAG run (the cell expansion directions + the
    //      node connections) ————
    const game::ActorInfo* sbag_info = map_world->Rules().FindActor("sbag");
    Check(sbag_info != nullptr, "p6b10: sbag ActorInfo found");
    const meta::RecordObject* rec_sbag_building =
        sbag_info != nullptr
            ? mods::FindTraitInfoRecord(*sbag_info, "BuildingInfo")
            : nullptr;
    mods::BuildingInfoData sbag_bi =
        rec_sbag_building != nullptr
            ? mods::BuildingInfoData::Parse(*rec_sbag_building)
            : mods::BuildingInfoData{};

    CPos run[3] = {CPos{0, 0}, CPos{0, 0}, CPos{0, 0}};
    bool b_found_run = false;
    for (int dy = 0; dy < 12 && !b_found_run; dy++)
      for (int dx = 0; dx < 12 && !b_found_run; dx++) {
        const CPos c0{cx + dx, cy + dy};
        bool b_all_clear = true;
        for (int i = 0; i < 3; i++)
          if (!mods::IsCellBuildable(*world, CPos{c0.X() + i, c0.Y()},
                                     sbag_info, sbag_bi))
            b_all_clear = false;
        if (b_all_clear) {
          for (int i = 0; i < 3; i++)
            run[i] = CPos{c0.X() + i, c0.Y()};
          b_found_run = true;
        }
      }
    Check(b_found_run, "p6b10: three-clear-cell run found");
    if (player_a->GetShroud() != nullptr)
      player_a->GetShroud()->ExploreAll();
    sim::Actor* sbag_node = spawn("sbag", player_a, run[2]);
    world->Tick();
    Check(sbag_node != nullptr, "p6b10: sbag node spawned");
    Check(sbag_node->TraitOrDefault<mods::LineBuild>() != nullptr &&
              sbag_node->TraitOrDefault<mods::LineBuildNode>() != nullptr,
          "p6b10: sbag carries LineBuild + LineBuildNode");

    const std::vector<std::pair<CPos, sim::Actor*>> vec_line_cells =
        mods::GetLineBuildCells(*world, run[0], *sbag_info, sbag_bi,
                                *player_a);
    bool b_cell_run0 = false;
    bool b_cell_run1 = false;
    bool b_connector_is_node = true;
    for (const auto& [cell, connector] : vec_line_cells) {
      if (cell == run[0])
        b_cell_run0 = true;
      if (cell == run[1])
        b_cell_run1 = connector == sbag_node;
      if (cell == run[2])
        b_connector_is_node = false;
    }
    Check(b_cell_run0, "p6b10: GetLineBuildCells yields the buildable origin");
    Check(b_cell_run1,
          "p6b10: intermediate cell yielded with the node connector");
    Check(b_connector_is_node, "p6b10: node cell itself not yielded");

    // 段出生经 LineBuildParentInit 驱动父节点的 SegmentAdded
    // A segment's birth drives the parent's SegmentAdded through
    // LineBuildParentInit.
    {
      sim::TypeDictionary dict_segment;
      dict_segment.Add(world->Arena().Create<sim::LocationInit>(run[1]));
      dict_segment.Add(world->Arena().Create<sim::OwnerInit>(player_a));
      dict_segment.Add(world->Arena().Create<sim::FactionInit>(
          std::string{player_a->Faction().InternalName}));
      dict_segment.Add(world->Arena().Create<mods::LineBuildDirectionInit>(
          mods::LineBuildDirection::X));
      dict_segment.Add(world->Arena().Create<mods::LineBuildParentInit>(
          std::vector<sim::Actor*>{sbag_node}));
      sim::Actor* sbag_segment =
          world->CreateActor(true, "sbag", dict_segment);
      world->Tick();
      Check(sbag_segment != nullptr, "p6b10: sbag segment spawned");
    }

    // ———— PrimaryBuilding:双 FACT 的主建筑互斥 ————
    // ———— PrimaryBuilding: the two-FACT primary mutual exclusion ————
    const game::ActorInfo* fact_info = map_world->Rules().FindActor("weap");
    const meta::RecordObject* rec_fact_building =
        fact_info != nullptr
            ? mods::FindTraitInfoRecord(*fact_info, "BuildingInfo")
            : nullptr;
    mods::BuildingInfoData fact_bi =
        rec_fact_building != nullptr
            ? mods::BuildingInfoData::Parse(*rec_fact_building)
            : mods::BuildingInfoData{};
    CPos site_fact_a{0, 0};
    CPos site_fact_b{0, 0};
    bool b_found_fact_sites = false;
    for (int dy = 0; dy < 14 && !b_found_fact_sites; dy++)
      for (int dx = 0; dx < 14 && !b_found_fact_sites; dx++) {
        const CPos c{cx + dx, cy + dy};
        if (mods::CanPlaceBuilding(*world, c, fact_info, fact_bi,
                                   nullptr) &&
            mods::CanPlaceBuilding(*world, c + CVec{0, 4}, fact_info,
                                   fact_bi, nullptr)) {
          site_fact_a = c;
          site_fact_b = c + CVec{0, 4};
          b_found_fact_sites = true;
        }
      }
    sim::Actor* fact_a = spawn("weap", player_a, site_fact_a);
    sim::Actor* fact_b = spawn("weap", player_a, site_fact_b);
    world->Tick();
    mods::PrimaryBuilding* primary_a =
        fact_a != nullptr ? fact_a->TraitOrDefault<mods::PrimaryBuilding>()
                          : nullptr;
    mods::PrimaryBuilding* primary_b =
        fact_b != nullptr ? fact_b->TraitOrDefault<mods::PrimaryBuilding>()
                          : nullptr;
    Check(primary_a != nullptr && primary_b != nullptr,
          "p6b10: both WEAPs carry PrimaryBuilding");
    if (primary_a != nullptr && primary_b != nullptr) {
      net::Order order_primary{"PrimaryProducer", fact_a, false};
      fact_a->ResolveOrder(order_primary);
      Check(primary_a->IsPrimary(), "p6b10: WEAP A primary set");
      net::Order order_primary_b{"PrimaryProducer", fact_b, false};
      fact_b->ResolveOrder(order_primary_b);
      world->Tick();
      Check(primary_b->IsPrimary() && !primary_a->IsPrimary(),
            "p6b10: WEAP B primary revokes A");
    }

    // FACT 的 Production@Building 使能 Building 队(ra 的生产主建筑)
    // FACT's Production@Building enables the Building queue (ra's
    // production main).
    CPos site_fact_prod{0, 0};
    bool b_found_fact_prod = false;
    for (int dy = 0; dy < 14 && !b_found_fact_prod; dy++)
      for (int dx = 0; dx < 14 && !b_found_fact_prod; dx++) {
        const CPos c{cx + dx, cy + dy};
        if (mods::CanPlaceBuilding(*world, c, map_world->Rules().FindActor("fact"),
                                   mods::BuildingInfoData::Parse(
                                       *mods::FindTraitInfoRecord(
                                           *map_world->Rules().FindActor("fact"),
                                           "BuildingInfo")),
                                   nullptr)) {
          site_fact_prod = c;
          b_found_fact_prod = true;
        }
      }
    sim::Actor* fact_prod = spawn("fact", player_a, site_fact_prod);
    Check(fact_prod != nullptr, "p6b10: production FACT spawned");

    // ———— PlaceBuilding:TENT 经 Building 队完成 + 放置 order 落地 ————
    // ———— PlaceBuilding: the TENT through the Building queue's completion
    //      + the placement order landing ————
    mods::ProductionQueue* queue_building = nullptr;
    for (auto* q :
         player_a->PlayerActor()->TraitsImplementing<mods::ProductionQueue>())
      if (q->Info().str_type == "Building")
        queue_building = q;
    Check(queue_building != nullptr,
          "p6b10: player ClassicProductionQueue@Building resolved");
    if (queue_building != nullptr && tent_info != nullptr) {
      world->Tick();
      Check(queue_building->Enabled(),
            "p6b10: Building queue enabled (FACT Production Building)");

      if (mods::PlayerResources* resources =
              player_a->PlayerActor()->Trait<mods::PlayerResources>())
        resources->GiveCash(10000);
      net::Order order_start = net::Order::StartProduction(
          player_a->PlayerActor(), "tent", 1, false);
      queue_building->ResolveOrder(*player_a->PlayerActor(), order_start);
      CheckEq(queue_building->AllQueued().size(), std::size_t{1},
              "p6b10: TENT queued");

      for (int tick = 0;
           tick < 3200 && !queue_building->AllQueued().empty() &&
               !queue_building->AllQueued().front()->Done();
           tick++)
        world->Tick();
      Check(!queue_building->AllQueued().empty() &&
                queue_building->AllQueued().front()->Done(),
            "p6b10: TENT item done (awaiting placement)");

      CPos site_place{0, 0};
      bool b_found_place = false;
      for (int dy = 0; dy < 14 && !b_found_place; dy++)
        for (int dx = 0; dx < 14 && !b_found_place; dx++) {
          const CPos c{cx + dx, cy + dy};
          if (mods::CanPlaceBuilding(*world, c, tent_info, tent_bi, nullptr)) {
            site_place = c;
            b_found_place = true;
          }
        }
      Check(b_found_place, "p6b10: placement site found");

      net::Order order_place{"PlaceBuilding", player_a->PlayerActor(),
                             sim::Target::FromCell(*world, site_place),
                             false};
      order_place.str_target_string = "tent";
      order_place.uint4_extra_data =
          queue_building->Actor()->ActorID();
      player_a->PlayerActor()->ResolveOrder(order_place);
      world->Tick();
      Check(queue_building->AllQueued().empty(),
            "p6b10: placement order ends production");

      std::size_t tent_count = 0;
      for (sim::Actor* a : world->Actors())
        if (a->Info()->Name() == "tent" && a->Owner() == player_a &&
            a->Location() == site_place)
          tent_count++;
      CheckEq(tent_count, std::size_t{1},
              "p6b10: placed TENT stands at the site");
    }

    // ———— ProductionIconOverlayManager(player actor;ra player.yaml)————
    // ———— ProductionIconOverlayManager (the player actor; ra player.yaml)
    //      ————
    mods::ProductionIconOverlayManager* overlay_manager = nullptr;
    for (auto* m : player_a->PlayerActor()
                       ->TraitsImplementing<
                           mods::ProductionIconOverlayManager>())
      overlay_manager = m;
    Check(overlay_manager != nullptr,
          "p6b10: ProductionIconOverlayManager resolved");
    if (overlay_manager != nullptr) {
      Check(overlay_manager->Info().str_type == "Veterancy",
            "p6b10: overlay type Veterancy (ra player.yaml)");
      Check(!overlay_manager->IsOverlayActive(*tent_info),
            "p6b10: overlay inactive without prerequisites");
      const core::Vector2 vec_offset =
          overlay_manager->OverlayOffset(core::Vector2{64.0f, 48.0f});
      CheckEq(vec_offset.X, -28.0f,
              "p6b10: overlay offset X = (8 - 64) / 2 (iconchevrons 8px)");
    }

    // ———— palette widget:icon 刷新 + BuildPaletteOrder 序 ————
    // ———— The palette widget: the icon refresh + the BuildPaletteOrder
    //      ordering ————
    ui::WidgetArgs args_palette;
    args_palette.Add("world", world.get());
    args_palette.Add("worldRenderer", &wr);
    mods::ProductionPaletteWidget widget_palette(args_palette);
    Check(widget_palette.LoadFieldOrProperty("Columns", "4") &&
              widget_palette.LoadFieldOrProperty("IconMargin", "2, 2"),
          "p6b10: palette fields load");
    Check(widget_palette.LoadFieldOrProperty("MaxIconRowOffset", "4"),
          "p6b10: MaxIconRowOffset loads (ClassicProductionLogic's setting)");
    ui::WidgetArgs args_palette_init;
    widget_palette.Initialize(args_palette_init);
    widget_palette.SetCurrentQueue(queue_building);
    widget_palette.Tick();
    const std::size_t n_icons = widget_palette.IconsForTest().size();
    const std::size_t n_buildables = widget_palette.AllBuildables().size();
    Check(n_icons > 0 &&
              n_icons == std::min(n_buildables, std::size_t{16}),
          "p6b10: refresh materializes one icon per buildable (row cap 4x4)");
    CheckEq(static_cast<std::size_t>(widget_palette.int4_total_icon_count),
            n_buildables,
            "p6b10: TotalIconCount tracks AllBuildables (uncapped)");
    if (n_buildables >= 2) {
      bool b_ordered = true;
      const auto& vec_icons = widget_palette.IconsForTest();
      int prev_order = -0x7fffffff;
      for (const auto& [rect, icon_entry] : vec_icons) {
        static_cast<void>(rect);
        int order = 0;
        if (const meta::RecordObject* rec_b = mods::FindTraitInfoRecord(
                *icon_entry.actor, "BuildableInfo"))
          order = mods::BuildableInfoData::Parse(*rec_b)
                      .int4_build_palette_order;
        if (order < prev_order)
          b_ordered = false;
        prev_order = order;
      }
      Check(b_ordered, "p6b10: icons ordered by BuildPaletteOrder");
      const Rectangle rect_first = widget_palette.IconsForTest().front().first;
      CheckEq(rect_first.Width, 64, "p6b10: icon rect width 64");
    }
    widget_palette.Tick();
    CheckEq(widget_palette.IconsForTest().size(), n_icons,
            "p6b10: Tick re-refreshes the same icon count");
    Check(widget_palette.GetCursor(int2{-4096, -4096}) == nullptr,
          "p6b10: GetCursor null outside every icon");

    // ———— tabs widget:规则组表 + TypeButton 字段面 ————
    // ———— The tabs widget: the rules group table + TypeButton's field
    //      face ————
    ui::WidgetArgs args_tabs;
    args_tabs.Add("world", world.get());
    mods::ProductionTabsWidget widget_tabs(args_tabs);
    Check(!widget_tabs.GroupsForTest().empty() &&
              widget_tabs.GroupsForTest().begin()->second.vec_tabs.empty(),
          "p6b10: tab groups gathered (ra queues share the null Group)");
    Check(widget_tabs.LoadFieldOrProperty("TabWidth", "24") &&
              widget_tabs.LoadFieldOrProperty("ArrowWidth", "16"),
          "p6b10: tabs fields load");
    ui::WidgetArgs args_tabs_init;
    widget_tabs.Initialize(args_tabs_init);
    Check(widget_tabs.TabsForTest().empty(),
          "p6b10: tabs empty before a queue group is selected");

    static ui::ChromeMetrics metrics_palette;
    {
      const auto dir_metrics = std::filesystem::temp_directory_path() /
                               "ora_render_test_metrics";
      std::filesystem::create_directories(dir_metrics);
      {
        std::ofstream out_metrics(dir_metrics / "metrics.yaml");
        out_metrics << "Chrome:\n";
        out_metrics << "\tButtonDepth: 1\n";
        out_metrics << "\tButtonFont: Regular\n";
        out_metrics << "\tButtonTextColor: FFFFFFFF\n";
        out_metrics << "\tButtonTextColorDisabled: 808080FF\n";
        out_metrics << "\tButtonTextContrast: True\n";
        out_metrics << "\tButtonTextShadow: False\n";
        out_metrics << "\tButtonTextContrastColorDark: 000000FF\n";
        out_metrics << "\tButtonTextContrastColorLight: FFFFFFFF\n";
        out_metrics << "\tButtonTextContrastRadius: 1\n";
        out_metrics << "\tClickSound: click_sound\n";
        out_metrics << "\tClickDisabledSound: click_disabled_sound\n";
        out_metrics << "\tButtonCursor: default\n";
        out_metrics << "\tWorldDefaultCursor: default\n";
        out_metrics << "\tInfiniteOffset: 4, 2\n";
      }
      fs::FileSystem fs_metrics;
      fs_metrics.Mount(dir_metrics.generic_string());
      const std::vector<std::string> vec_files_metrics{"metrics.yaml"};
      metrics_palette.Initialize({&fs_metrics, &vec_files_metrics});

      ui::Ui::Deps deps_metrics;
      deps_metrics.ptr_chrome_metrics = &metrics_palette;
      deps_metrics.fn_window_size = [] { return int2{1280, 720}; };
      ui::Ui::Instance().Initialize(std::move(deps_metrics));
    }
    core::Vector2 vec_infinite_check{};
    Check(metrics_palette.TryGetVector2("InfiniteOffset", vec_infinite_check) &&
              vec_infinite_check.X == 4.0f && vec_infinite_check.Y == 2.0f,
          "p6b10: ChromeMetrics TryGetVector2 parses");
    ui::WidgetArgs args_type_button;
    args_type_button.Add("world", world.get());
    mods::ProductionTypeButtonWidget widget_type_button(args_type_button);
    Check(widget_type_button.LoadFieldOrProperty("ProductionGroup",
                                                 "Infantry") &&
              widget_type_button.ProductionGroup() == "Infantry",
          "p6b10: type button ProductionGroup loads");
    auto widget_type_clone = widget_type_button.Clone();
    Check(widget_type_clone->TypeName() ==
              std::string_view{"ProductionTypeButtonWidget"},
          "p6b10: type button clone type");
  }

  mods::SupportPowerManager* spm = nullptr;

  // ———— 第十一批:Radar 链 + SupportPowers 面 + PlayerStatistics/Tooltip ————
  // ———— Batch 11: the radar chain + the SupportPowers face +
  //      PlayerStatistics/Tooltip ————
  {
    // RadarPings:三点星的收缩/旋转/到期排水 + LastPingPosition
    // RadarPings: the star's shrink/rotate/expiry drain + LastPingPosition.
    mods::RadarPingsInfoData info_pings;
    info_pings.int4_from_radius = 200;
    info_pings.int4_to_radius = 15;
    info_pings.int4_shrink_speed = 4;
    info_pings.fp4_rotation_speed = 0.12f;
    mods::RadarPings pings{info_pings};
    mods::RadarPing& ping_visible =
        pings.Add([] { return true; }, WPos{1024, 1024, 0},
                  core::Color::FromArgb(255, 255, 0, 0), 5);
    Check(pings.LastPingPosition().has_value() &&
              pings.LastPingPosition()->X == 1024,
          "p6b11: visible ping records LastPingPosition");
    pings.Add([] { return false; }, WPos{2048, 2048, 0},
              core::Color::FromArgb(255, 0, 255, 0), 3);
    Check(pings.Pings().size() == 2, "p6b11: two pings queued");
    Check(ping_visible.Tick(), "p6b11: ping tick 1 keeps alive");
    const auto vec_points = ping_visible.Points(int2{50, 60});
    Check(std::abs(vec_points[0].X - (50 + 196 * std::sin(-0.12f))) < 0.01f &&
              std::abs(vec_points[0].Y - (60 + 196 * std::cos(-0.12f))) < 0.01f,
          "p6b11: star point 0 = center + 196*(sin,cos)(-0.12)");
    int int4_manager_ticks = 0;
    while (!pings.Pings().empty() && int4_manager_ticks < 10) {
      pings.Tick(*world->WorldActor());
      int4_manager_ticks++;
      if (int4_manager_ticks == 3)
        Check(pings.Pings().size() == 1,
              "p6b11: short invisible ping drains at its duration");
    }
    Check(int4_manager_ticks == 4 && pings.Pings().empty(),
          "p6b11: 5-tick ping drains by the 4th manager tick");

    // AppearsOnRadar:占用格签名 + OwnerColor
    // AppearsOnRadar: the occupied-cell signature + OwnerColor.
    sim::Actor* jeep_a = spawn("jeep", player_a, CPos{cx, cy});
    auto* appears = jeep_a->TraitOrDefault<mods::AppearsOnRadar>();
    Check(appears != nullptr, "p6b11: jeep carries AppearsOnRadar");
    if (appears != nullptr) {
      std::vector<std::pair<CPos, core::Color>> vec_cells;
      appears->PopulateRadarSignatureCells(*jeep_a, vec_cells);
      Check(vec_cells.size() == 1 && vec_cells[0].first == jeep_a->Location(),
            "p6b11: jeep radar signature cell = its location");
      Check(vec_cells[0].second == player_a->Color(),
            "p6b11: signature color = owner color");
    }

    // PlayerRadarTerrain::GetColor 的空层回落 = map.GetTerrainColorPair
    // PlayerRadarTerrain::GetColor's empty-layer fallback = the map pair.
    {
      const MPos uv_probe{0, 0};
      const auto pair_static =
          mods::PlayerRadarTerrain::GetColor(world->Map(), {}, uv_probe);
      const auto pair_map = world->Map().GetTerrainColorPair(uv_probe);
      Check(pair_static.first == pair_map.first.ToArgb() &&
                pair_static.second == pair_map.second.ToArgb(),
            "p6b11: radar terrain color = map terrain pair");
    }

    // Player actor 的 SupportPowerManager
    // The player actor's SupportPowerManager.
    spm = player_a->PlayerActor()->TraitOrDefault<mods::SupportPowerManager>();
    Check(spm != nullptr, "p6b11: player actor carries SupportPowerManager");
    Check(spm->DevMode() != nullptr && spm->GetTechTree() != nullptr,
          "p6b11: manager resolves DeveloperMode + TechTree");

    // SupportPowerInstance 状态机(MSLO 的 NukePowerInfo 记录)
    // The SupportPowerInstance state machine (MSLO's NukePowerInfo record).
    const meta::RecordObject* rec_nuke = nullptr;
    for (const auto& [str_name, info_actor] : world->Map().Rules().Actors())
      if (str_name == std::string{"mslo"})
        for (const meta::RecordObject* rec :
             info_actor->TraitsInConstructOrder())
          if (rec->record_desc().str_full_name ==
              std::string_view{
                  "OpenRA.Mods.Common.Traits.NukePowerInfo"}) {
            rec_nuke = rec;
            break;
          }
    Check(rec_nuke != nullptr, "p6b11: MSLO NukePowerInfo record found");
    if (rec_nuke != nullptr && spm != nullptr) {
      sim::TypeDictionary dict_power;
      sim::ActorInitializer init_power{*player_a->PlayerActor(), dict_power};
      auto up_power = std::make_unique<TestPower>(
          init_power, mods::SupportPowerInfoData::Parse(*rec_nuke));
      TestPower* power = up_power.get();

      Check(power->Info().str_order_name == "NukePowerInfoOrder",
            "p6b11: OrderName ctor default = TypeName + Order");
      Check(power->Info().int4_charge_interval == 13500,
            "p6b11: MSLO ChargeInterval 13500 parses");

      auto up_instance =
          power->CreateInstance(power->Info().str_order_name, *spm);
      mods::SupportPowerInstance* instance = up_instance.get();
      instance->Instances().push_back(power);

      Check(instance->TotalTicks() == 13500 &&
                instance->RemainingTicks() == 13500,
            "p6b11: instance starts uncharged");
      Check(instance->Disabled(),
            "p6b11: disabled before the first Tick enables the instances");
      instance->Tick();
      Check(!instance->Disabled() && instance->Active(),
            "p6b11: one enabled instance activates");
      Check(instance->RemainingTicks() == 13499,
            "p6b11: charge decrements one tick per Tick");
      instance->Tick();
      Check(instance->RemainingTicks() == 13498,
            "p6b11: second tick keeps decrementing");
      instance->ResetTimer();
      Check(instance->RemainingTicks() == 13500,
            "p6b11: ResetTimer restores the full charge");

      spm->Powers().emplace(instance->Key(), std::move(up_instance));
      net::Order order_activate{instance->Key(), player_a->PlayerActor(),
                                false};
      spm->ResolveOrder(*player_a->PlayerActor(), order_activate);
      Check(spm->FindPower(instance->Key()) != nullptr,
            "p6b11: manager dispatches the activation order");
      Check(spm->GetPowersForActor(*player_a->PlayerActor()).empty(),
            "p6b11: player actor itself matches no power instances");

      // SelectGenericPowerTarget:落点 order + 取消态 + 光标
      // SelectGenericPowerTarget: the placement order + the cancel
      // transition + the cursor.
      Check(mods::SelectGenericPowerTarget(
                instance->Key(), *spm, power->Info())
                .GetCursor(*world, CPos{cx, cy}, int2{}, MouseInput{}) ==
                power->Info().str_cursor,
            "p6b11: in-map cursor = the power cursor");
      Check(mods::SelectGenericPowerTarget(
                instance->Key(), *spm, power->Info())
                .GetCursor(*world, CPos{-500, -500}, int2{}, MouseInput{}) ==
                power->Info().str_blocked_cursor,
            "p6b11: out-of-map cursor = the blocked cursor");
      auto up_selector =
          std::make_unique<mods::SelectGenericPowerTarget>(
              instance->Key(), *spm, power->Info());
      mods::SelectGenericPowerTarget* selector = up_selector.get();
      sim::IOrderGenerator* selector_ptr = up_selector.get();
      world->AdoptOrderGenerator(std::move(up_selector));
      MouseInput mi_down{};
      mi_down.Event = MouseInputEvent::Down;
      mi_down.Button = MouseButton::Left;
      const auto vec_orders =
          world->OrderGenerator()->Order(*world, CPos{cx, cy}, int2{},
                                         mi_down);
      Check(vec_orders.size() == 1 &&
                vec_orders[0]->str_order_string == instance->Key() &&
                vec_orders[0]->b_suppress_visual_feedback,
            "p6b11: generic target order emits with feedback suppressed");
      Check(world->OrderGenerator() != selector_ptr,
            "p6b11: OrderInner cancels the input mode");

      spm->Powers().erase(instance->Key());
    }

    // PlayerStatistics + UpdatesPlayerStatistics:击杀记账
    // PlayerStatistics + UpdatesPlayerStatistics: kill accounting.
    mods::PlayerStatistics* stats_a =
        player_a->PlayerActor()->TraitOrDefault<mods::PlayerStatistics>();
    mods::PlayerStatistics* stats_b =
        player_b->PlayerActor()->TraitOrDefault<mods::PlayerStatistics>();
    Check(stats_a != nullptr && stats_b != nullptr,
          "p6b11: player actors carry PlayerStatistics");
    if (stats_a != nullptr && stats_b != nullptr) {
      
      mods::ArmyUnit& e1_unit = stats_a->Unit("e1");
      Check(e1_unit.ActorInfo() != nullptr &&
                e1_unit.BuildableInfo().has_value() &&
                e1_unit.Icon() != nullptr,
            "p6b11: ArmyUnit e1 materializes with icon + buildable");
    }
  }

    // ———— 第十二批:具体 support power(Nuke/SpawnActor/IRON)—— 
    // The batch-12 concrete support powers (Nuke/SpawnActor/IRON).
    {
      const auto find_record =
          [world = world.get()](
              std::string_view str_actor,
              std::string_view str_full_name) -> const meta::RecordObject* {
        for (const auto& [name, info] : world->Map().Rules().Actors())
          if (name == str_actor)
            for (const meta::RecordObject* rec :
                 info->TraitsInConstructOrder())
              if (rec->record_desc().str_full_name == str_full_name)
                return rec;
        return nullptr;
      };

      // NukePowerInfo:MSLO 的字段面
      // NukePowerInfo: MSLO's field face.
      const meta::RecordObject* rec_mslo =
          find_record("mslo", "OpenRA.Mods.Common.Traits.NukePowerInfo");
      Check(rec_mslo != nullptr, "p6b12: MSLO NukePowerInfo record found");
      mods::NukePowerInfoData nuke_info_data;
      if (rec_mslo != nullptr) {
        nuke_info_data =
            mods::NukePowerInfoData::Parse(*rec_mslo, *world);
        Check(nuke_info_data.str_missile_weapon == "atomic" &&
                  nuke_info_data.ptr_weapon_info != nullptr,
              "p6b12: NukeMissileWeapon atomic resolves against the rules");
        Check(nuke_info_data.int4_missile_delay == 5,
              "p6b12: MissileDelay 5 parses");
        Check(nuke_info_data.wvec_spawn_offset == WVec{1024, 427, 0},
              "p6b12: SpawnOffset 1c0,427,0 parses");
        Check(nuke_info_data.dist_camera_range == WDist{10240},
              "p6b12: CameraRange 10c0 parses");
        Check(nuke_info_data.str_beacon_poster == "atomicon" &&
                  nuke_info_data.b_display_beacon &&
                  nuke_info_data.b_display_radar_ping,
              "p6b12: beacon poster + display flags parse");
        Check(!nuke_info_data.b_has_circle_ranges,
              "p6b12: absent CircleRanges stays null");
        Check(nuke_info_data.str_order_name == "NukePowerInfoOrder",
              "p6b12: NukePowerInfo OrderName default");
        Check(nuke_info_data.int4_flight_delay == 400 &&
                  nuke_info_data.dist_flight_velocity == WDist{512},
              "p6b12: FlightDelay/FlightVelocity defaults");
      }

      // NukeLaunch 弹道:turn 中点 + 落地爆点 + 帧末移除
      // The NukeLaunch trajectory: the turn midpoint + the impact + the
      // frame-end removal.
      const game::WeaponInfo* weapon_carbine = nullptr;
      for (const auto& [name, weapon] : world->Map().Rules().Weapons())
        if (name == "m1carbine")
          weapon_carbine = weapon.get();
      Check(weapon_carbine != nullptr, "p6b12: M1Carbine weapon resolved");
      if (weapon_carbine != nullptr) {
        const WPos pos_launch{cx * 1024, cy * 1024, 0};
        const WPos pos_target{(cx + 6) * 1024, cy * 1024, 0};
        auto up_launch = std::make_unique<mods::NukeLaunch>(
            *player_a, "", *weapon_carbine, "effect", "up", "down",
            pos_launch, pos_target, WDist{1}, false, WDist{0}, 0, 40,
            false, "", std::vector<std::string>{}, "", false, 0, 1);
        mods::NukeLaunch* launch = up_launch.get();
        world->Add(std::move(up_launch));
        Check(launch->PositionForTest() == pos_launch,
              "p6b12: ascending launch starts at the launch position");
        world->Tick();
        Check(launch->PositionForTest() == pos_launch,
              "p6b12: zero launch delay fires this tick at the source");
        Check(std::abs(launch->FractionComplete() - 0.025f) < 1e-6f,
              "p6b12: FractionComplete = ticks / impactDelay");
        for (int i = 0; i < 20; i++)
          world->Tick();
        Check(launch->DetonatedForTest(),
              "p6b12: detonation altitude triggers the explosion");
        Check(launch->InWorldForTest(),
              "p6b12: removeOnDetonation false keeps the projectile");
        Check(launch->PositionForTest() == pos_target,
              "p6b12: the turn point sits at the target when velocity is 0");
      }

      // SpawnActorPower:sonarpulse 的字段面 + 地形验证 + 出生/到期
      // SpawnActorPower: sonarpulse's field face + the terrain validation +
      // the spawn/expiry.
      const meta::RecordObject* rec_sonar =
          find_record("powerproxy.sonarpulse",
                      "OpenRA.Mods.Common.Traits.SpawnActorPowerInfo");
      Check(rec_sonar != nullptr,
            "p6b12: powerproxy.sonarpulse SpawnActorPowerInfo found");
      mods::SpawnActorPowerInfoData spawn_info_data;
      if (rec_sonar != nullptr) {
        spawn_info_data =
            mods::SpawnActorPowerInfoData::Parse(*rec_sonar);
        Check(spawn_info_data.str_actor == "sonar" &&
                  spawn_info_data.int4_life_time == 250,
              "p6b12: SpawnActor actor/LifeTime parse");
        Check(spawn_info_data.b_has_terrain &&
                  spawn_info_data.vec_terrain.size() == 1 &&
                  spawn_info_data.vec_terrain[0] == "Water",
              "p6b12: SpawnActor Terrain Water parses");
        Check(!spawn_info_data.b_allow_under_shroud &&
                  spawn_info_data.str_deploy_sound == "sonpulse.aud",
              "p6b12: AllowUnderShroud false + DeploySound parse");
      }

      sim::Actor* sonar_proxy =
          spawn("powerproxy.sonarpulse", player_a, CPos{cx, cy});
      auto* spawn_power =
          sonar_proxy != nullptr
              ? sonar_proxy->TraitOrDefault<mods::SpawnActorPower>()
              : nullptr;
      Check(spawn_power != nullptr,
            "p6b12: powerproxy.sonarpulse constructs SpawnActorPower");
      if (spawn_power != nullptr) {
        Check(spawn_power->Validate(*world, CPos{cx, cy}) == false,
              "p6b12: land cell fails the Water terrain gate");
        std::optional<CPos> opt_water;
        for (int y = bounds.Top(); y < bounds.Bottom() && !opt_water;
             y++)
          for (int x = bounds.Left(); x < bounds.Right(); x++)
            if (world->Map().GetTerrainInfo(CPos{x, y}).Type == "Water") {
              opt_water = CPos{x, y};
              break;
            }
        if (opt_water.has_value()) {
          Check(spawn_power->Validate(*world, *opt_water),
                "p6b12: water cell passes the Water terrain gate");
          net::Order order_spawn{
              "SpawnActorPowerInfoOrder", sonar_proxy,
              sim::Target::FromCell(*world, *opt_water), false};
          spawn_power->Activate(*sonar_proxy, order_spawn, *spm);
          world->Tick();
          sim::Actor* sonar_actor = nullptr;
          for (sim::Actor* a : world->Actors())
            if (a->Info() != nullptr && a->Info()->Name() == "sonar")
              sonar_actor = a;
          Check(sonar_actor != nullptr,
                "p6b12: SpawnActorPower creates the sonar actor");
          if (sonar_actor != nullptr) {
            for (int i = 0; i < 260 && sonar_actor->IsInWorld(); i++)
              world->Tick();
            Check(!sonar_actor->IsInWorld(),
                  "p6b12: LifeTime Wait then RemoveSelf removes it");
          }
        }
      }

      // GrantExternalConditionPower:IRON 的字段面 + footprint 展开
      // GrantExternalConditionPower: IRON's field face + the footprint.
      const meta::RecordObject* rec_iron =
          find_record(
              "iron",
              "OpenRA.Mods.Common.Traits.GrantExternalConditionPowerInfo");
      Check(rec_iron != nullptr, "p6b12: IRON condition power record found");
      if (rec_iron != nullptr) {
        const mods::GrantExternalConditionPowerInfoData ic_info =
            mods::GrantExternalConditionPowerInfoData::Parse(*rec_iron);
        Check(ic_info.str_condition == "invulnerability" &&
                  ic_info.int4_duration == 400,
              "p6b12: condition/duration parse");
        Check(ic_info.cvec_dimensions == CVec{3, 3} &&
                  ic_info.kind_valid_relationships ==
                      sim::PlayerRelationship::Ally,
              "p6b12: Dimensions 3,3 + the Ally default");
        std::vector<char> vec_footprint;
        for (const char c : ic_info.str_footprint)
          if (!std::isspace(static_cast<unsigned char>(c)))
            vec_footprint.push_back(c);
        const std::vector<CPos> vec_cells = mods::SupportPower::CellsMatching(
            CPos{10, 10}, vec_footprint, ic_info.cvec_dimensions);
        Check(vec_cells.size() == 5 &&
                  vec_cells[0] == CPos{10, 9} &&
                  vec_cells[1] == CPos{9, 10} &&
                  vec_cells[2] == CPos{10, 10} &&
                  vec_cells[3] == CPos{11, 10} &&
                  vec_cells[4] == CPos{10, 11},
              "p6b12: '_x_ xxx _x_' expands to the five cross cells");
      }

      sim::Actor* iron = spawn("iron", player_a, CPos{cx, cy});
      auto* ic_power =
          iron != nullptr
              ? iron->TraitOrDefault<mods::GrantExternalConditionPower>()
              : nullptr;
      Check(ic_power != nullptr, "p6b12: IRON carries the condition power");
      if (ic_power != nullptr) {
        sim::Actor* jeep_ally =
            spawn("jeep", player_a, CPos{cx + 5, cy + 4});
        sim::Actor* jeep_enemy =
            spawn("jeep", player_b, CPos{cx + 5, cy + 6});
        Check(jeep_ally != nullptr && jeep_enemy != nullptr,
              "p6b12: ally + enemy jeeps spawned");
        const std::vector<sim::Actor*> vec_in_range =
            ic_power->UnitsInRange(CPos{cx + 5, cy + 5});
        const bool b_has_ally =
            std::find(vec_in_range.begin(), vec_in_range.end(), jeep_ally) !=
            vec_in_range.end();
        const bool b_has_enemy =
            std::find(vec_in_range.begin(), vec_in_range.end(),
                      jeep_enemy) != vec_in_range.end();
        Check(b_has_ally && !b_has_enemy,
              "p6b12: UnitsInRange keeps allies and filters enemies");
      }

      // SelectNukePowerTarget:CircleRanges 注释圈
      // SelectNukePowerTarget: the CircleRanges annotations.
      if (spm != nullptr) {
        mods::NukePowerInfoData circle_info = nuke_info_data;
        circle_info.b_has_circle_ranges = true;
        circle_info.vec_circle_ranges = {WDist{2048}, WDist{3072}};
        mods::SelectNukePowerTarget selector{"NukePowerInfoOrder", *spm,
                                             circle_info};
        class CircleViewportStub final : public gfx::IViewportSurface {
         public:
          int2 WorldToViewPx(int2 int2_world) override {
            return int2_world;
          }
          int2 WorldToViewPx(const core::Vector3& vec_world) override {
            return int2{static_cast<std::int32_t>(vec_world.X),
                        static_cast<std::int32_t>(vec_world.Y)};
          }
          Rectangle GetScissorBounds(bool) override { return Rectangle{}; }
          int2 TopLeft() override { return int2{}; }
          int2 BottomRight() override { return int2{}; }
          CPos ViewToWorld(int2 int2_view) override {
            return CPos{int2_view.X / 1024, int2_view.Y / 1024};
          }
          int2 ViewToWorldPx(int2 int2_view) override { return int2_view; }
          WPos CenterPosition() const override { return WPos{}; }
        };
        CircleViewportStub viewport_circle;
        wr.SetViewport(&viewport_circle);
        gfx::Viewport::int2_last_mouse_pos = int2{10 * 1024, 20 * 1024};
        const auto vec_annotations =
            selector.RenderAnnotations(wr, *world);
        Check(vec_annotations.size() == 2,
              "p6b12: two circle ranges yield two annotations");
        if (vec_annotations.size() == 2) {
          Check(vec_annotations[0]->Pos() ==
                    world->Map().CenterOfCell(CPos{10, 20}),
                "p6b12: circle centered on the last mouse cell");
        }
        wr.SetViewport(nullptr);
      }

      // Beacon:箭头高度振荡 + duration 到期移除
      // Beacon: the arrow-height oscillation + the duration expiry.
      {
        auto up_beacon = std::make_unique<mods::Beacon>(
            *player_a, WPos{cx * 1024, cy * 1024, 0}, 10, "player", false,
            "beacon", "", "arrow", "");
        mods::Beacon* beacon = up_beacon.get();
        world->Add(std::move(up_beacon));
        world->Tick();
        Check(beacon->ArrowHeightForTest() == 512,
              "p6b12: first beacon tick clamps the arrow at max");
        world->Tick();
        Check(beacon->ArrowHeightForTest() == 462,
              "p6b12: the arrow descends by 50 per tick after the flip");
      }
    }

    // ———— 第十三批:空中机动链(Aircraft/Fly 族/AmmoPool/Airstrike)———
    // The batch-13 air-mobility chain (Aircraft/the Fly family/AmmoPool/
    // Airstrike).
    {
      const auto find_record =
          [world = world.get()](
              std::string_view str_actor,
              std::string_view str_full_name) -> const meta::RecordObject* {
        for (const auto& [name, info] : world->Map().Rules().Actors())
          if (name == str_actor)
            for (const meta::RecordObject* rec :
                 info->TraitsInConstructOrder())
              if (rec->record_desc().str_full_name == str_full_name)
                return rec;
        return nullptr;
      };

      const meta::RecordObject* rec_mig_air =
          find_record("mig", "OpenRA.Mods.Common.Traits.AircraftInfo");
      Check(rec_mig_air != nullptr, "p6b13: MIG AircraftInfo record found");
      mods::AircraftInfoData mig_air;
      if (rec_mig_air != nullptr) {
        mig_air = mods::AircraftInfoData::Parse(*rec_mig_air);
        Check(mig_air.dist_cruise_altitude == WDist{2560} &&
                  mig_air.angle_turn_speed.Angle == 16 &&
                  mig_air.int4_speed == 223,
              "p6b13: MIG CruiseAltitude/TurnSpeed/Speed parse");
        Check(mig_air.str_airborne_condition == "airborne" &&
                  !mig_air.b_can_hover && mig_air.b_take_off_on_creation,
              "p6b13: MIG inherits ^NeutralPlane's airborne faces");
        Check(mig_air.angle_maximum_pitch.Angle == 56,
              "p6b13: MIG MaximumPitch 56 parses");
      }

      const meta::RecordObject* rec_mig_ammo =
          find_record("mig", "OpenRA.Mods.Common.Traits.AmmoPoolInfo");
      Check(rec_mig_ammo != nullptr, "p6b13: MIG AmmoPoolInfo record found");
      if (rec_mig_ammo != nullptr) {
        const mods::AmmoPoolInfoData mig_ammo =
            mods::AmmoPoolInfoData::Parse(*rec_mig_ammo);
        Check(mig_ammo.int4_ammo == 8 && mig_ammo.str_ammo_condition == "ammo",
              "p6b13: MIG Ammo 8 + AmmoCondition ammo parse");
      }

      const meta::RecordObject* rec_mig_rearm =
          find_record("mig", "OpenRA.Mods.Common.Traits.RearmableInfo");
      Check(rec_mig_rearm != nullptr,
            "p6b13: MIG RearmableInfo record found");
      if (rec_mig_rearm != nullptr) {
        const mods::RearmableInfoData mig_rearm =
            mods::RearmableInfoData::Parse(*rec_mig_rearm);
        Check(mig_rearm.vec_rearm_actors.size() == 2 &&
                  mig_rearm.vec_rearm_actors[0] == "afld",
              "p6b13: MIG RearmActors afld,afld.ukraine parse");
      }

      Check(mods::activities::Fly::CalculateTurnRadius(223, WAngle{16}) ==
                    180 * 223 / 16 &&
                mods::activities::Fly::CalculateTurnRadius(223, WAngle{0}) ==
                    0,
            "p6b13: CalculateTurnRadius follows 180*speed/turn");

      const auto mig_spawn = [&](CPos cell) {
        sim::TypeDictionary dict;
        sim::LocationInit location{cell};
        sim::OwnerInit owner_init{player_a};
        dict.Add(&location);
        dict.Add(&owner_init);
        return world->CreateActor(true, "mig", dict);
      };

      sim::Actor* mig = mig_spawn(CPos{cx + 3, cy + 3});
      Check(mig != nullptr, "p6b13: MIG spawns into the world");
      mods::Aircraft* mig_aircraft =
          mig != nullptr ? mig->Trait<mods::Aircraft>() : nullptr;
      if (mig_aircraft != nullptr) {
        Check(mig_aircraft->CenterPosition() ==
                  world->Map().CenterOfCell(CPos{cx + 3, cy + 3}),
              "p6b13: MIG ctor SetPosition anchors the center");

        Check(mig_aircraft->OccupiedCells().empty(),
              "p6b13: airborne MIG holds no landing influence");

        Check(!mig_aircraft->CanLand(CPos{cx + 3, cy + 3}),
              "p6b13: MIG's empty LandableTerrainTypes rejects bare terrain");
        sim::Actor* afld =
            spawn("afld", player_a, CPos{cx + 3, cy + 5});
        Check(afld != nullptr, "p6b13: afld spawns for the docking face");
        if (afld != nullptr)
          Check(mig_aircraft->CanLand(CPos{cx + 3, cy + 5}, afld),
                "p6b13: docking at the afld ignores terrain type");

        auto* ammo_pool = mig->Trait<mods::AmmoPool>();
        Check(ammo_pool->CurrentAmmoCount() == 8 && ammo_pool->HasFullAmmo(),
              "p6b13: MIG spawns with a full ammo pool");
        ammo_pool->TakeAmmo(*mig, 3);
        Check(ammo_pool->CurrentAmmoCount() == 5 && !ammo_pool->HasFullAmmo(),
              "p6b13: TakeAmmo drains to 5");
        Check(ammo_pool->GiveAmmo(*mig, 2) &&
                  ammo_pool->CurrentAmmoCount() == 7,
              "p6b13: GiveAmmo refills to 7");

        Check(mig->CurrentActivity() == nullptr,
              "p6b13: map-spawned MIG has no creation activity");

        net::Order order_move{
            "Move", mig,
            sim::Target::FromCell(*world, world->Map().Clamp(CPos{cx, cy})),
            false};
        mig->ResolveOrder(order_move);
        Check(mig->CurrentActivity() != nullptr,
              "p6b13: the Move order queues the Fly activity");

        for (int i = 0; i < 300; i++)
          world->Tick();
        Check(mig_aircraft->CenterPosition().Z >= 2560,
              "p6b13: the landed MIG takes off and climbs to 2560");

        mods::AmmoPool* pool_probe = mig->Trait<mods::AmmoPool>();
        Check(pool_probe != nullptr &&
                  pool_probe->Info().int4_reload_delay == 50,
              "p6b13: AmmoPool default ReloadDelay 50");
      }

      sim::Actor* mig2 = mig_spawn(CPos{cx + 6, cy + 6});
      if (mig2 != nullptr && mig_aircraft != nullptr) {
        mods::Aircraft* mig2_aircraft = mig2->Trait<mods::Aircraft>();
        auto* mig2_fly = world->Arena().Create<mods::activities::Fly>(
            *mig2, sim::Target::FromCell(
                       *world, world->Map().Clamp(CPos{cx, cy})),
            WDist::FromCells(1));
        mig2->QueueActivity(mig2_fly);
        const auto wpos_start = mig2_aircraft->CenterPosition();
        for (int i = 0; i < 200; i++)
          world->Tick();
        const auto wpos_end = mig2_aircraft->CenterPosition();
        Check((wpos_end - wpos_start).HorizontalLengthSquared() > 0,
              "p6b13: Fly moves the aircraft horizontally");
      }

      const meta::RecordObject* rec_badr_bomber =
          find_record("badr.bomber",
                      "OpenRA.Mods.Common.Traits.AttackBomberInfo");
      Check(rec_badr_bomber != nullptr,
            "p6b13: badr.bomber AttackBomberInfo record found");
      if (rec_badr_bomber != nullptr) {
        const mods::AttackBomberInfoData badr_bomber =
            mods::AttackBomberInfoData::Parse(*rec_badr_bomber);
        Check(badr_bomber.angle_facing_tolerance.Angle == 8,
              "p6b13: badr.bomber FacingTolerance 8 parses");
      }

      const meta::RecordObject* rec_parabombs = find_record(
          "powerproxy.parabombs",
          "OpenRA.Mods.Common.Traits.AirstrikePowerInfo");
      Check(rec_parabombs != nullptr,
            "p6b13: parabombs AirstrikePowerInfo record found");
      mods::AirstrikePowerInfoData parabombs;
      if (rec_parabombs != nullptr) {
        parabombs = mods::AirstrikePowerInfoData::Parse(*rec_parabombs);
        Check(parabombs.str_unit_type == "badr.bomber" &&
                  parabombs.int4_squad_size == 1,
              "p6b13: parabombs UnitType badr.bomber + SquadSize 1");
        Check(parabombs.int4_quantized_facings == 8 &&
                  parabombs.str_camera_actor == "camera" &&
                  parabombs.int4_camera_remove_delay == 150,
              "p6b13: parabombs QuantizedFacings/Camera parse");
        Check(parabombs.b_one_shot && parabombs.b_allow_multiple,
              "p6b13: parabombs OneShot + AllowMultiple parse");
        Check(parabombs.b_display_beacon &&
                  parabombs.str_beacon_poster == "pbmbicon",
              "p6b13: parabombs beacon faces parse");
      }

      if (spm != nullptr) {
        sim::Actor* parabombs_proxy =
            spawn("powerproxy.parabombs", player_a, CPos{cx, cy});
        auto* strike_power =
            parabombs_proxy != nullptr
                ? parabombs_proxy->TraitOrDefault<mods::AirstrikePower>()
                : nullptr;
        Check(strike_power != nullptr,
              "p6b13: powerproxy.parabombs constructs AirstrikePower");
        if (strike_power != nullptr) {
          auto vec_sent = strike_power->SendAirstrike(
              *parabombs_proxy,
              world->Map().CenterOfCell(CPos{cx, cy}), WAngle{0});
          Check(vec_sent.size() == 1,
                "p6b13: SendAirstrike spawns the single bomber");
          if (vec_sent.size() == 1) {
            Check(!vec_sent[0]->IsInWorld(),
                  "p6b13: the bomber waits outside the world pre-frame-end");
            world->Tick();
            Check(vec_sent[0]->IsInWorld(),
                  "p6b13: the frame-end task adds the bomber");
            mods::AttackBomber* bomber =
                vec_sent[0]->Trait<mods::AttackBomber>();
            Check(bomber != nullptr && bomber->BomberTarget().Type() ==
                                            sim::TargetType::Terrain,
                  "p6b13: the bomber carries the scripted target");
            const auto wpos_bomber_start = vec_sent[0]->CenterPosition();
            for (int i = 0; i < 60; i++)
              world->Tick();
            Check((vec_sent[0]->CenterPosition() - wpos_bomber_start)
                          .HorizontalLengthSquared() > 0,
                  "p6b13: the bomber flies its ingress");
          }
        }
      }

      const meta::RecordObject* rec_falls =
          find_record("mig.husk",
                      "OpenRA.Mods.Common.Traits.FallsToEarthInfo");
      if (rec_falls != nullptr) {
        mods::FallsToEarthInfoData falls_info =
            mods::FallsToEarthInfoData::Parse(*rec_falls);
        const game::ActorInfo* husk_info = nullptr;
        for (const auto& [name, info] : world->Map().Rules().Actors())
          if (name == "mig.husk")
            husk_info = info.get();
        if (husk_info != nullptr)
          falls_info.RulesetLoaded(world->Map().Rules(), *husk_info);
        Check(falls_info.ptr_explosion_weapon != nullptr,
              "p6b13: husk explosion weapon resolves");
      }

      sim::Actor* mig3 = mig_spawn(CPos{cx + 9, cy + 9});
      if (mig3 != nullptr && mig_aircraft != nullptr) {
        mods::Aircraft* mig3_aircraft = mig3->Trait<mods::Aircraft>();
        mig3_aircraft->SetPosition(
            mig3, WPos{(cx + 9) * 1024 + 512, (cy + 9) * 1024 + 512, 2400});
        mods::FallsToEarthInfoData falls_fall{};
        auto* fall = world->Arena().Create<mods::activities::FallToEarth>(
            *mig3, falls_fall);
        mig3->QueueActivity(false, fall);
        const auto int4_z_start = mig3_aircraft->CenterPosition().Z;
        world->Tick();
        Check(mig3_aircraft->CenterPosition().Z < int4_z_start,
              "p6b13: FallToEarth drags the aircraft down");
      }
    }

    // ———— 第十四批:运输链(Cargo/Passenger/ParaDrop/Parachutable/
    // WithParachute/ParatroopersPower)————
    {
      const auto find_record =
          [world = world.get()](
              std::string_view str_actor,
              std::string_view str_full_name) -> const meta::RecordObject* {
        for (const auto& [name, info] : world->Map().Rules().Actors())
          if (name == str_actor)
            for (const meta::RecordObject* rec :
                 info->TraitsInConstructOrder())
              if (rec->record_desc().str_full_name == str_full_name)
                return rec;
        return nullptr;
      };

      const meta::RecordObject* rec_badr_cargo =
          find_record("badr", "OpenRA.Mods.Common.Traits.CargoInfo");
      Check(rec_badr_cargo != nullptr,
            "p6b14: BADR CargoInfo record found");
      if (rec_badr_cargo != nullptr) {
        const mods::CargoInfoData badr_cargo =
            mods::CargoInfoData::Parse(*rec_badr_cargo);
        Check(badr_cargo.int4_max_weight == 10,
              "p6b14: BADR Cargo MaxWeight 10 parses");
        Check(badr_cargo.b_eject_on_sell && !badr_cargo.b_eject_on_death,
              "p6b14: BADR Cargo EjectOnSell true + EjectOnDeath false");
      }

      const meta::RecordObject* rec_badr_drop =
          find_record("badr", "OpenRA.Mods.Common.Traits.ParaDropInfo");
      Check(rec_badr_drop != nullptr, "p6b14: BADR ParaDropInfo found");
      if (rec_badr_drop != nullptr) {
        const mods::ParaDropInfoData badr_drop =
            mods::ParaDropInfoData::Parse(*rec_badr_drop);
        Check(badr_drop.dist_drop_range == WDist::FromCells(4) &&
                  badr_drop.int4_drop_interval == 5 &&
                  badr_drop.str_chute_sound == "chute1.aud",
              "p6b14: BADR ParaDrop DropRange/DropInterval/ChuteSound");
      }

      const meta::RecordObject* rec_apc_cargo =
          find_record("apc", "OpenRA.Mods.Common.Traits.CargoInfo");
      Check(rec_apc_cargo != nullptr, "p6b14: apc CargoInfo found");
      if (rec_apc_cargo != nullptr) {
        const mods::CargoInfoData apc_cargo =
            mods::CargoInfoData::Parse(*rec_apc_cargo);
        Check(apc_cargo.int4_max_weight == 5 &&
                  apc_cargo.str_loading_condition == "notmobile",
              "p6b14: apc Cargo MaxWeight 5 + LoadingCondition notmobile");
        Check(apc_cargo.vec_types.size() == 1 &&
                  apc_cargo.vec_types[0] == "Infantry",
              "p6b14: apc Cargo Types Infantry");
      }

      const meta::RecordObject* rec_e1_passenger =
          find_record("e1", "OpenRA.Mods.Common.Traits.PassengerInfo");
      Check(rec_e1_passenger != nullptr, "p6b14: e1 PassengerInfo found");
      if (rec_e1_passenger != nullptr) {
        const mods::PassengerInfoData e1_passenger =
            mods::PassengerInfoData::Parse(*rec_e1_passenger);
        Check(e1_passenger.str_cargo_type == "Infantry" &&
                  e1_passenger.int4_weight == 1,
              "p6b14: e1 Passenger CargoType Infantry + Weight 1");
      }

      const meta::RecordObject* rec_e1_parachutable =
          find_record("e1", "OpenRA.Mods.Common.Traits.ParachutableInfo");
      Check(rec_e1_parachutable != nullptr,
            "p6b14: e1 ParachutableInfo found");
      if (rec_e1_parachutable != nullptr) {
        const mods::ParachutableInfoData e1_parachutable =
            mods::ParachutableInfoData::Parse(*rec_e1_parachutable);
        Check(e1_parachutable.int4_fall_rate == 26 &&
                  e1_parachutable.str_parachuting_condition == "parachute",
              "p6b14: e1 Parachutable FallRate 26 + ParachutingCondition");
      }

      const meta::RecordObject* rec_e1_with_parachute = find_record(
          "e1", "OpenRA.Mods.Common.Traits.Render.WithParachuteInfo");
      Check(rec_e1_with_parachute != nullptr,
            "p6b14: e1 WithParachuteInfo found");
      if (rec_e1_with_parachute != nullptr) {
        const mods::WithParachuteInfoData e1_with_parachute =
            mods::WithParachuteInfoData::Parse(*rec_e1_with_parachute);
        Check(e1_with_parachute.str_image == "parach" &&
                  e1_with_parachute.str_opening_sequence == "open" &&
                  e1_with_parachute.str_sequence == "idle",
              "p6b14: e1 WithParachute parach/open/idle parse");
        Check(e1_with_parachute.str_shadow_image == "parach-shadow" &&
                  e1_with_parachute.wvec_offset == WVec{0, 0, 427},
              "p6b14: e1 WithParachute shadow + offset 427");
        Check(e1_with_parachute.conditional.expr_requires.has_value(),
              "p6b14: e1 WithParachute RequiresCondition parachute");
      }

      const meta::RecordObject* rec_para_power = find_record(
          "powerproxy.paratroopers",
          "OpenRA.Mods.Common.Traits.ParatroopersPowerInfo");
      Check(rec_para_power != nullptr,
            "p6b14: powerproxy.paratroopers found");
      if (rec_para_power != nullptr) {
        const mods::ParatroopersPowerInfoData para_power =
            mods::ParatroopersPowerInfoData::Parse(*rec_para_power);
        Check(para_power.vec_drop_items.size() == 5 &&
                  para_power.vec_drop_items[0] == "E1" &&
                  para_power.vec_drop_items[4] == "E3",
              "p6b14: paratroopers DropItems E1,E1,E1,E3,E3");
        Check(para_power.int4_quantized_facings == 8 &&
                  para_power.str_camera_actor == "camera.paradrop" &&
                  para_power.int4_camera_remove_delay == 85,
              "p6b14: paratroopers QuantizedFacings/Camera faces");
        Check(para_power.str_unit_type == "badr" &&
                  para_power.int4_squad_size == 1 &&
                  !para_power.b_allow_impassable_cells,
              "p6b14: paratroopers UnitType badr + SquadSize 1");
      }

      const auto spawn_offworld = [&](const std::string& str_type) {
        sim::TypeDictionary dict;
        sim::OwnerInit owner_init{player_a};
        dict.Add(new sim::OwnerInit{player_a});
        return world->CreateActor(false, str_type, dict);
      };

      sim::Actor* badr = spawn("badr", player_a, CPos{cx + 4, cy - 6});
      Check(badr != nullptr, "p6b14: BADR spawns");
      if (badr != nullptr) {
        auto* badr_cargo = badr->Trait<mods::Cargo>();
        sim::Actor* e1_off = spawn_offworld("e1");
        badr_cargo->Load(*badr, *e1_off);
        Check(badr_cargo->PassengerCount() == 1 &&
                  badr_cargo->Peek() == e1_off,
              "p6b14: Load carries the off-world e1");
        Check(badr_cargo->HasSpace(9) && !badr_cargo->HasSpace(10),
              "p6b14: MaxWeight 10 gates HasSpace(9)/HasSpace(10)");
        Check(badr_cargo->Unload(*badr) == e1_off && badr_cargo->IsEmpty(),
              "p6b14: Unload returns the passenger and empties");
        e1_off->Dispose();

        sim::Actor* e1_r = spawn_offworld("e1");
        Check(badr_cargo->ReserveSpace(*e1_r),
              "p6b14: ReserveSpace accepts weight 1");
        Check(!badr_cargo->HasSpace(10) && badr_cargo->HasSpace(9),
              "p6b14: reserved weight counts against HasSpace");
        badr_cargo->UnreserveSpace(*e1_r);
        Check(badr_cargo->HasSpace(10),
              "p6b14: UnreserveSpace restores the space");
        e1_r->Dispose();
      }

      sim::Actor* apc = spawn("apc", player_a, CPos{cx + 6, cy + 2});
      sim::Actor* e1rider = spawn("e1", player_a, CPos{cx + 4, cy + 2});
      Check(apc != nullptr && e1rider != nullptr,
            "p6b14: apc + e1 spawn for the enter chain");
      if (apc != nullptr && e1rider != nullptr) {
        net::Order order_enter{
            "EnterTransport", e1rider, sim::Target::FromActor(apc), false};
        e1rider->ResolveOrder(order_enter);
        Check(e1rider->CurrentActivity() != nullptr,
              "p6b14: EnterTransport queues RideTransport");

        for (int i = 0; i < 400; i++)
          world->Tick();
        auto* apc_cargo = apc->Trait<mods::Cargo>();
        Check(apc_cargo->PassengerCount() == 1 && !e1rider->IsInWorld(),
              "p6b14: the e1 rides into the apc and leaves the world");
        Check(e1rider->Trait<mods::Passenger>()->Transport() == apc,
              "p6b14: the passenger records its transport");

        net::Order order_unload{"Unload", apc, false};
        apc->ResolveOrder(order_unload);
        for (int i = 0; i < 400; i++)
          world->Tick();
        Check(apc_cargo->IsEmpty() && e1rider->IsInWorld(),
              "p6b14: the Unload order empties the apc");
        const CPos e1_cell = e1rider->Location();
        const CPos apc_cell = apc->Location();
        const int dx = std::abs(e1_cell.X() - apc_cell.X());
        const int dy = std::abs(e1_cell.Y() - apc_cell.Y());
        Check(dx <= 1 && dy <= 1 && (dx + dy) > 0,
              "p6b14: the unloaded e1 lands adjacent to the apc");
      }

      sim::Actor* badr2 = spawn("badr", player_a, CPos{cx + 4, cy + 8});
      Check(badr2 != nullptr, "p6b14: the paradrop BADR spawns");
      if (badr2 != nullptr) {
        auto* badr2_cargo = badr2->Trait<mods::Cargo>();
        std::vector<sim::Actor*> vec_paras;
        for (int i = 0; i < 3; i++) {
          sim::Actor* para = spawn_offworld("e1");
          badr2_cargo->Load(*badr2, *para);
          vec_paras.push_back(para);
        }

        const CPos lz{cx + 12, cy + 12};
        auto* para_drop = badr2->Trait<mods::ParaDrop>();
        para_drop->SetLZ(lz, true);
        net::Order order_fly{
            "Move", badr2, sim::Target::FromCell(*world, lz), false};
        badr2->ResolveOrder(order_fly);

        int int4_drop_z = -1;
        bool b_was_in_air = false;
        for (int i = 0; i < 800; i++) {
          world->Tick();
          if (int4_drop_z < 0 && vec_paras[0]->IsInWorld())
            int4_drop_z = vec_paras[0]->CenterPosition().Z;
          if (vec_paras[0]->IsInWorld() && !vec_paras[0]->IsDead() &&
              vec_paras[0]->Trait<mods::Parachutable>()->IsInAir())
            b_was_in_air = true;
        }
        Check(int4_drop_z > 2000,
              "p6b14: the para starts high in the air");
        Check(b_was_in_air, "p6b14: Parachutable reports in-air");

        auto* badr2_aircraft = badr2->Trait<mods::Aircraft>();
        Check(badr2_aircraft->CenterPosition().Z >= 2560,
              "p6b14: the paradrop BADR cruises at 2560");

        Check(!vec_paras[0]->IsInWorld() ||
                  vec_paras[0]->CenterPosition().Z ==
                      world->Map().CenterOfCell(vec_paras[0]->Location()).Z,
              "p6b14: the para lands on the ground");
        Check(!vec_paras[0]->Trait<mods::Parachutable>()->IsInAir(),
              "p6b14: Parachutable reports landed");
      }
    }

    // ———— 第十五批:Carryall 运输扩展(Carryable/AutoCarryable/
    // Carryall/AutoCarryall/CarryableHarvester + PickupUnit/DeliverUnit)————
    {
      const auto parse_record =
          [](std::string_view str_type_name,
             std::string_view str_yaml) {
        auto rec = meta::TypeRegistry::CreateObject(str_type_name);
        if (rec == nullptr)
          return std::unique_ptr<meta::RecordObject>{};
        yaml::MiniYaml my{};
        my.Nodes = yaml::MiniYaml::FromStream(str_yaml, str_type_name);
        meta::Load(rec.get(), my);
        return rec;
      };

      auto rec_carryable_info =
          parse_record("CarryableInfo",
                       "ReservedCondition: reserved\n"
                       "CarriedCondition: carried\n"
                       "LockedCondition: locked\n"
                       "LocalOffset: 0,0,-256\n");
      Check(rec_carryable_info != nullptr,
            "p6b15: CarryableInfo record loads");
      if (rec_carryable_info != nullptr) {
        const mods::CarryableInfoData carryable_info =
            mods::CarryableInfoData::Parse(*rec_carryable_info);
        Check(carryable_info.str_reserved_condition == "reserved" &&
                  carryable_info.str_carried_condition == "carried" &&
                  carryable_info.str_locked_condition == "locked",
              "p6b15: Carryable three conditions parse");
        Check(carryable_info.wvec_local_offset == WVec{0, 0, -256},
              "p6b15: Carryable LocalOffset -256");
      }

      auto rec_carryall_info =
          parse_record("CarryallInfo",
                       "BeforeLoadDelay: 7\n"
                       "BeforeUnloadDelay: 9\n"
                       "DropRange: 3c0\n"
                       "CarryCondition: carrying\n"
                       "CarryableConditions:\n"
                       "\tharv: hauling\n"
                       "UnloadCursor: deploy2\n"
                       "AllowDropOff: true\n"
                       "Voice: Move\n");
      Check(rec_carryall_info != nullptr, "p6b15: CarryallInfo record loads");
      if (rec_carryall_info != nullptr) {
        const mods::CarryallInfoData carryall_info =
            mods::CarryallInfoData::Parse(*rec_carryall_info);
        Check(carryall_info.int4_before_load_delay == 7 &&
                  carryall_info.int4_before_unload_delay == 9,
              "p6b15: Carryall load/unload delays parse");
        Check(carryall_info.dist_drop_range == WDist::FromCells(3) &&
                  carryall_info.str_carry_condition == "carrying",
              "p6b15: Carryall DropRange 3c + CarryCondition");
        Check(carryall_info.vec_carryable_conditions.size() == 1 &&
                  carryall_info.vec_carryable_conditions[0].first == "harv" &&
                  carryall_info.vec_carryable_conditions[0].second ==
                      "hauling",
              "p6b15: Carryall CarryableConditions dict");
        Check(carryall_info.str_unload_cursor == "deploy2" &&
                  carryall_info.b_allow_drop_off && carryall_info.str_voice == "Move",
              "p6b15: Carryall UnloadCursor/AllowDropOff/Voice");
        Check(carryall_info.color_target_line ==
                  core::Color::FromArgbRaw(4294967040u),
              "p6b15: Carryall TargetLineColor default yellow");
      }

      auto rec_auto_carryable_info =
          parse_record("AutoCarryableInfo", "MinDistance: 8c0\n");
      Check(rec_auto_carryable_info != nullptr,
            "p6b15: AutoCarryableInfo record loads");
      if (rec_auto_carryable_info != nullptr) {
        const mods::AutoCarryableInfoData auto_carryable_info =
            mods::AutoCarryableInfoData::Parse(*rec_auto_carryable_info);
        Check(auto_carryable_info.dist_min_distance == WDist::FromCells(8),
              "p6b15: AutoCarryable MinDistance 8c");
      }

      auto rec_auto_carryall_info = parse_record(
          "AutoCarryallInfo", "AutoCarryCondition: autotraining\n");
      Check(rec_auto_carryall_info != nullptr,
            "p6b15: AutoCarryallInfo record loads");
      if (rec_auto_carryall_info != nullptr) {
        const mods::AutoCarryallInfoData auto_carryall_info =
            mods::AutoCarryallInfoData::Parse(*rec_auto_carryall_info);
        Check(auto_carryall_info.expr_auto_carry_condition.has_value() &&
                  auto_carryall_info.carryall.dist_drop_range ==
                      WDist::FromCells(5),
              "p6b15: AutoCarryall condition + inherited DropRange 5c");
      }

      const auto mount_carryall =
          [&world](sim::Actor* host, const mods::CarryallInfoData& info) {
            sim::TypeDictionary dict_mount;
            sim::ActorInitializer init_mount{*host, dict_mount};
            auto* carryall =
                world->Arena().Create<mods::Carryall>(init_mount, info);
            host->AddTrait(carryall);
            return carryall;
          };
      const auto mount_carryable =
          [&world](sim::Actor* host, const mods::CarryableInfoData& info) {
            auto* carryable = world->Arena().Create<mods::Carryable>(info);
            host->AddTrait(carryable);
            carryable->Created(*host);
            return carryable;
          };

      mods::CarryallInfoData carryall_mount_info{};
      carryall_mount_info.int4_before_load_delay = 0;
      carryall_mount_info.int4_before_unload_delay = 0;
      carryall_mount_info.str_carry_condition = "testcarry";

      sim::Actor* heli = spawn("heli", player_a, CPos{cx + 8, cy - 8});
      Check(heli != nullptr, "p6b15: heli spawns");
      if (heli != nullptr) {
        auto* carryall = mount_carryall(heli, carryall_mount_info);
        Check(carryall->State() == mods::CarryallState::Idle,
              "p6b15: Carryall starts Idle");

        sim::Actor* harv = spawn("harv", player_a, CPos{cx + 8, cy - 4});
        Check(harv != nullptr, "p6b15: harv spawns");
        if (harv != nullptr) {
          mods::CarryableInfoData carryable_mount_info{};
          auto* carryable = mount_carryable(harv, carryable_mount_info);

          Check(carryable->Reserve(*harv, heli) &&
                    carryable->Reserved() &&
                    carryable->Carrier() == heli,
                "p6b15: Carryable Reserve marks Reserved + Carrier");
          Check(carryable->LockForPickup(*harv, heli) ==
                    mods::LockResponse::Success,
                "p6b15: idle ground Carryable locks for pickup");
          carryable->UnReserve(*harv);
          Check(!carryable->Reserved() && carryable->Carrier() == nullptr,
                "p6b15: UnReserve frees the carryable");

          Check(carryall->ReserveCarryable(*heli, harv) &&
                    carryall->State() == mods::CarryallState::Reserved &&
                    carryall->CarryableActor() == harv,
                "p6b15: ReserveCarryable flips Carryall to Reserved");
          carryall->UnreserveCarryable(*heli);
          Check(carryall->State() == mods::CarryallState::Idle &&
                    carryall->CarryableActor() == nullptr,
                "p6b15: UnreserveCarryable returns to Idle");

          Check(carryall->CanUnload() == false,
                "p6b15: CanUnload false without cargo");

          carryable->Attached(*harv, heli);
          Check(carryall->AttachCarryable(*heli, harv),
                "p6b15: AttachCarryable attaches");
          Check(carryall->State() == mods::CarryallState::Carrying &&
                    carryall->CarryableActor() == harv,
                "p6b15: AttachCarryable carries the harv");
          Check(carryall->OffsetForCarryable(*heli, *harv) == WVec{0, 0, 0},
                "p6b15: OffsetForCarryable zero with zero offsets");
          Check(carryall->LandableTerrainTypes().size() > 0,
                "p6b15: landable terrain types mirror the cargo locomotor");

          carryall->DetachCarryable(*heli);
          Check(carryall->State() == mods::CarryallState::Idle &&
                    carryall->LandableTerrainTypes().size() > 0,
                "p6b15: DetachCarryable resets state (aircraft default set)");
          carryable->Detached(*harv);
        }
      }

      const auto mount_auto =
          [&world](sim::Actor* host, const mods::AutoCarryallInfoData& info) {
            sim::TypeDictionary dict_mount;
            sim::ActorInitializer init_mount{*host, dict_mount};
            auto* auto_carryall =
                world->Arena().Create<mods::AutoCarryall>(init_mount, info);
            host->AddTrait(auto_carryall);
            return auto_carryall;
          };

      mods::AutoCarryallInfoData auto_mount_info{};
      mods::AutoCarryableInfoData auto_cargo_mount_info{};

      sim::Actor* heli2 = spawn("heli", player_a, CPos{cx - 8, cy - 8});
      sim::Actor* cargo2 = spawn("e1", player_a, CPos{cx - 8, cy - 2});
      Check(heli2 != nullptr && cargo2 != nullptr,
            "p6b15: auto chain heli + cargo spawn");
      if (heli2 != nullptr && cargo2 != nullptr) {
        auto* auto_carryall = mount_auto(heli2, auto_mount_info);
        auto* auto_carryable = world->Arena().Create<mods::AutoCarryable>(
            auto_cargo_mount_info);
        cargo2->AddTrait(auto_carryable);
        auto_carryable->Created(*cargo2);
        auto* harvester_trait =
            world->Arena().Create<mods::CarryableHarvester>(
                mods::CarryableHarvesterInfoData{});
        cargo2->AddTrait(harvester_trait);
        harvester_trait->Created(*cargo2);

        Check(auto_carryall->EnableAutoCarry() &&
                  !auto_carryable->WantsTransport(),
              "p6b15: AutoCarryall enabled + no transport wanted yet");

        const CPos far_cell{cx + 14, cy + 14};
        harvester_trait->MovingToResources(*cargo2, far_cell);
        Check(auto_carryable->Destination() == far_cell &&
                  auto_carryable->WantsTransport(),
                "p6b15: MovingToResources sets the destination");
        Check(heli2->CurrentActivity() != nullptr,
              "p6b15: the carryall takes the ferry job");

        bool b_in_cabin = false;
        bool b_landed = false;
        for (int i = 0; i < 1500; i++) {
          world->Tick();
          if (!b_in_cabin && !cargo2->IsInWorld() && !cargo2->IsDead())
            b_in_cabin = true;
          if (cargo2->IsInWorld() && !cargo2->IsDead() &&
              !auto_carryable->Reserved())
            b_landed = true;
        }
        Check(b_in_cabin, "p6b15: the cargo is picked up off the world");
        Check(auto_carryall->State() == mods::CarryallState::Idle ||
                  auto_carryall->CarryableActor() == nullptr,
              "p6b15: the carryall releases the cargo");
        Check(b_landed, "p6b15: the cargo is delivered back into the world");
        if (b_landed) {
          const CPos cargo_cell = cargo2->Location();
          const int dx = std::abs(cargo_cell.X() - far_cell.X());
          const int dy = std::abs(cargo_cell.Y() - far_cell.Y());
          Check(dx <= 6 && dy <= 6,
                "p6b15: the cargo lands within the drop range of the LZ");
        }
        Check(auto_carryable->Destination() == std::nullopt,
              "p6b15: Detached clears the destination");
      }
    }

  // ———— Phase 6 第十六批:游戏内 HUD Logic 第一组 ————
  // ———— Phase 6 batch 16: the HUD logic group one ————
  {
    std::println("-- batch 16: ingame HUD logics --");
    world->SetLocalPlayerForTest(player_a);

    int int4_local_tick = 0;
    std::vector<std::string> vec_played;
    static ui::ChromeMetrics metrics_hud;
    {
      const auto dir_metrics_hud = std::filesystem::temp_directory_path() /
                                   "ora_render_test_metrics_hud";
      std::filesystem::create_directories(dir_metrics_hud);
      {
        std::ofstream out_metrics(dir_metrics_hud / "metrics.yaml");
        out_metrics << "Chrome:\n";
        out_metrics << "\tTextFont: Regular\n";
        out_metrics << "\tTextColor: FFFFFFFF\n";
        out_metrics << "\tTextContrast: True\n";
        out_metrics << "\tTextShadow: False\n";
        out_metrics << "\tTextContrastColorDark: 000000FF\n";
        out_metrics << "\tTextContrastColorLight: FFFFFFFF\n";
        out_metrics << "\tTextContrastRadius: 1\n";
        out_metrics << "\tButtonDepth: 1\n";
        out_metrics << "\tButtonFont: Regular\n";
        out_metrics << "\tButtonTextColor: FFFFFFFF\n";
        out_metrics << "\tButtonTextColorDisabled: 808080FF\n";
        out_metrics << "\tButtonTextContrast: True\n";
        out_metrics << "\tButtonTextShadow: False\n";
        out_metrics << "\tButtonTextContrastColorDark: 000000FF\n";
        out_metrics << "\tButtonTextContrastColorLight: FFFFFFFF\n";
        out_metrics << "\tButtonTextContrastRadius: 1\n";
        out_metrics << "\tButtonCursor: default\n";
        out_metrics << "\tClickSound: click_sound\n";
        out_metrics << "\tClickDisabledSound: click_disabled_sound\n";
        out_metrics << "\tWorldSelectCursor: select\n";
        out_metrics << "\tWorldDefaultCursor: default\n";
        out_metrics << "\tInfiniteOffset: 4, 2\n";
      }
      fs::FileSystem fs_metrics_hud;
      fs_metrics_hud.Mount(dir_metrics_hud.generic_string());
      const std::vector<std::string> vec_files_metrics_hud{"metrics.yaml"};
      metrics_hud.Initialize({&fs_metrics_hud, &vec_files_metrics_hud});
    }
    ui::Ui::Deps deps_hud;
    deps_hud.ptr_chrome_metrics = &metrics_hud;
    deps_hud.fn_local_tick = [&int4_local_tick] {
      return int4_local_tick;
    };
    deps_hud.fn_play_notification =
        [&vec_played](const game::Ruleset*, std::string_view str_name) {
          if (!str_name.empty())
            vec_played.emplace_back(str_name);
        };
    deps_hud.fn_window_size = [] { return int2{1280, 720}; };
    ui::Ui::Instance().Initialize(std::move(deps_hud));

    ui::WidgetArgs args_widget;

    // IngameRadarDisplayLogic:ticker 状态机 + 渐隐面
    // IngameRadarDisplayLogic: the ticker state machine + the fade face.
    {
      auto up_root = std::make_unique<ui::ContainerWidget>();
      auto up_radar = std::make_unique<mods::RadarWidget>(args_widget);
      up_radar->str_id = "RADAR_MINIMAP";
      up_radar->str_sound_up = "radar-on";
      up_radar->str_sound_down = "radar-off";
      mods::RadarWidget* ptr_radar = up_radar.get();
      auto up_ticker = std::make_unique<ui::LogicTickerWidget>();
      up_ticker->str_id = "RADAR_TICKER";
      ui::LogicTickerWidget* ptr_ticker = up_ticker.get();
      auto up_block = std::make_unique<ui::ColorBlockWidget>(args_widget);
      up_block->str_id = "RADAR_FADETOBLACK";
      ui::ColorBlockWidget* ptr_block = up_block.get();
      up_root->AddChild(std::move(up_radar));
      up_root->AddChild(std::move(up_ticker));
      up_root->AddChild(std::move(up_block));

      mods::IngameRadarDisplayLogic logic_radar(*up_root, *world);
      Check(!ptr_radar->fn_is_enabled(),
            "p6b16: radar starts disabled");
      ptr_ticker->fn_on_tick();
      Check(!ptr_radar->fn_is_enabled() && vec_played.empty(),
            "p6b16: no ProvidesRadar keeps the minimap dark and silent");

      auto* ptr_dev = world->LocalPlayer()->PlayerActor()
                          ->Trait<mods::DeveloperMode>();
      ptr_dev->DisableShroud = true;
      ptr_ticker->fn_on_tick();
      Check(ptr_radar->fn_is_enabled() && vec_played.size() == 1 &&
                vec_played.back() == "radar-on",
            "p6b16: DisableShroud lights the radar and plays SoundUp");
      ptr_dev->DisableShroud = false;
      ptr_ticker->fn_on_tick();
      Check(!ptr_radar->fn_is_enabled() && vec_played.size() == 2 &&
                vec_played.back() == "radar-off",
            "p6b16: losing radar plays SoundDown");

      ptr_radar->fn_animating(0.5f);
      Check(ptr_block->fn_is_visible() &&
                ptr_block->fn_get_color().A() == 127,
            "p6b16: the fade-to-black alpha tracks the animation");
      ptr_radar->fn_animating(0.0f);
      Check(!ptr_block->fn_is_visible(),
            "p6b16: a zero alpha hides the block");
    }

    // IngameCashCounterLogic:收敛步进 + silo tooltip
    // IngameCashCounterLogic: the decay step + the silo tooltip.
    {
      auto up_root = std::make_unique<ui::ContainerWidget>();
      auto up_cash =
          std::make_unique<ui::LabelWithTooltipWidget>(args_widget);
      up_cash->str_id = "CASH";
      ui::LabelWithTooltipWidget* ptr_cash = up_cash.get();
      up_root->AddChild(std::move(up_cash));

      auto* ptr_res = world->LocalPlayer()->PlayerActor()
                          ->Trait<mods::PlayerResources>();
      const int int4_start_cash = ptr_res->GetCashAndResources();
      mods::IngameCashCounterLogic logic_cash(*up_root, *world);
      logic_cash.Tick();
      Check(ptr_cash->fn_get_text() == std::to_string(int4_start_cash),
            "p6b16: the cash label starts at the full amount");

      ptr_res->Cash -= 463;
      logic_cash.Tick();
      const int int4_expect =
          int4_start_cash -
          std::min(
              std::max(static_cast<int>(463 * 0.07f), 37), 463);
      Check(ptr_cash->fn_get_text() == std::to_string(int4_expect),
            "p6b16: the cash counter decays at DisplayFracPerFrame");
      Check(!ptr_cash->fn_get_tooltip_text().empty(),
            "p6b16: the silo-usage tooltip renders (the key fallback)");
    }

    // IngamePowerCounterLogic:三态文本/颜色/图标
    // IngamePowerCounterLogic: the three-state text/color/icon.
    {
      auto up_root = std::make_unique<ui::ContainerWidget>();
      auto up_power =
          std::make_unique<ui::LabelWithTooltipWidget>(args_widget);
      up_power->str_id = "POWER";
      ui::LabelWithTooltipWidget* ptr_power = up_power.get();
      auto up_icon = std::make_unique<ui::ImageWidget>();
      up_icon->str_id = "POWER_ICON";
      ui::ImageWidget* ptr_power_icon = up_icon.get();
      up_root->AddChild(std::move(up_power));
      up_root->AddChild(std::move(up_icon));

      auto* ptr_pm = world->LocalPlayer()->PlayerActor()
                         ->Trait<mods::PowerManager>();
      auto* ptr_dev = world->LocalPlayer()->PlayerActor()
                          ->Trait<mods::DeveloperMode>();
      ptr_pm->PowerProvided = 100;
      ptr_pm->PowerDrained = 50;
      ptr_dev->UnlimitedPower = false;
      mods::IngamePowerCounterLogic logic_power(*up_root, *world);
      Check(ptr_power_icon->fn_get_image_name() == "power-normal" &&
                ptr_power->fn_get_text() == "50" &&
                ptr_power->fn_get_color() ==
                    core::Color::FromArgb(255, 255, 255, 255),
            "p6b16: the power counter shows a healthy surplus");
      ptr_pm->PowerDrained = 150;
      Check(ptr_power_icon->fn_get_image_name() == "power-critical" &&
                ptr_power->fn_get_text() == "-50" &&
                ptr_power->fn_get_color() ==
                    core::Color::FromArgb(255, 255, 0, 0),
            "p6b16: a drained grid turns the counter critical");
      ptr_dev->UnlimitedPower = true;
      Check(ptr_power->fn_get_text() == "label-infinite-power",
            "p6b16: unlimited power renders the Fluent label (key fallback)");
      ptr_dev->UnlimitedPower = false;
    }

    // IngameSiloBarLogic:三色门 + 双源
    // IngameSiloBarLogic: the three-color gate + the two sources.
    {
      auto up_root = std::make_unique<ui::ContainerWidget>();
      auto up_bar = std::make_unique<ui::ResourceBarWidget>(args_widget);
      up_bar->str_id = "SILOBAR";
      ui::ResourceBarWidget* ptr_bar = up_bar.get();
      up_root->AddChild(std::move(up_bar));

      auto* ptr_res = world->LocalPlayer()->PlayerActor()
                          ->Trait<mods::PlayerResources>();
      ptr_res->Resources = 70;
      ptr_res->ResourceCapacity = 100;
      mods::IngameSiloBarLogic logic_silo(*up_root, *world);
      Check(ptr_bar->fn_get_provided() == 100.0f &&
                ptr_bar->fn_get_used() == 70.0f &&
                ptr_bar->fn_get_bar_color() ==
                    core::Color::FromArgb(255, 50, 205, 50),
            "p6b16: the silo bar starts lime-green");
      ptr_res->Resources = 80;
      Check(ptr_bar->fn_get_bar_color() ==
                core::Color::FromArgb(255, 255, 165, 0),
            "p6b16: 80% storage turns the silo bar orange");
      ptr_res->Resources = 100;
      Check(ptr_bar->fn_get_bar_color() ==
                core::Color::FromArgb(255, 255, 0, 0),
            "p6b16: a full silo turns the bar red");
    }

    // GameTimerLogic:暂停文本/状态闪烁/录像 tooltip 三态
    // GameTimerLogic: the paused text/status blink/replay tooltip trio.
    {
      auto up_root = std::make_unique<ui::ContainerWidget>();
      auto up_timer =
          std::make_unique<ui::LabelWithTooltipWidget>(args_widget);
      up_timer->str_id = "GAME_TIMER";
      ui::LabelWithTooltipWidget* ptr_timer = up_timer.get();
      auto up_status = std::make_unique<ui::LabelWidget>(args_widget);
      up_status->str_id = "GAME_TIMER_STATUS";
      ui::LabelWidget* ptr_status = up_status.get();
      up_root->AddChild(std::move(up_timer));
      up_root->AddChild(std::move(up_status));

      world->SetReplayTimestep(0);
      mods::GameTimerLogic logic_timer(
          *up_root, *game_.OrderManagerFace(), *world);
      Check(ptr_status->fn_get_text() == "label-paused" &&
                ptr_status->fn_is_visible(),
            "p6b16: a paused replay blinks the paused status");
      int4_local_tick += 1000;
      Check(!ptr_status->fn_is_visible(),
            "p6b16: the status blinks off on the odd second");
      int4_local_tick += 1000;
      Check(ptr_status->fn_is_visible(),
            "p6b16: the status blinks back on");
      world->SetReplayTimestep(1);
      Check(ptr_status->fn_get_text() == "label-max-speed",
            "p6b16: replay speed 1 renders the max-speed label");
      world->SetReplayTimestep(40);
      Check(ptr_timer->fn_get_text() ==
                ui::FormatTime(world->WorldTick(), world->Timestep()),
            "p6b16: the timer renders FormatTime(WorldTick, Timestep)");
      Check(ptr_timer->fn_get_tooltip_text == nullptr,
            "p6b16: a non-replay connection leaves the tooltip unset");
    }

    // ClassicProductionLogic:背景行方程 + tab 装配 + ticker 选队
    // ClassicProductionLogic: the background-row equation + the tab
    // assembly + the ticker's queue pick.
    {
      auto up_root = std::make_unique<ui::ContainerWidget>();
      auto up_palette =
          std::make_unique<mods::ProductionPaletteWidget>(args_widget);
      up_palette->str_id = "PRODUCTION_PALETTE";
      up_palette->ptr_world = world.get();
      up_palette->ptr_world_renderer = &wr;
      mods::ProductionPaletteWidget* ptr_palette = up_palette.get();
      auto up_background = std::make_unique<ui::ContainerWidget>();
      up_background->str_id = "PALETTE_BACKGROUND";
      ui::ContainerWidget* ptr_background = up_background.get();
      auto up_row_template = std::make_unique<ui::ContainerWidget>();
      up_row_template->str_id = "ROW_TEMPLATE";
      up_row_template->bounds_widget.int4_height = 24;
      auto up_bottom_cap = std::make_unique<ui::ContainerWidget>();
      up_bottom_cap->str_id = "BOTTOM_CAP";
      up_bottom_cap->bounds_widget.int4_height = 8;
      auto up_foreground = std::make_unique<ui::ContainerWidget>();
      up_foreground->str_id = "PALETTE_FOREGROUND";
      ui::ContainerWidget* ptr_foreground = up_foreground.get();
      auto up_fg_row = std::make_unique<ui::ContainerWidget>();
      up_fg_row->str_id = "ROW_TEMPLATE";
      up_fg_row->bounds_widget.int4_height = 20;
      auto up_types = std::make_unique<ui::ContainerWidget>();
      up_types->str_id = "PRODUCTION_TYPES";
      auto up_button =
          std::make_unique<mods::ProductionTypeButtonWidget>(args_widget);
      up_button->LoadFieldOrProperty("ProductionGroup", "Building");
      mods::ProductionTypeButtonWidget* ptr_button = up_button.get();
      auto up_icon = std::make_unique<ui::ImageWidget>();
      up_icon->str_id = "ICON";
      ui::ImageWidget* ptr_tab_icon = up_icon.get();
      auto up_ticker = std::make_unique<ui::LogicTickerWidget>();
      up_ticker->str_id = "PRODUCTION_TICKER";
      ui::LogicTickerWidget* ptr_prod_ticker = up_ticker.get();

      up_button->AddChild(std::move(up_icon));
      up_types->AddChild(std::move(up_button));
      up_background->AddChild(std::move(up_row_template));
      up_background->AddChild(std::move(up_bottom_cap));
      up_foreground->AddChild(std::move(up_fg_row));
      up_root->AddChild(std::move(up_palette));
      up_root->AddChild(std::move(up_background));
      up_root->AddChild(std::move(up_foreground));
      up_root->AddChild(std::move(up_types));
      up_root->AddChild(std::move(up_ticker));

      mods::ClassicProductionLogic logic_production(*up_root, *world);

      Check(ptr_background->Children().size() == 5 &&
                ptr_background->Children()[3]->bounds_widget.int4_y ==
                    3 * 24 &&
                ptr_background->Children()[4]->str_id == "BOTTOM_CAP" &&
                ptr_background->Children()[4]->bounds_widget.int4_y ==
                    4 * 24,
            "p6b16: the background starts at MinimumRows + the cap");
      Check(ptr_foreground->Children().size() == 4 &&
                ptr_foreground->Children()[3]->bounds_widget.int4_y ==
                    3 * 20,
            "p6b16: the foreground mirrors the row stack");

      ptr_palette->fn_on_icon_count_changed(16, 16);
      Check(ptr_background->Children().size() == 7 &&
                ptr_background->Children()[5]->bounds_widget.int4_y ==
                    5 * 24 &&
                ptr_background->Children()[0]->bounds_widget.int4_y == 0,
            "p6b16: sixteen icons re-row the background to six rows");
      Check(ptr_background->Children().back()->str_id == "BOTTOM_CAP" &&
                ptr_background->Children().back()->bounds_widget.int4_y ==
                    6 * 24,
            "p6b16: the cap follows the re-rowed stack");

      ptr_prod_ticker->fn_on_tick();
      Check(ptr_palette->ptr_current_queue != nullptr,
            "p6b16: the ticker picks the first enabled queue");
      if (ptr_palette->ptr_current_queue != nullptr) {
        const mods::ProductionQueueInfoData& info_queue =
            ptr_palette->ptr_current_queue->Info();
        Check((info_queue.str_group.empty() ? info_queue.str_type
                                            : info_queue.str_group) ==
                  "Building",
              "p6b16: the selected queue belongs to the Building tab");
        Check(ptr_button->fn_is_highlighted(),
              "p6b16: the Building tab highlights its queue");
      }
      Check(ptr_tab_icon->fn_get_image_name() == "building",
            "p6b16: the tab icon resolves the bare chrome name");
    }

    // ProductionTabsLogic:组表装配 + 选组链
    // ProductionTabsLogic: the group-table assembly + the pick chain.
    {
      auto up_palette =
          std::make_unique<mods::ProductionPaletteWidget>(args_widget);
      up_palette->str_id = "PRODUCTION_PALETTE";
      up_palette->ptr_world = world.get();
      up_palette->ptr_world_renderer = &wr;
      ui::WidgetArgs args_tabs;
      args_tabs.Add("world", world.get());
      auto up_tabs =
          std::make_unique<mods::ProductionTabsWidget>(args_tabs);
      up_tabs->str_id = "PRODUCTION_TABS";
      up_tabs->str_types_container = "PRODUCTION_TYPES";
      up_tabs->str_background_container = "PRODUCTION_TABS_BACKGROUND";
      up_tabs->str_palette_widget = "PRODUCTION_PALETTE";
      mods::ProductionTabsWidget* ptr_tabs = up_tabs.get();
      ui::Ui::Instance().Root()->AddChild(std::move(up_palette));
      ui::Ui::Instance().Root()->AddChild(std::move(up_tabs));

      auto up_types = std::make_unique<ui::ContainerWidget>();
      up_types->str_id = "PRODUCTION_TYPES";
      auto up_button =
          std::make_unique<mods::ProductionTypeButtonWidget>(args_widget);
      up_button->LoadFieldOrProperty("ProductionGroup", "");
      mods::ProductionTypeButtonWidget* ptr_button = up_button.get();
      auto up_icon = std::make_unique<ui::ImageWidget>();
      up_icon->str_id = "ICON";
      up_button->AddChild(std::move(up_icon));
      up_types->AddChild(std::move(up_button));

      auto up_bg_container = std::make_unique<ui::ContainerWidget>();
      up_bg_container->str_id = "PRODUCTION_TABS_BACKGROUND";
      auto up_icon_template = std::make_unique<ui::ContainerWidget>();
      up_icon_template->str_id = "ICON_TEMPLATE";
      up_bg_container->AddChild(std::move(up_icon_template));

      ui::Ui::Instance().Root()->AddChild(std::move(up_types));
      ui::Ui::Instance().Root()->AddChild(std::move(up_bg_container));

      mods::ProductionTabsLogic logic_tabs(*ptr_tabs, *world);

      Check(ptr_tabs->Groups().count("") == 1 &&
                ptr_tabs->Groups().count("Building") == 0,
            "p6b16: ra's groupless queues land under the empty group key");
      Check(!ptr_button->fn_is_highlighted(),
            "p6b16: nothing highlighted before the first pick");

      sim::Actor* player_actor_a = world->LocalPlayer()->PlayerActor();
      ptr_tabs->ActorChanged(*player_actor_a);
      Check(!ptr_tabs->Groups().at("").vec_tabs.empty(),
            "p6b16: ActorChanged fills the groupless tab list");
      if (!ptr_tabs->Groups().at("").vec_tabs.empty()) {
        ptr_button->fn_on_mouse_up(MouseInput{});
        Check(ptr_tabs->QueueGroup() == "",
              "p6b16: the tab pick sets QueueGroup");
        Check(ptr_button->fn_is_highlighted(),
              "p6b16: the picked tab highlights");
        Check(ptr_button->Get<ui::ImageWidget>("ICON")
                    ->fn_get_image_name() == "",
              "p6b16: the tabs icon resolves the bare chrome name");
      }

      ui::Ui::Instance().Root()->RemoveChildren();
    }
  }

  // ———— Phase 6 第十七批:菜单 Logic 组 + sell/repair 链 ————
  // ———— Phase 6 batch 17: the menu-logic group + the sell/repair
  //      chains ————
  {
    std::println("-- batch 17: menu logics + sell/repair chains --");
    world->SetLocalPlayerForTest(player_a);

    {
      auto* mo =
          player_a->PlayerActor()->Trait<mods::MissionObjectives>();
      Check(mo != nullptr,
            "p6b17: MissionObjectives mounted on the player actor");
      if (mo != nullptr) {
        int int4_events = 0;
        const std::size_t n_token = mo->AddObjectiveAddedHandler(
            [&int4_events](sim::Player&, bool b_inhibit) {
              if (!b_inhibit)
                int4_events++;
            });
        Check(!player_a->HasObjectives(),
              "p6b17: no objectives before Add");
        const int int4_first_id = mo->Add(*player_a, "obj1", "primary");
        CheckEq(int4_first_id, 0, "p6b17: the first objective id is 0");
        Check(player_a->HasObjectives() && int4_events == 1,
              "p6b17: Add fires ObjectiveAdded and flags HasObjectives");
        mo->Add(*player_a, "obj2", "secondary", true, true);
        Check(int4_events == 1,
              "p6b17: inhibit suppresses the announcement");
        mo->RemoveObjectiveAddedHandler(n_token);
        mo->Add(*player_a, "obj3", "primary");
        Check(int4_events == 1,
              "p6b17: the unsubscribed handler stays silent");
        CheckEq(mo->Objectives().size(), std::size_t{3},
                "p6b17: three objectives registered");
      }
    }

    {
      mods::SellOrderGenerator sell_gen(*world);
      Check(sell_gen.GetCursor(*world, CPos{cx, cy},
                               int2{cx * 1024, cy * 1024},
                               MouseInput{}) == "sell-blocked",
            "p6b17: the sell cursor stays blocked without a hover hit");
      mods::RepairOrderGenerator repair_gen(*world);
      Check(repair_gen.GetCursor(*world, CPos{cx, cy},
                                 int2{cx * 1024, cy * 1024},
                                 MouseInput{}) == "repair-blocked",
            "p6b17: the repair cursor stays blocked without a hover hit");
      mods::PowerDownOrderGenerator powerdown_gen(*world);
      Check(powerdown_gen.GetCursor(*world, CPos{cx, cy},
                                    int2{cx * 1024, cy * 1024},
                                    MouseInput{}) == "powerdown-blocked",
            "p6b17: the powerdown cursor stays blocked without a hover hit");
      mods::BeaconOrderGenerator beacon_gen(*world);
      Check(beacon_gen.GetCursor(*world, CPos{cx, cy},
                                 int2{cx * 1024, cy * 1024},
                                 MouseInput{}) == "ability",
            "p6b17: the beacon cursor is the ability cursor");

      world->AdoptOrderGenerator(
          std::make_unique<mods::BeaconOrderGenerator>(*world));
      Check(dynamic_cast<mods::BeaconOrderGenerator*>(
                world->OrderGenerator()) != nullptr,
            "p6b17: the beacon generator takes the input mode");

      ora::MouseInput mi_beacon{};
      mi_beacon.Event = ora::MouseInputEvent::Down;
      mi_beacon.Button = ora::MouseButton::Left;
      std::vector<std::unique_ptr<net::Order>> vec_beacon_orders =
          world->OrderGenerator()->Order(*world, CPos{cx, cy},
                                         int2{cx * 1024, cy * 1024},
                                         mi_beacon);
      Check(vec_beacon_orders.size() == 1 &&
                vec_beacon_orders.front()->str_order_string ==
                    "PlaceBeacon" &&
                vec_beacon_orders.front()->b_suppress_visual_feedback,
            "p6b17: the beacon action orders PlaceBeacon with the "
            "visual-feedback suppression");
      Check(dynamic_cast<mods::BeaconOrderGenerator*>(
                world->OrderGenerator()) == nullptr,
            "p6b17: ordering the beacon cancels the input mode");

      world->AdoptOrderGenerator(
          std::make_unique<mods::SellOrderGenerator>(*world));
      ora::MouseInput mi_cancel{};
      mi_cancel.Event = ora::MouseInputEvent::Up;
      mi_cancel.Button = ora::MouseButton::Right;
      const std::vector<std::unique_ptr<net::Order>> vec_cancel_orders =
          world->OrderGenerator()->Order(*world, CPos{cx, cy},
                                         int2{cx * 1024, cy * 1024},
                                         mi_cancel);
      Check(vec_cancel_orders.empty() &&
                dynamic_cast<mods::SellOrderGenerator*>(
                    world->OrderGenerator()) == nullptr,
            "p6b17: the cancel button releases the input mode");
    }

    auto spawn_built = [&](const std::string& str_type, sim::Player* owner,
                           CPos cell) {
      sim::TypeDictionary dict;
      sim::LocationInit location{cell};
      sim::OwnerInit owner_init{owner};
      sim::SkipMakeAnimsInit skip_make_anims;
      dict.Add(&location);
      dict.Add(&owner_init);
      dict.Add(&skip_make_anims);
      return world->CreateActor(true, str_type, dict);
    };

    {
      sim::Actor* tent_sell =
          spawn_built("tent", player_a, CPos{cx - 14, cy - 14});
      Check(tent_sell != nullptr, "p6b17: the sell tent constructed");
      if (tent_sell != nullptr) {
        auto* sellable = tent_sell->Trait<mods::Sellable>();
        auto* resources =
            player_a->PlayerActor()->Trait<mods::PlayerResources>();
        Check(sellable != nullptr && resources != nullptr,
              "p6b17: Sellable + PlayerResources mounted");
        if (sellable != nullptr && resources != nullptr) {
          for (int i = 0; i < 8 && sellable->IsTraitDisabled(); i++)
            world->Tick();
          const int int4_sell_value = mods::GetSellValue(*tent_sell);
          const int int4_cash_before = resources->Cash;
          net::Order order_sell{"Sell", tent_sell, false};
          sellable->ResolveOrder(*tent_sell, order_sell);

          const bool b_sold = [&] {
            for (int i = 0; i < 1400; i++) {
              world->Tick();
              if (tent_sell->IsDead())
                return true;
            }
            return tent_sell->IsDead();
          }();
          Check(b_sold, "p6b17: the sell activity disposes the tent");
          const int int4_refund =
              int4_sell_value * sellable->Info().int4_refund_percent / 100;
          Check(resources->Cash == int4_cash_before + int4_refund,
                "p6b17: the refund credits the half value at full health");
        }
      }
    }

    {
      sim::Actor* tent_repair =
          spawn_built("tent", player_a, CPos{cx + 14, cy + 14});
      Check(tent_repair != nullptr, "p6b17: the repair tent constructed");
      if (tent_repair != nullptr) {
        auto* health = tent_repair->Trait<sim::IHealth>();
        auto* repairable_building =
            tent_repair->Trait<mods::RepairableBuilding>();
        auto* resources =
            player_a->PlayerActor()->Trait<mods::PlayerResources>();
        Check(health != nullptr && repairable_building != nullptr,
              "p6b17: RepairableBuilding + IHealth mounted");
        if (health != nullptr && repairable_building != nullptr &&
            resources != nullptr) {
          resources->GiveCash(100000);
          const int int4_max_hp = health->MaxHP();
          health->InflictDamage(*tent_repair, nullptr,
                                sim::Damage{int4_max_hp / 2}, false);
          Check(health->HP() < int4_max_hp,
                "p6b17: the repair tent starts damaged");

          repairable_building->RepairBuilding(*tent_repair, player_a);
          CheckEq(repairable_building->Repairers().size(), std::size_t{1},
                  "p6b17: RepairBuilding registers the repairer");
          repairable_building->RepairBuilding(*tent_repair, player_a);
          Check(repairable_building->Repairers().empty(),
                "p6b17: a second toggle unregisters the repairer");

          repairable_building->RepairBuilding(*tent_repair, player_a);
          const int int4_cash_before =
              resources->Cash + resources->Resources;
          int int4_ticks = 0;
          while (health->HP() < int4_max_hp && int4_ticks < 4000) {
            world->Tick();
            int4_ticks++;
          }
          Check(health->HP() == int4_max_hp,
                "p6b17: the repair chain restores the full health");
          Check(resources->Cash + resources->Resources < int4_cash_before &&
                    resources->Spent > 0,
                "p6b17: the repair steps charge the treasury");
          Check(!repairable_building->RepairActive() &&
                    repairable_building->Repairers().empty(),
                "p6b17: the repair deactivates at full health");
          Check(mods::GetSellValue(*tent_repair) > 0,
                "p6b17: GetSellValue reads the Valued cost");
        }
      }
    }
  }

  {
    static ui::ChromeMetrics metrics_menu17;
    {
      const auto dir_metrics_menu =
          std::filesystem::temp_directory_path() /
          "ora_render_test_metrics_menu17";
      std::filesystem::create_directories(dir_metrics_menu);
      {
        std::ofstream out_metrics(dir_metrics_menu / "metrics.yaml");
        out_metrics << "Chrome:" << '\n';
        out_metrics << "\tTextFont: Regular" << '\n';
        out_metrics << "\tTextColor: FFFFFFFF" << '\n';
        out_metrics << "\tTextContrast: True" << '\n';
        out_metrics << "\tTextShadow: False" << '\n';
        out_metrics << "\tTextContrastColorDark: 000000FF" << '\n';
        out_metrics << "\tTextContrastColorLight: FFFFFFFF" << '\n';
        out_metrics << "\tTextContrastRadius: 1" << '\n';
        out_metrics << "\tButtonDepth: 1" << '\n';
        out_metrics << "\tButtonFont: Regular" << '\n';
        out_metrics << "\tButtonTextColor: FFFFFFFF" << '\n';
        out_metrics << "\tButtonTextColorDisabled: 808080FF" << '\n';
        out_metrics << "\tButtonTextContrast: True" << '\n';
        out_metrics << "\tButtonTextShadow: False" << '\n';
        out_metrics << "\tButtonTextContrastColorDark: 000000FF" << '\n';
        out_metrics << "\tButtonTextContrastColorLight: FFFFFFFF" << '\n';
        out_metrics << "\tButtonTextContrastRadius: 1" << '\n';
        out_metrics << "\tButtonCursor: default" << '\n';
        out_metrics << "\tClickSound: click_sound" << '\n';
        out_metrics << "\tClickDisabledSound: click_disabled_sound" << '\n';
      }
      fs::FileSystem fs_metrics_menu;
      fs_metrics_menu.Mount(dir_metrics_menu.generic_string());
      const std::vector<std::string> vec_files_metrics_menu{"metrics.yaml"};
      metrics_menu17.Initialize({&fs_metrics_menu, &vec_files_metrics_menu});
    }

    int int4_tick17 = 0;
    ui::WidgetArgs args_local;
    ui::Ui::Deps deps_menu17;
    deps_menu17.ptr_chrome_metrics = &metrics_menu17;
    deps_menu17.fn_local_tick = [&int4_tick17] { return int4_tick17; };
    deps_menu17.fn_window_size = [] { return int2{1280, 720}; };
    ui::Ui::Instance().Initialize(std::move(deps_menu17));

    {
      auto up_bin = std::make_unique<ui::ContainerWidget>();
      auto up_palette =
          std::make_unique<mods::SupportPowersWidget>(args_local);
      up_palette->str_id = "SUPPORT_PALETTE";
      mods::SupportPowersWidget* ptr_palette_bin = up_palette.get();
      ptr_palette_bin->int2_icon_size = int2{62, 46};
      ptr_palette_bin->int4_icon_margin = 4;
      auto up_bg = std::make_unique<ui::ContainerWidget>();
      up_bg->str_id = "PALETTE_BACKGROUND";
      auto up_bg_template = std::make_unique<ui::ContainerWidget>();
      up_bg_template->str_id = "ICON_TEMPLATE";
      up_bg->AddChild(std::move(up_bg_template));
      auto up_fg = std::make_unique<ui::ContainerWidget>();
      up_fg->str_id = "PALETTE_FOREGROUND";
      auto up_fg_template = std::make_unique<ui::ContainerWidget>();
      up_fg_template->str_id = "ICON_TEMPLATE";
      up_fg->AddChild(std::move(up_fg_template));
      up_bin->AddChild(std::move(up_palette));
      up_bin->AddChild(std::move(up_bg));
      up_bin->AddChild(std::move(up_fg));
      ui::ContainerWidget* ptr_bg = dynamic_cast<ui::ContainerWidget*>(
          up_bin->Get("PALETTE_BACKGROUND"));
      ui::ContainerWidget* ptr_fg = dynamic_cast<ui::ContainerWidget*>(
          up_bin->Get("PALETTE_FOREGROUND"));

      mods::SupportPowerBinLogic logic_bin(*up_bin);
      Check(ptr_bg != nullptr && ptr_fg != nullptr &&
                ptr_bg->Children().empty() && ptr_fg->Children().empty(),
            "p6b17: the bin starts with zero rows");
      ptr_palette_bin->fn_on_icon_count_changed(0, 3);
      Check(ptr_bg != nullptr && ptr_bg->Children().size() == 3 &&
                ptr_fg != nullptr && ptr_fg->Children().size() == 3,
            "p6b17: three icons clone three rows");
      if (ptr_bg != nullptr && ptr_bg->Children().size() == 3) {
        Check(ptr_bg->Children()[1]->bounds_widget.int4_y ==
                  ptr_bg->Children()[0]->bounds_widget.int4_y + 50,
              "p6b17: the vertical stride is icon height + margin");
        ptr_palette_bin->fn_on_icon_count_changed(0, 1);
        Check(ptr_bg->Children().size() == 1,
              "p6b17: a recount rebuilds the row stack");
      }
    }

    {
      auto up_button_suffix = std::make_unique<ui::ButtonWidget>(args_local);
      up_button_suffix->str_background = "button";
      auto up_image_suffix = std::make_unique<ui::ImageWidget>();
      up_image_suffix->str_image_collection = "logos";
      auto up_plain_suffix = std::make_unique<ui::ContainerWidget>();
      const std::string str_faction =
          world->LocalPlayer()->Faction().InternalName;

      mods::AddFactionSuffixLogic logic_suffix_a(*up_button_suffix, *world);
      Check(up_button_suffix->str_background == "button-" + str_faction,
            "p6b17: the button background takes the faction suffix");
      mods::AddFactionSuffixLogic logic_suffix_b(*up_image_suffix, *world);
      Check(up_image_suffix->str_image_collection == "logos-" + str_faction,
            "p6b17: the image collection takes the faction suffix");
      bool b_threw_suffix = false;
      try {
        mods::AddFactionSuffixLogic logic_suffix_c(*up_plain_suffix, *world);
      } catch (const std::runtime_error&) {
        b_threw_suffix = true;
      }
      Check(b_threw_suffix,
            "p6b17: an unsupported widget type throws the upstream message");
    }

    {
      auto up_world_root = std::make_unique<ui::ContainerWidget>();
      up_world_root->str_id = "WORLD_ROOT";
      auto up_menu_root = std::make_unique<ui::ContainerWidget>();
      up_menu_root->str_id = "MENU_ROOT";
      ui::Ui::Instance().Root()->AddChild(std::move(up_world_root));
      ui::Ui::Instance().Root()->AddChild(std::move(up_menu_root));

      auto up_top_buttons = std::make_unique<ui::ContainerWidget>();
      auto up_options = std::make_unique<ui::MenuButtonWidget>(args_local);
      up_options->str_id = "OPTIONS_BUTTON";
      ui::MenuButtonWidget* ptr_options = up_options.get();
      up_top_buttons->AddChild(std::move(up_options));

      {
        mods::MenuButtonsChromeLogic logic_menu_buttons(*up_top_buttons,
                                                         *world);
        Check(ptr_options->fn_is_disabled() == false,
              "p6b17: the options button starts enabled");
        auto* mo_menu =
            world->LocalPlayer()->PlayerActor()
                ->Trait<mods::MissionObjectives>();
        if (mo_menu != nullptr)
          mo_menu->Add(*world->LocalPlayer(), "menu obj", "primary");
        Check(ptr_options->fn_is_highlighted(),
              "p6b17: an added objective starts the blink");
        int4_tick17 = 26;
        Check(!ptr_options->fn_is_highlighted(),
              "p6b17: the blink rests past half the beat");
        int4_tick17 = 3;
        Check(ptr_options->fn_is_highlighted(),
              "p6b17: the blink returns within the beat");
      }
      ui::Ui::Instance().Root()->RemoveChildren();
    }

    {
      yaml::StringPool pool_menu;
      const std::string* ptr_stride_value = pool_menu.Intern("0, 40");
      const std::string* ptr_buttons_value =
          pool_menu.Intern("RESUME, SETTINGS, MUSIC");
      yaml::MiniYaml yaml_stride{ptr_stride_value};
      yaml::MiniYaml yaml_buttons{ptr_buttons_value};
      const ui::YamlDictionary dict_logic_args{
          {std::string_view{"ButtonStride"}, &yaml_stride},
          {std::string_view{"Buttons"}, &yaml_buttons}};

      auto up_menu_root = std::make_unique<ui::ContainerWidget>();
      auto up_ingame_menu = std::make_unique<ui::ContainerWidget>();
      up_ingame_menu->str_id = "INGAME_MENU";
      auto up_menu_buttons = std::make_unique<ui::ContainerWidget>();
      up_menu_buttons->str_id = "MENU_BUTTONS";
      up_menu_buttons->bounds_widget = ui::WidgetBounds{100, 60, 200, 120};
      auto up_button_template =
          std::make_unique<ui::ButtonWidget>(args_local);
      up_button_template->str_id = "BUTTON_TEMPLATE";
      up_button_template->bounds_widget = ui::WidgetBounds{30, 60, 140, 30};
      up_menu_buttons->AddChild(std::move(up_button_template));
      up_ingame_menu->AddChild(std::move(up_menu_buttons));
      up_menu_root->AddChild(std::move(up_ingame_menu));

      bool b_exited_menu = false;
      auto logic_ingame_menu = std::make_unique<mods::IngameMenuLogic>(
          *up_menu_root, mod_data, *world,
          [&b_exited_menu] { b_exited_menu = true; }, nullptr,
          mods::IngameInfoPanel::AutoSelect, &dict_logic_args);

      ui::ContainerWidget* ptr_menu_buttons_out =
          dynamic_cast<ui::ContainerWidget*>(
              up_menu_root->Get("INGAME_MENU")->Get("MENU_BUTTONS"));
      Check(ptr_menu_buttons_out != nullptr &&
                ptr_menu_buttons_out->Children().size() == 3,
            "p6b17: the Buttons list builds three menu buttons");
      Check(ptr_menu_buttons_out != nullptr &&
                ptr_menu_buttons_out->bounds_widget.int4_width == 200 &&
                ptr_menu_buttons_out->bounds_widget.int4_height == 120 + 80,
            "p6b17: the stride expands the button container");
      if (ptr_menu_buttons_out != nullptr &&
          ptr_menu_buttons_out->Children().size() == 3) {
        Check(ptr_menu_buttons_out->Children()[1]->bounds_widget.int4_y ==
                  ptr_menu_buttons_out->Children()[0]->bounds_widget.int4_y +
                      40,
              "p6b17: the buttons stack at the ButtonStride pitch");
        Check(ptr_menu_buttons_out->Children()[0]->str_id == "RESUME" &&
                  ptr_menu_buttons_out->Children()[2]->str_id == "MUSIC",
              "p6b17: the buttons keep their ids");
        auto* ptr_resume = dynamic_cast<ui::ButtonWidget*>(
            ptr_menu_buttons_out->Children()[0]);
        if (ptr_resume != nullptr) {
          Check(ptr_resume->fn_get_text() == "menu-ingame.resume",
                "p6b17: the resume label falls back to the fluent key");
          Check(!ptr_resume->fn_is_disabled(),
                "p6b17: the resume button starts enabled");
          ptr_resume->fn_on_click();
          Check(b_exited_menu,
                "p6b17: the resume click runs the onExit callback");
        }
        Check(ptr_menu_buttons_out->fn_is_visible(),
              "p6b17: the menu is visible before hiding");
      }
      logic_ingame_menu.reset();
    }
  }

  // ———— Phase 6 第十八批:浏览器面板第一组(Replay 链 + GameSave 格式)————
  // ———— Phase 6 batch 18: the first browser-panel group (the replay
  //      chain + the GameSave format) ————
  {
    std::println("-- batch 18: replay browser + game save format --");

    {
      server::GameDateTime dt{};
      dt.str_text = "1970-01-02 00-00-00";
      CheckEq(dt.ToUnixSeconds(), std::int64_t{86400},
              "p6b18: GameDateTime decomposes to unix seconds");
      server::GameInformation info_duration{};
      info_duration.StartTimeUtc.str_text = "2026-01-01 00-00-00";
      info_duration.EndTimeUtc.str_text = "2026-01-01 01-00-00";
      CheckEq(info_duration.DurationSeconds(), 3600.0,
              "p6b18: the duration spans start to end");
      info_duration.EndTimeUtc.str_text = "2025-12-31 00-00-00";
      CheckEq(info_duration.DurationSeconds(), 0.0,
              "p6b18: the inverted range clamps to zero");
      info_duration.vec_players.push_back(server::GameInformationPlayer{});
      info_duration.vec_players.back().IsHuman = true;
      Check(info_duration.IsSinglePlayer(),
            "p6b18: one human is singleplayer");
      info_duration.vec_players.push_back(server::GameInformationPlayer{});
      info_duration.vec_players.back().IsHuman = false;
      info_duration.vec_players.back().IsBot = true;
      Check(info_duration.IsSinglePlayer(),
            "p6b18: a bot alongside does not break singleplayer");
      info_duration.vec_players.push_back(server::GameInformationPlayer{});
      info_duration.vec_players.back().IsHuman = true;
      Check(!info_duration.IsSinglePlayer(),
            "p6b18: two humans are multiplayer");
    }

    {
      net::Session lobby{};
      lobby.global_settings.GameTimestep = 40;
      lobby.vec_slots.emplace_back("Multi0", net::SessionSlot{});
      lobby.vec_slots.back().second.PlayerReference = "Multi0";
      net::SessionClient client_host{};
      client_host.Index = 0;
      client_host.Slot = "Multi0";
      client_host.Name = "Host";
      net::SessionClient client_bot{};
      client_bot.Index = 1;
      client_bot.Slot = "Multi1";
      client_bot.Bot = "bot1";
      client_bot.Name = "Easy AI";
      client_bot.BotControllerClientIndex = 0;
      lobby.vec_clients = {client_host, client_bot};

      net::GameSave save{};
      save.opt_global_settings = lobby.global_settings;
      save.vec_slots = lobby.vec_slots;
      save.vec_slot_clients.emplace_back(
          "Multi0", net::GameSaveSlotClient{client_host});
      save.vec_slot_clients.emplace_back(
          "Multi1", net::GameSaveSlotClient{client_bot});
      save.vec_clients_by_slot_index = {0, 1};
      save.int4_first_bot_slot_index = 1;
      map::MapGenerationArgs gen_args{};
      gen_args.str_uid = "gen-uid";
      gen_args.str_generator = "GeneratorA";
      gen_args.str_tileset = "DESERT";
      gen_args.size_ = Size{128, 128};
      gen_args.str_title = "T";
      gen_args.str_author = "A";
      gen_args.kind_preview_visibility =
          map::MapGenerationArgs::PreviewVisibilityFlags::MapChooser;
      gen_args.vec_options = {{"k", "v"}};
      save.opt_map_generation_args = gen_args;
      save.vec_trait_data.emplace_back(
          0, yaml::MiniYaml{new std::string{"trait0"}});

      std::vector<std::uint8_t> vec_sync(13, 0);
      vec_sync[0] = 0x65;
      save.DispatchOrders(0, 5, vec_sync);
      save.DispatchOrders(0, 10, {0x01, 0x02});
      save.DispatchOrders(99, 11, {0x03});
      save.DispatchOrders(0, 9, {0x04});
      CheckEq(save.LastOrdersFrame, 11,
              "p6b18: stale frames do not advance LastOrdersFrame");

      std::error_code ec{};
      std::filesystem::create_directories("Saves-test", ec);
      const std::string str_save_path =
          (std::filesystem::path{"Saves-test"} / "t.orasav").generic_string();
      Check(save.Save(str_save_path), "p6b18: the save writes to disk");

      auto up_loaded = net::GameSave::Load(str_save_path);
      Check(up_loaded.has_value(),
            "p6b18: the save reloads from disk");
      if (up_loaded.has_value()) {
        CheckEq(up_loaded->LastOrdersFrame, 11,
                "p6b18: LastOrdersFrame survives the roundtrip");
        CheckEq(up_loaded->LastSyncFrame, 5,
                "p6b18: LastSyncFrame survives the roundtrip");
        Check(up_loaded->opt_global_settings.has_value() &&
                  up_loaded->opt_global_settings->GameTimestep == 40,
              "p6b18: the global settings roundtrip");
        CheckEq(up_loaded->vec_slots.size(), std::size_t{1},
                "p6b18: the slots roundtrip");
        CheckEq(up_loaded->vec_slot_clients.size(), std::size_t{2},
                "p6b18: the slot clients roundtrip");
        Check(up_loaded->vec_slot_clients.size() == 2 &&
                  up_loaded->vec_slot_clients[1].second.str_bot == "bot1" &&
                  up_loaded->vec_slot_clients[1].second.str_bot_name ==
                      "Easy AI",
              "p6b18: the bot slot client keeps its name");
        Check(up_loaded->opt_map_generation_args.has_value() &&
                  up_loaded->opt_map_generation_args->str_generator ==
                      "GeneratorA" &&
                  up_loaded->opt_map_generation_args->size_.Width == 128,
              "p6b18: the map generation args roundtrip");
        Check(up_loaded->vec_trait_data.size() == 1 &&
                  up_loaded->vec_trait_data[0].first == 0,
              "p6b18: the trait data roundtrip");

        up_loaded->vec_clients_by_slot_index = {0, 1};
        up_loaded->int4_first_bot_slot_index = 1;
        std::vector<std::tuple<int, int, std::vector<std::uint8_t>>>
            vec_packets;
        up_loaded->ParseOrders(lobby,
                               [&](int frame, int client,
                                   const std::vector<std::uint8_t>& data) {
                                 vec_packets.emplace_back(frame, client, data);
                               });
        CheckEq(vec_packets.size(), std::size_t{5},
                "p6b18: trait data + three orders + the sync tail");
        if (vec_packets.size() == 5) {
          Check(std::get<0>(vec_packets[0]) == 0 &&
                    !std::get<2>(vec_packets[0]).empty(),
                "p6b18: the SaveTraitData packet leads");
          Check(std::get<0>(vec_packets[1]) == 5 &&
                    std::get<2>(vec_packets[1])[0] == 0x65,
                "p6b18: the in-stream sync order replays in order");
          Check(std::get<0>(vec_packets[2]) == 10 &&
                    std::get<1>(vec_packets[2]) == 0 &&
                    std::get<2>(vec_packets[2]) ==
                        std::vector<std::uint8_t>{0x01, 0x02},
                "p6b18: the host order replays at its frame");
          Check(std::get<0>(vec_packets[3]) == 11 &&
                    std::get<1>(vec_packets[3]) == 0,
                "p6b18: the spectator bot order remaps to the controller");
          Check(std::get<0>(vec_packets[4]) == 5 &&
                    std::get<2>(vec_packets[4])[0] == 0x65,
                "p6b18: the sync packet closes the stream");
        }
      }
      std::filesystem::remove_all("Saves-test", ec);
    }

    {
      server::GameInformation info{};
      info.Mod = mod_data.ManifestRef().Id();
      info.Version = mod_data.ManifestRef().Metadata().str_version;
      info.MapUid = "xxxxxxxx";
      info.MapTitle = "Map A";
      info.StartTimeUtc = server::GameDateTime::UtcNow();
      info.EndTimeUtc = server::GameDateTime::UtcNow();
      server::GameInformationPlayer player_a{};
      player_a.IsHuman = true;
      player_a.Name = "Alice";
      player_a.Team = 0;
      player_a.Outcome = server::WinState::Won;
      player_a.FactionName = "Allies";
      info.vec_players.push_back(player_a);

      const std::string str_payload = info.Serialize();
      std::vector<std::uint8_t> vec_bytes{0x01, 0x02, 0x03, 0x04};
      net::ReplayMetadata meta{str_payload};
      meta.Write(vec_bytes);

      std::error_code ec{};
      std::filesystem::remove_all("Replays", ec);
      const std::string str_replays_dir =
          std::string{"Replays/"} + mod_data.ManifestRef().Id() + "/" +
          mod_data.ManifestRef().Metadata().str_version;
      std::filesystem::create_directories(str_replays_dir, ec);
      const std::string str_replay_a =
          (std::filesystem::path{str_replays_dir} / "game-a.orarep")
              .generic_string();
      {
        std::FILE* file = std::fopen(str_replay_a.c_str(), "wb");
        std::fwrite(vec_bytes.data(), 1, vec_bytes.size(), file);
        std::fclose(file);
      }

      auto up_meta = net::ReplayMetadata::ReadFile(str_replay_a);
      Check(up_meta != nullptr, "p6b18: the replay metadata file reads");
      if (up_meta != nullptr) {
        const auto info_parsed = server::GameInformation::Deserialize(
            up_meta->GameInfoData(), up_meta->FilePath());
        Check(info_parsed.has_value(),
              "p6b18: the game info parses from the replay metadata");
        if (info_parsed.has_value()) {
          Check(info_parsed->IsSinglePlayer(),
                "p6b18: the replay is singleplayer");
          CheckEq(
              mods::GameInfoResolvedPlayerName(*info_parsed,
                                               info_parsed->vec_players[0]),
              std::string{"Alice"},
              "p6b18: the human resolves to the plain name");
          Check(info_parsed->vec_players[0].Outcome ==
                    server::WinState::Won,
                "p6b18: the outcome survives the roundtrip");
        }
        Check(up_meta->RenameFile("game-a-renamed"),
              "p6b18: the replay renames on disk");
        Check(std::filesystem::exists(
                  std::filesystem::path{str_replays_dir} /
                      "game-a-renamed.orarep",
                  ec),
              "p6b18: the renamed file exists");
      }

      server::GameInformation info_b{};
      info_b.Mod = info.Mod;
      info_b.Version = info.Version;
      info_b.MapUid = info.MapUid;
      info_b.MapTitle = "Map B";
      info_b.StartTimeUtc = info.StartTimeUtc;
      info_b.EndTimeUtc = info.EndTimeUtc;
      server::GameInformationPlayer player_b1{};
      player_b1.IsHuman = true;
      player_b1.Name = "Bob";
      player_b1.Team = 1;
      info_b.vec_players.push_back(player_b1);
      server::GameInformationPlayer player_b2{};
      player_b2.IsBot = true;
      player_b2.Name = "Easy AI";
      player_b2.BotType = "bot1";
      player_b2.Team = 1;
      info_b.vec_players.push_back(player_b2);
      server::GameInformationPlayer player_b3{};
      player_b3.IsHuman = true;
      player_b3.Name = "Carol";
      player_b3.Team = 1;
      info_b.vec_players.push_back(player_b3);
      {
        std::vector<std::uint8_t> vec_b{0x05, 0x06};
        net::ReplayMetadata meta_b{info_b.Serialize()};
        meta_b.Write(vec_b);
        const std::string str_replay_b =
            (std::filesystem::path{str_replays_dir} / "game-b.orarep")
                .generic_string();
        std::FILE* file = std::fopen(str_replay_b.c_str(), "wb");
        std::fwrite(vec_b.data(), 1, vec_b.size(), file);
        std::fclose(file);
      }
      CheckEq(mods::GameInfoResolvedPlayerName(info_b,
                                               info_b.vec_players[1]),
              std::string{"enumerated-bot-name"},
              "p6b18: the bot name falls back to the fluent key");

      {
        struct PromptStub {
          std::vector<std::unique_ptr<ui::Widget>> vec_owned;
          ui::Widget* MakeTree(const std::string& str_id) {
            ui::WidgetArgs args_stub;
            auto up_root = std::make_unique<ui::ContainerWidget>();
            up_root->str_id = str_id;
            auto attach = [&](const char* str_child, bool b_button) {
              std::unique_ptr<ui::Widget> up_child;
              if (b_button)
                up_child = std::make_unique<ui::ButtonWidget>(args_stub);
              else
                up_child = std::make_unique<ui::LabelWidget>(args_stub);
              up_child->str_id = str_child;
              up_root->AddChild(std::move(up_child));
            };
            attach("PROMPT_TITLE", false);
            attach("PROMPT_TEXT", false);
            attach("CONFIRM_BUTTON", true);
            attach("CANCEL_BUTTON", true);
            attach("OTHER_BUTTON", true);
            ui::Widget* ptr_root = up_root.get();
            vec_owned.push_back(std::move(up_root));
            return ptr_root;
          }
        };
        auto up_stub = std::make_unique<PromptStub>();
        PromptStub* ptr_stub = up_stub.get();
        static std::unique_ptr<PromptStub> up_stub_holder;
        up_stub_holder = std::move(up_stub);
        static ui::ChromeMetrics metrics_browser18;
        {
          const auto dir_metrics_browser =
              std::filesystem::temp_directory_path() /
              "ora_render_test_metrics_browser18";
          std::filesystem::create_directories(dir_metrics_browser);
          {
            std::ofstream out_metrics(dir_metrics_browser / "metrics.yaml");
            out_metrics << "Chrome:" << '\n';
            out_metrics << "\tTextFont: Regular" << '\n';
            out_metrics << "\tTextColor: FFFFFFFF" << '\n';
            out_metrics << "\tTextContrast: True" << '\n';
            out_metrics << "\tTextShadow: False" << '\n';
            out_metrics << "\tTextContrastColorDark: 000000FF" << '\n';
            out_metrics << "\tTextContrastColorLight: FFFFFFFF" << '\n';
            out_metrics << "\tTextContrastRadius: 1" << '\n';
            out_metrics << "\tButtonDepth: 1" << '\n';
            out_metrics << "\tButtonFont: Regular" << '\n';
            out_metrics << "\tButtonTextColor: FFFFFFFF" << '\n';
            out_metrics << "\tButtonTextColorDisabled: 808080FF" << '\n';
            out_metrics << "\tButtonTextContrast: True" << '\n';
            out_metrics << "\tButtonTextShadow: False" << '\n';
            out_metrics << "\tButtonTextContrastColorDark: 000000FF" << '\n';
            out_metrics << "\tButtonTextContrastColorLight: FFFFFFFF" << '\n';
            out_metrics << "\tButtonTextContrastRadius: 1" << '\n';
            out_metrics << "\tButtonCursor: default" << '\n';
            out_metrics << "\tClickSound: click_sound" << '\n';
            out_metrics << "\tClickDisabledSound: click_disabled_sound" << '\n';
          }
          fs::FileSystem fs_metrics_browser;
          fs_metrics_browser.Mount(dir_metrics_browser.generic_string());
          const std::vector<std::string> vec_files_metrics_browser{
              "metrics.yaml"};
          metrics_browser18.Initialize(
              {&fs_metrics_browser, &vec_files_metrics_browser});
        }
        ui::Ui::Deps deps_browser;
        deps_browser.ptr_chrome_metrics = &metrics_browser18;
        deps_browser.ptr_map_cache = &cache;
        deps_browser.fn_window_size = [] { return int2{1280, 720}; };
        deps_browser.fn_load_widget =
            [ptr_stub](ui::WidgetArgs&, ui::Widget*, const std::string& str_id)
            -> ui::Widget* { return ptr_stub->MakeTree(str_id); };
        ui::Ui::Instance().Initialize(std::move(deps_browser));
      }

      const std::string str_uid_available = [&] {
        for (map::MapPreview* ptr_preview : cache.Previews())
          if (ptr_preview->Status() == map::MapStatus::Available)
            return ptr_preview->Uid();
        return std::string{"none"};
      }();
      server::GameInformation info_ok = info;
      info_ok.MapUid = str_uid_available;
      net::ReplayMetadata meta_ok{info_ok.Serialize()};
      Check(mods::PromptConfirmReplayCompatibility(meta_ok, mod_data),
            "p6b18: a matching replay passes the compatibility gate");
      server::GameInformation info_bad_version = info_ok;
      info_bad_version.Version = "not-the-version";
      net::ReplayMetadata meta_bad{info_bad_version.Serialize()};
      Check(!mods::PromptConfirmReplayCompatibility(meta_bad, mod_data,
                                                    nullptr),
            "p6b18: the mismatched version fails the gate");

      mods::ReplayBrowserLogic::Filter filter_test{};
      const auto apply_filter = [&filter_test] {
        mods::ReplayBrowserLogic::SetStaticFilterForTest(filter_test);
        return mods::ReplayBrowserLogic::EvaluateReplayVisibility;
      };
      (void)apply_filter;
      const auto eval_with =
          [&filter_test](
              const server::GameInformation& info_eval) -> bool {
        mods::ReplayBrowserLogic::SetStaticFilterForTest(filter_test);
        return mods::ReplayBrowserLogic::EvaluateReplayVisibility(info_eval);
      };
      filter_test.kind_type =
          mods::ReplayBrowserLogic::GameType::Singleplayer;
      Check(!eval_with(info_b),
            "p6b18: a multiplayer replay hides under the singleplayer "
            "filter");
      Check(eval_with(info),
            "p6b18: a singleplayer replay stays visible");
      filter_test.kind_type = mods::ReplayBrowserLogic::GameType::Any;
      filter_test.str_player_name = "Bob";
      Check(eval_with(info_b),
            "p6b18: the player filter matches by name");
      filter_test.str_player_name = "Alice";
      Check(!eval_with(info_b),
            "p6b18: the player filter rejects a missing player");
      filter_test.str_player_name.clear();
      filter_test.kind_duration =
          mods::ReplayBrowserLogic::DurationType::VeryShort;
      server::GameInformation info_long = info;
      info_long.StartTimeUtc.str_text = "2026-01-01 00-00-00";
      info_long.EndTimeUtc.str_text = "2026-01-01 01-00-00";
      Check(!eval_with(info_long),
            "p6b18: an hour-long replay hides under the very-short filter");
      filter_test.kind_duration = mods::ReplayBrowserLogic::DurationType::Any;
      filter_test.kind_date =
          mods::ReplayBrowserLogic::DateType::LastMonth;
      server::GameInformation info_old = info;
      info_old.StartTimeUtc.str_text = "2020-01-01 00-00-00";
      info_old.EndTimeUtc.str_text = "2020-01-01 01-00-00";
      Check(!eval_with(info_old),
            "p6b18: an old replay hides under the date filter");
      mods::ReplayBrowserLogic::SetStaticFilterForTest({});
    }
    std::error_code ec_cleanup{};
    std::filesystem::remove_all("Replays", ec_cleanup);
  }


  // ———— Phase 6 第十九批:浏览器面板第二组(GameSave 浏览器 + 任务浏览器 +
  //      地图选择器 + MusicPlaylist/MusicPlayer)————
  // ———— Phase 6 batch 19: the second browser-panel group (the game-save
  //      browser + mission browser + map chooser + MusicPlaylist/
  // ———— Phase 6 第十九批:浏览器面板第二组(GameSave 浏览器 + 任务浏览器 +
  //      地图选择器 + MusicPlaylist/MusicPlayer)————
  // ———— Phase 6 batch 19: the second browser-panel group (the game-save
  //      browser + mission browser + map chooser + MusicPlaylist/
  //      MusicPlayer) ————
  {
    std::println("-- batch 19: browser panels ii + music playlist --");

    {
      CheckEq(mods::MapChooserMapSizeLabel(Size{128, 128}),
              std::string{"128x128 (label-map-size-huge)"},
              "p6b19: a 128x128 map labels huge");
      CheckEq(mods::MapChooserMapSizeLabel(Size{90, 90}),
              std::string{"90x90 (label-map-size-large)"},
              "p6b19: a 90x90 map labels large");
      CheckEq(mods::MapChooserMapSizeLabel(Size{60, 60}),
              std::string{"60x60 (label-map-size-medium)"},
              "p6b19: a 60x60 map labels medium");
      CheckEq(mods::MapChooserMapSizeLabel(Size{59, 59}),
              std::string{"59x59 (label-map-size-small)"},
              "p6b19: a 59x59 map labels small");

      const auto pair_split = mods::SplitOnFirstToken("first\nsecond");
      Check(pair_split.first.has_value() && *pair_split.first == "first" &&
                pair_split.second.has_value() &&
                *pair_split.second == "second",
            "p6b19: SplitOnFirstToken splits on the first newline");
      const auto pair_nosplit = mods::SplitOnFirstToken("only");
      Check(pair_nosplit.first.has_value() &&
                *pair_nosplit.first == "only" &&
                !pair_nosplit.second.has_value(),
            "p6b19: SplitOnFirstToken passes a token-free line through");
      const auto pair_empty = mods::SplitOnFirstToken("");
      Check(!pair_empty.first.has_value() && !pair_empty.second.has_value(),
            "p6b19: SplitOnFirstToken empties out on empty input");
      const auto pair_leading = mods::SplitOnFirstToken("\nsecond");
      Check(pair_leading.first.has_value() &&
            !pair_leading.second.has_value(),
            "p6b19: SplitOnFirstToken keeps a zeroth-token line whole");
    }

    {
      auto& ruleset_music = const_cast<game::Ruleset&>(world->Map().Rules());
      for (auto& [str_key, up_music] : ruleset_music.Music())
        up_music->Load([](const std::string&) { return true; },
                       [](game::MusicInfo& info_song) {
                         info_song.SetLength(120);
                       });
      bool b_installed = false;
      for (const auto& [str_key, up_music] : ruleset_music.Music())
        if (up_music->Exists()) {
          b_installed = true;
          break;
        }
      Check(b_installed, "p6b19: the ra music table loads with Exists set");

      mods::MusicPlaylistInfoData data_playlist{};
      data_playlist.str_background_music = "map";
      data_playlist.str_starting_music = "await_r";
      data_playlist.str_victory_music = "map";
      data_playlist.str_defeat_music = "await_r";

      mods::MusicPlaylistInfoData data_bg = data_playlist;
      mods::MusicPlaylist playlist_bg{*world, std::move(data_bg), nullptr};
      Check(playlist_bg.IsMusicInstalled(),
            "p6b19: the playlist sees installed music");
      Check(playlist_bg.IsMusicAvailable(),
            "p6b19: the playlist sees available music");
      Check(!playlist_bg.CurrentSongIsBackground(),
            "p6b19: StartingMusic overrides the background song");
      Check(playlist_bg.CurrentSong() != nullptr &&
                playlist_bg.CurrentSong()->Title() == "Afterlife (Await)",
            "p6b19: the starting song resolves by key");

      const game::MusicInfo* ptr_next = playlist_bg.GetNextSong();
      Check(ptr_next != nullptr && ptr_next != playlist_bg.CurrentSong(),
            "p6b19: GetNextSong advances the playlist");
      Check(playlist_bg.GetPrevSong() != nullptr,
            "p6b19: GetPrevSong walks back");

      mods::MusicPlaylistInfoData data_bgonly = data_playlist;
      mods::MusicPlaylist playlist_bgonly{*world, std::move(data_bgonly),
                                          nullptr};
      playlist_bgonly.Play(playlist_bgonly.CurrentSong());
      playlist_bgonly.Stop();
      Check(playlist_bgonly.CurrentSongIsBackground(),
            "p6b19: Stop falls back to the background song");

      world->SetLocalPlayerForTest(nullptr);
      mods::MusicPlaylistInfoData data_gameover = data_playlist;
      mods::MusicPlaylist playlist_gameover{*world,
                                            std::move(data_gameover),
                                            nullptr};
      playlist_gameover.GameOver(*world);
      Check(playlist_gameover.CurrentSongIsBackground(),
            "p6b19: GameOver of a playerless world falls to defeat music");

      std::size_t sz_playlist = 0;
      for (const auto& [str_key, up_music] : ruleset_music.Music())
        if (up_music->Exists() && !up_music->Hidden())
          sz_playlist++;
      CheckEq(playlist_gameover.AvailablePlaylist().size(), sz_playlist,
              "p6b19: the playlist hides hidden tracks");
    }

    {
      struct FakeFont19 final : platform::IFont {
        platform::FontGlyph CreateGlyph(std::uint32_t, int, float) override {
          platform::FontGlyph glyph_ret;
          glyph_ret.offset = int2{1, 2};
          glyph_ret.int4_width = 2;
          glyph_ret.int4_height = 3;
          glyph_ret.fp4_advance = 4.0f;
          glyph_ret.vec_data.assign(2 * 3, 0);
          return glyph_ret;
        }
      };

      struct PanelStub19 {
        std::vector<std::unique_ptr<ui::Widget>> vec_owned;
        gfx::SheetBuilder builder_sheets{gfx::SheetType::BGRA, 128};
        std::unique_ptr<gfx::SpriteFont> up_font;

        enum Kind {
          kContainer,
          kLabel,
          kButton,
          kScrollPanel,
          kScrollItem,
          kTextField,
          kCheckbox,
          kSlider,
          kDropDown,
          kMapPreview,
          kVideoPlayer,
          kLogicTicker,
          kLabelTooltip,
          kImage
        };

        void MakeFont() {
          up_font = std::make_unique<gfx::SpriteFont>(
              "Regular", std::span<const std::uint8_t>{},
              std::make_unique<FakeFont19>(), 10, 8, 1.0f, builder_sheets,
              [](const gfx::Sprite&, core::Vector3, float, core::Vector3,
                 float) {},
              [](const gfx::Sprite&, core::Vector3, core::Vector3,
                 core::Vector3, core::Vector3, core::Vector3,
                 float) {});
        }

        std::unique_ptr<ui::Widget> MakeNode(std::string_view str_id,
                                             Kind kind_node) {
          ui::WidgetArgs args_stub;
          std::unique_ptr<ui::Widget> up_node;
          switch (kind_node) {
            case kLabel:
              up_node = std::make_unique<ui::LabelWidget>(args_stub);
              break;
            case kButton:
              up_node = std::make_unique<ui::ButtonWidget>(args_stub);
              break;
            case kScrollPanel:
              up_node = std::make_unique<ui::ScrollPanelWidget>(args_stub);
              break;
            case kScrollItem:
              up_node = std::make_unique<ui::ScrollItemWidget>(args_stub);
              break;
            case kTextField:
              up_node = std::make_unique<ui::TextFieldWidget>(args_stub);
              break;
            case kCheckbox:
              up_node = std::make_unique<ui::CheckboxWidget>(args_stub);
              break;
            case kSlider:
              up_node = std::make_unique<ui::SliderWidget>(args_stub);
              break;
            case kDropDown:
              up_node =
                  std::make_unique<ui::DropDownButtonWidget>(args_stub);
              break;
            case kMapPreview:
              up_node = std::make_unique<ui::MapPreviewWidget>();
              break;
            case kVideoPlayer:
              up_node =
                  std::make_unique<mods::VideoPlayerWidget>(args_stub);
              break;
            case kLogicTicker:
              up_node = std::make_unique<ui::LogicTickerWidget>();
              break;
            case kLabelTooltip:
              up_node =
                  std::make_unique<ui::LabelWithTooltipWidget>(args_stub);
              break;
            case kImage:
              up_node = std::make_unique<ui::ImageWidget>();
              break;
            default:
              up_node = std::make_unique<ui::ContainerWidget>();
              break;
          }
          up_node->str_id = std::string{str_id};
          up_node->bounds_widget.int4_width = 200;
          up_node->bounds_widget.int4_height = 20;
          return up_node;
        }

        ui::Widget* MakeRoot() {
          auto up_root = std::make_unique<ui::ContainerWidget>();
          up_root->str_id = "ROOT19";
          ui::Widget* ptr_root = up_root.get();
          vec_owned.push_back(std::move(up_root));
          return ptr_root;
        }

        ui::Widget* Attach(ui::Widget& ref_parent, std::string_view str_id,
                           Kind kind_node) {
          std::unique_ptr<ui::Widget> up_node = MakeNode(str_id, kind_node);
          ui::Widget* ptr_node = up_node.get();
          ref_parent.AddChild(std::move(up_node));
          return ptr_node;
        }

        ui::Widget* MakeTree(
            std::initializer_list<std::pair<std::string_view, int>>
                vec_layout) {
          ui::Widget* ptr_root = MakeRoot();
          for (const auto& [str_id, kind_node] : vec_layout)
            Attach(*ptr_root, str_id, static_cast<Kind>(kind_node));
          return ptr_root;
        }
      };

      auto up_stub = std::make_unique<PanelStub19>();
      up_stub->MakeFont();
      PanelStub19* ptr_stub = up_stub.get();
      static std::unique_ptr<PanelStub19> up_stub_holder19;
      up_stub_holder19 = std::move(up_stub);

      static ui::ChromeMetrics metrics_browser19;
      {
        const auto dir_metrics_browser19 =
            std::filesystem::temp_directory_path() /
            "ora_render_test_metrics_browser19";
        std::filesystem::create_directories(dir_metrics_browser19);
        {
          std::ofstream out_metrics(dir_metrics_browser19 / "metrics.yaml");
          out_metrics << "Chrome:" << '\n';
          out_metrics << "	TextFont: Regular" << '\n';
          out_metrics << "	TextColor: FFFFFFFF" << '\n';
          out_metrics << "	TextContrast: True" << '\n';
          out_metrics << "	TextShadow: False" << '\n';
          out_metrics << "	TextContrastColorDark: 000000FF" << '\n';
          out_metrics << "	TextContrastColorLight: FFFFFFFF" << '\n';
          out_metrics << "	TextContrastRadius: 1" << '\n';
          out_metrics << "	ButtonDepth: 1" << '\n';
          out_metrics << "	ButtonFont: Regular" << '\n';
          out_metrics << "	ButtonTextColor: FFFFFFFF" << '\n';
          out_metrics << "	ButtonTextColorDisabled: 808080FF" << '\n';
          out_metrics << "	ButtonTextContrast: True" << '\n';
          out_metrics << "	ButtonTextShadow: False" << '\n';
          out_metrics << "	ButtonTextContrastColorDark: 000000FF" << '\n';
          out_metrics << "	ButtonTextContrastColorLight: FFFFFFFF" << '\n';
          out_metrics << "	ButtonTextContrastRadius: 1" << '\n';
          out_metrics << "	ButtonCursor: default" << '\n';
          out_metrics << "	ClickSound: click_sound" << '\n';
          out_metrics << "	ClickDisabledSound: click_disabled_sound" << '\n';
          out_metrics << "\tTextDisabledColor: 808080FF" << '\n';
          out_metrics << "\tTextfieldFont: Regular" << '\n';
          out_metrics << "\tTextfieldColor: FFFFFFFF" << '\n';
          out_metrics << "\tTextfieldColorDisabled: 808080FF" << '\n';
          out_metrics << "\tTextfieldColorInvalid: FFFF00FF" << '\n';
          out_metrics << "\tTextfieldColorHighlight: 808080FF" << '\n';
          out_metrics << "\tSpawnFont: Regular" << '\n';
          out_metrics << "\tSpawnColor: FFFFFFFF" << '\n';
          out_metrics << "\tSpawnContrastColor: 000000FF" << '\n';
          out_metrics << "\tSpawnLabelOffset: 0,0" << '\n';
        }
        static fs::FileSystem fs_metrics_browser19;
        fs_metrics_browser19.Mount(
            dir_metrics_browser19.generic_string());
        static const std::vector<std::string>
            vec_files_metrics_browser19{"metrics.yaml"};
        metrics_browser19.Initialize(
            {&fs_metrics_browser19, &vec_files_metrics_browser19});
      }

      static gfx::ChromeProvider chrome_provider19;
      chrome_provider19.Initialize(
          {.ptr_file_system = &mod_data.ModFiles(),
           .vec_chrome_files = &mod_data.ManifestRef().Chrome(),
           .fp4_dpi_scale = 1.0f});

      ui::Ui::Deps deps_browser19;
      deps_browser19.ptr_chrome_provider = &chrome_provider19;
      deps_browser19.ptr_chrome_metrics = &metrics_browser19;
      deps_browser19.ptr_map_cache = &cache;
      deps_browser19.fn_window_size = [] { return int2{1280, 720}; };
      deps_browser19.fn_try_get_font =
          [ptr_font = ptr_stub->up_font.get()](const std::string&) {
            return ptr_font;
          };
      deps_browser19.fn_load_widget =
          [ptr_stub](ui::WidgetArgs&, ui::Widget* ptr_parent,
                     const std::string&) -> ui::Widget* {
        ui::WidgetArgs args_stub;
        auto up_root = std::make_unique<ui::ContainerWidget>();
        up_root->str_id = "LOADED19";
        auto up_player =
            ptr_stub->MakeNode("PLAYER", PanelStub19::kVideoPlayer);
        ui::Widget* ptr_root = up_root.get();
        up_root->AddChild(std::move(up_player));
        ptr_parent->AddChild(std::move(up_root));
        return ptr_root;
      };
      ui::Ui::Instance().Initialize(std::move(deps_browser19));

      const std::string str_saves_dir =
          std::string{"Saves"} + "/" + manifest->Id() + "/" +
          manifest->Metadata().str_version;
      std::error_code ec_saves{};
      std::filesystem::remove_all("Saves", ec_saves);
      std::filesystem::create_directories(str_saves_dir, ec_saves);

      for (const std::string& str_name : {"alpha", "beta"}) {
        net::Session lobby_save{};
        lobby_save.global_settings.GameTimestep = 40;
        lobby_save.vec_slots.emplace_back("Multi0", net::SessionSlot{});
        lobby_save.vec_slots.back().second.PlayerReference = "Multi0";
        net::SessionClient client_host{};
        client_host.Index = 0;
        client_host.Slot = "Multi0";
        client_host.Name = "Alice";
        client_host.SpawnPoint = 1;
        net::SessionClient client_bot{};
        client_bot.Index = 1;
        client_bot.Slot = "Multi1";
        client_bot.Bot = "bot1";
        client_bot.BotControllerClientIndex = 0;
        client_bot.SpawnPoint = 2;
        lobby_save.vec_clients = {client_host, client_bot};

        net::GameSave save_disk{};
        save_disk.opt_global_settings = lobby_save.global_settings;
        save_disk.vec_slots = lobby_save.vec_slots;
        net::GameSaveSlotClient slot_host{client_host};
        slot_host.str_bot_name = "bots.easy";
        net::GameSaveSlotClient slot_bot{client_bot};
        slot_bot.str_bot_name = "bots.easy";
        save_disk.vec_slot_clients.emplace_back("Multi0", slot_host);
        save_disk.vec_slot_clients.emplace_back("Multi1", slot_bot);
        save_disk.DispatchOrders(0, 7, {0x65, 0x00});
        save_disk.DispatchOrders(0, 9, {0x01});
        save_disk.vec_trait_data.emplace_back(
            0, yaml::MiniYaml{new std::string{"trait0"}});
        Check(save_disk.Save((std::filesystem::path{str_saves_dir} /
                              (str_name + ".orasav"))
                                 .generic_string()),
              "p6b19: the save writes for the browser");
      }

      ui::Widget* ptr_panel_save = ptr_stub->MakeTree(
          {{"CANCEL_BUTTON", PanelStub19::kButton},
           {"GAME_LIST", PanelStub19::kScrollPanel},
           {"SAVE_TITLE", PanelStub19::kLabel},
           {"SAVE_BUTTON", PanelStub19::kButton},
           {"SAVE_WIDGETS", PanelStub19::kContainer},
           {"SAVE_INFO", PanelStub19::kContainer},
           {"RENAME_BUTTON", PanelStub19::kButton},
           {"DELETE_BUTTON", PanelStub19::kButton},
           {"DELETE_ALL_BUTTON", PanelStub19::kButton},
           {"MAP_PREVIEW_ROOT", PanelStub19::kContainer}});
      std::println("-- t4a attach --");
      {
        ui::Widget* ptr_tpl_game = ptr_stub->Attach(
            *ptr_panel_save->Get("GAME_LIST"), "GAME_TEMPLATE",
            PanelStub19::kScrollItem);
        ptr_stub->Attach(*ptr_tpl_game, "LABEL", PanelStub19::kLabel);
        ptr_stub->Attach(*ptr_tpl_game, "TITLE",
                         PanelStub19::kLabelTooltip);
        ptr_stub->Attach(*ptr_tpl_game, "CREATION_TIME",
                         PanelStub19::kLabel);
      }
      {
        ui::Widget* ptr_tpl_date = ptr_stub->Attach(
            *ptr_panel_save->Get("GAME_LIST"), "DATE_HEADER",
            PanelStub19::kScrollItem);
        ptr_stub->Attach(*ptr_tpl_date, "LABEL", PanelStub19::kLabel);
      }
      std::println("-- t4b textfield --");
      ptr_stub->Attach(*ptr_panel_save->Get("SAVE_WIDGETS"),
                       "SAVE_TEXTFIELD", PanelStub19::kTextField);
      ptr_stub->Attach(*ptr_panel_save->Get("SAVE_INFO"),
                       "NO_SAVE_SELECTED_LABEL", PanelStub19::kLabel);
      for (const std::string_view str_id :
           {"INCOMPATIBLE_TITLE_LABEL", "INCOMPATIBLE_LABEL_A",
            "INCOMPATIBLE_LABEL_B", "SAVEGAME_INFO_DATE",
            "SAVEGAME_INFO_DURATION"})
        ptr_stub->Attach(*ptr_panel_save->Get("SAVE_INFO"), str_id,
                         PanelStub19::kLabel);
      std::println("-- t4c playerlist --");
      ptr_stub->Attach(*ptr_panel_save->Get("SAVE_INFO"), "PLAYER_LIST",
                       PanelStub19::kScrollPanel);
      {
        ui::Widget* ptr_list_players =
            ptr_panel_save->Get("SAVE_INFO")->Get("PLAYER_LIST");
        ui::Widget* ptr_tpl_header =
            ptr_stub->Attach(*ptr_list_players, "HEADER",
                             PanelStub19::kScrollItem);
        ptr_stub->Attach(*ptr_tpl_header, "LABEL", PanelStub19::kLabel);
        ui::Widget* ptr_tpl_player =
            ptr_stub->Attach(*ptr_list_players, "TEMPLATE",
                             PanelStub19::kScrollItem);
        ptr_stub->Attach(*ptr_tpl_player, "LABEL", PanelStub19::kLabel);
        ptr_stub->Attach(*ptr_tpl_player, "FLAG", PanelStub19::kImage);
      }

      mods::GameSaveBrowserLogic logic_save{*ptr_panel_save, mod_data,
                                            *world, [] {}, [] {}};
      CheckEq(logic_save.GamesForTest().size(), std::size_t{2},
              "p6b19: the save browser lists both saves");
      Check(logic_save.DefaultSaveFilenameForTest() == world->Map().Title(),
            "p6b19: the default save filename comes from the map title");
      Check(logic_save.SelectedPathForTest().empty(),
            "p6b19: no save is selected on entry");
      Check(logic_save.PlayerListLabelsForTest().empty(),
            "p6b19: the world slot clients start empty");

      const std::string str_alpha =
          (std::filesystem::path{str_saves_dir} / "alpha.orasav")
              .generic_string();
      logic_save.SelectForTest(&str_alpha);
      Check(logic_save.SelectedPathForTest() == str_alpha,
            "p6b19: selecting a save records the path");
      CheckEq(logic_save.PlayerListLabelsForTest().size(), std::size_t{3},
              "p6b19: the player list renders header + human + bot");
      Check(logic_save.PlayerListLabelsForTest()[0] == "label-players",
            "p6b19: a one-team save collapses to the players header");
      Check(logic_save.PlayerListLabelsForTest()[1] ==
                    "label-load-game-browser-panel-human-player",
            "p6b19: the human player labels by key fallback");
      Check(logic_save.PlayerListLabelsForTest()[2] ==
                    "enumerated-bot-name",
            "p6b19: the bot player labels enumerated by key fallback");
      CheckEq(logic_save.GetSpawnOccupantsForTest().size(), std::size_t{2},
              "p6b19: spawn occupants cover both slot clients");

      const std::string str_tooltip =
          mods::BuildSaveTooltipText(str_alpha, nullptr, mod_data);
      Check(str_tooltip.find("tooltip-savegame-map") != std::string::npos,
            "p6b19: the save tooltip names the map line");
      Check(str_tooltip.find("tooltip-savegame-players: 0") !=
                std::string::npos,
            "p6b19: a null save tooltip counts zero players");

      logic_save.DeleteForTest(str_alpha);
      CheckEq(logic_save.GamesForTest().size(), std::size_t{1},
              "p6b19: deleting a save drops it from the list");

      {
        ui::Widget* ptr_panel_music = ptr_stub->MakeTree(
            {{"MUSIC_LIST", PanelStub19::kScrollPanel},
             {"NO_MUSIC_LABEL", PanelStub19::kLabel},
             {"MUTE_LABEL", PanelStub19::kLabel},
             {"BUTTON_PLAY", PanelStub19::kButton},
             {"BUTTON_PAUSE", PanelStub19::kButton},
             {"BUTTON_STOP", PanelStub19::kButton},
             {"BUTTON_NEXT", PanelStub19::kButton},
             {"BUTTON_PREV", PanelStub19::kButton},
             {"SHUFFLE", PanelStub19::kCheckbox},
             {"REPEAT", PanelStub19::kCheckbox},
             {"TIME_LABEL", PanelStub19::kLabel},
             {"TITLE_LABEL", PanelStub19::kLabel},
             {"MUSIC_SLIDER", PanelStub19::kSlider},
             {"SONG_WATCHER", PanelStub19::kLogicTicker},
             {"BACK_BUTTON", PanelStub19::kButton}});
        {
          ui::Widget* ptr_tpl_music = ptr_stub->Attach(
              *ptr_panel_music->Get("MUSIC_LIST"), "MUSIC_TEMPLATE",
              PanelStub19::kScrollItem);
          ptr_stub->Attach(*ptr_tpl_music, "TITLE",
                           PanelStub19::kLabelTooltip);
          ptr_stub->Attach(*ptr_tpl_music, "LENGTH", PanelStub19::kLabel);
        }

        mods::MusicPlayerLogic logic_music{*ptr_panel_music, *world,
                                           mod_data, [] {}};
        Check(logic_music.SongTitlesForTest().empty(),
              "p6b19: the world playlist stays empty until PrepareMap "
              "loads music");
        Check(ptr_panel_music->Get<ui::ButtonWidget>("BUTTON_PLAY")
                      ->fn_is_disabled(),
              "p6b19: the play button disables without a current song");
        CheckEq(ptr_panel_music->Get<ui::SliderWidget>("MUSIC_SLIDER")
                    ->fp4_value,
                0.5f,
                "p6b19: the music slider seeds at the default volume");
      }

      {
        ui::Widget* ptr_panel_chooser = ptr_stub->MakeTree(
            {{"BUTTON_OK", PanelStub19::kButton},
             {"BUTTON_CANCEL", PanelStub19::kButton},
             {"GAMEMODE_FILTER", PanelStub19::kDropDown},
             {"MAP_TEMPLATE", PanelStub19::kScrollItem},
             {"FILTER_ORDER_CONTROLS", PanelStub19::kContainer},
             {"MAPFILTER_INPUT", PanelStub19::kTextField},
             {"RANDOMMAP_BUTTON", PanelStub19::kButton},
             {"DELETE_MAP_BUTTON", PanelStub19::kButton},
             {"DELETE_ALL_MAPS_BUTTON", PanelStub19::kButton},
             {"REMOTE_MAP_LABEL", PanelStub19::kLabel},
             {"ORDERBY", PanelStub19::kDropDown},
             {"BUTTON1", PanelStub19::kButton},
             {"BUTTON2", PanelStub19::kButton},
             {"BUTTON3", PanelStub19::kButton},
             {"USER_MAPS_TAB", PanelStub19::kContainer},
             {"SYSTEM_MAPS_TAB", PanelStub19::kContainer},
             {"REMOTE_MAPS_TAB", PanelStub19::kContainer}});
        for (const std::string_view str_tab :
             {"USER_MAPS_TAB", "SYSTEM_MAPS_TAB", "REMOTE_MAPS_TAB"})
          ptr_stub->Attach(*ptr_panel_chooser->Get(str_tab), "MAP_LIST",
                           PanelStub19::kScrollPanel);

        ui::Ui::Instance().OpenWindow("MAPCHOOSER_PANEL", ui::WidgetArgs{});

        std::string str_picked;
        mods::MapChooserLogic logic_chooser{
            *ptr_panel_chooser,
            mod_data,
            nullptr,
            nullptr,
            nullptr,
            map::MapClassification::System,
            [] {},
            [&](const std::string& str_uid) { str_picked = str_uid; },
            nullptr,
            map::MapVisibility::Lobby};
        Check(!logic_chooser.VisibleMapsForTest().empty(),
              "p6b19: the chooser enumerates available maps");
        Check(!logic_chooser.SelectedUidForTest().empty(),
              "p6b19: the chooser pre-selects a map");
        Check(logic_chooser.CurrentTabForTest() !=
                  map::MapClassification::Unknown,
              "p6b19: the chooser lands on a known tab");

        ptr_panel_chooser->Get<ui::ButtonWidget>("BUTTON_OK")->fn_on_click();
        Check(str_picked == logic_chooser.SelectedUidForTest(),
              "p6b19: OK hands the selected uid to onSelect");
      }

      {
        struct MissionRun19 {
          bool b_minified = false;
          bool b_difficulty_disabled = false;
          bool b_start_disabled = false;
        };

        const auto run_mission_logic =
            [ptr_stub, &mod_data, &world](bool b_select_first_map) {
          ui::Widget* ptr_panel_mission = ptr_stub->MakeTree(
              {{"MISSION_LIST", PanelStub19::kScrollPanel},
               {"MISSIONBROWSER_TITLE", PanelStub19::kLabel},
               {"MISSION_INFO", PanelStub19::kContainer},
               {"MISSION_PREVIEW", PanelStub19::kMapPreview},
               {"MISSION_VIDEO", PanelStub19::kVideoPlayer},
               {"MISSION_BIN", PanelStub19::kContainer},
               {"MISSION_DETAIL", PanelStub19::kContainer},
               {"START_BRIEFING_VIDEO_BUTTON", PanelStub19::kButton},
               {"STOP_BRIEFING_VIDEO_BUTTON", PanelStub19::kButton},
               {"START_INFO_VIDEO_BUTTON", PanelStub19::kButton},
               {"STOP_INFO_VIDEO_BUTTON", PanelStub19::kButton},
               {"MISSION_MINIFIED_OPTIONS", PanelStub19::kContainer},
               {"STARTGAME_BUTTON", PanelStub19::kButton},
               {"BACK_BUTTON", PanelStub19::kButton},
               {"MISSION_TABS", PanelStub19::kContainer}});
          ptr_stub->Attach(*ptr_panel_mission->Get("MISSION_LIST"), "HEADER",
                           PanelStub19::kScrollItem);
          ptr_stub->Attach(*ptr_panel_mission->Get("MISSION_LIST"),
                           "TEMPLATE", PanelStub19::kScrollItem);
          ptr_stub->Attach(*ptr_panel_mission->Get("MISSION_DETAIL"),
                           "MISSION_DESCRIPTION_PANEL",
                           PanelStub19::kScrollPanel);
          ptr_stub->Attach(
              *ptr_panel_mission->Get("MISSION_DETAIL")
                   ->Get("MISSION_DESCRIPTION_PANEL"),
              "MISSION_DESCRIPTION", PanelStub19::kLabel);
          ptr_stub->Attach(*ptr_panel_mission->Get("MISSION_DETAIL"),
                           "MISSION_OPTIONS", PanelStub19::kContainer);
          ptr_stub->Attach(
              *ptr_panel_mission->Get("MISSION_DETAIL")
                   ->Get("MISSION_OPTIONS"),
              "CHECKBOX_ROW_TEMPLATE", PanelStub19::kContainer);
          ptr_stub->Attach(
              *ptr_panel_mission->Get("MISSION_DETAIL")
                   ->Get("MISSION_OPTIONS"),
              "DROPDOWN_ROW_TEMPLATE", PanelStub19::kContainer);
          ptr_stub->Attach(
              *ptr_panel_mission->Get("MISSION_MINIFIED_OPTIONS"),
              "DIFFICULTY", PanelStub19::kDropDown);
          ptr_stub->Attach(
              *ptr_panel_mission->Get("MISSION_MINIFIED_OPTIONS"),
              "GAMESPEED", PanelStub19::kDropDown);
          ptr_stub->Attach(*ptr_panel_mission->Get("MISSION_TABS"),
                           "OPTIONS_TAB", PanelStub19::kButton);
          ptr_stub->Attach(*ptr_panel_mission->Get("MISSION_TABS"),
                           "MISSIONINFO_TAB", PanelStub19::kButton);

          mods::MissionBrowserLogic logic_mission{
              *ptr_panel_mission, mod_data, *world, [] {}, [] {},
              std::string{}};
          if (b_select_first_map) {
            map::MapCache* ptr_cache_run =
                ora::ui::Ui::Instance().MapCacheDeps();
            if (ptr_cache_run != nullptr)
              for (map::MapPreview* ptr_preview : ptr_cache_run->Previews())
                if (ptr_preview->Status() == map::MapStatus::Available) {
                  logic_mission.SelectMapForTest(*ptr_preview);
                  break;
                }
          }
          MissionRun19 run{};
          run.b_minified = logic_mission.MinifiedOptionsForTest();
          run.b_difficulty_disabled = logic_mission.DifficultyDisabledForTest();
          run.b_start_disabled =
              ptr_panel_mission->Get<ui::ButtonWidget>("STARTGAME_BUTTON")
                  ->fn_is_disabled();
          return run;
        };

        static std::vector<server::LobbyOptionData> vec_options_injected;
        vec_options_injected.clear();
        server::LobbyOptionData option_difficulty{};
        option_difficulty.Id = "difficulty";
        option_difficulty.Name = "Difficulty";
        option_difficulty.DefaultValue = "easy";
        option_difficulty.vec_values = {{"easy", "Easy"}, {"hard", "Hard"}};
        vec_options_injected.push_back(option_difficulty);
        static std::function<std::vector<server::LobbyOptionData>(
            const map::MapPreview&)>
            fn_collector = [](const map::MapPreview&) {
              return vec_options_injected;
            };
        mods::SetMapLobbyOptionsCollectorForTest(&fn_collector);

        const MissionRun19 run_full = run_mission_logic(true);
        Check(run_full.b_minified,
              "p6b19: difficulty-only options keep the minified panel");
        Check(!run_full.b_difficulty_disabled,
              "p6b19: a difficulty option enables the dropdown");
        Check(!run_full.b_start_disabled,
              "p6b19: the start button enables once a map is selected");

        vec_options_injected.clear();
        const MissionRun19 run_empty = run_mission_logic(true);
        Check(run_empty.b_difficulty_disabled,
              "p6b19: no difficulty option disables the dropdown");
        mods::SetMapLobbyOptionsCollectorForTest(nullptr);
      }

      std::error_code ec_cleanup19{};
      std::filesystem::remove_all("Saves", ec_cleanup19);
    }
  }

  // ———— Phase 6 第二十批:连接链 + 服务器列表 ————
  // ———— Phase 6 batch 20: the connection chain + the server list ————
  {
    std::println("-- batch 20: connection chain + server list --");

    auto& deps_server_game = server::GameServerClientDepsRef();
    const server::GameServerClientDeps deps_server_game_saved = [&] {
      server::GameServerClientDeps deps_copy{};
      deps_copy.fn_external_mod_compatible =
          deps_server_game.fn_external_mod_compatible;
      deps_copy.fn_external_mod_id = deps_server_game.fn_external_mod_id;
      deps_copy.fn_mod_title = deps_server_game.fn_mod_title;
      deps_copy.fn_allow_downloading =
          deps_server_game.fn_allow_downloading;
      deps_copy.fn_map_available = deps_server_game.fn_map_available;
      return deps_copy;
    }();
    deps_server_game.fn_external_mod_compatible =
        [](const std::string& str_mod, const std::string& str_version) {
          return str_mod == "ra" && str_version == "1.0-test";
        };
    deps_server_game.fn_external_mod_id =
        [](const std::string& str_mod, const std::string&) {
          return str_mod == "ra" ? std::optional<std::string>{"Red Alert"}
                                 : std::nullopt;
        };
    deps_server_game.fn_mod_title = [](const std::string& str_mod) {
      return str_mod == "ra" ? std::optional<std::string>{"Installed Mod"}
                             : std::nullopt;
    };
    deps_server_game.fn_allow_downloading = [] { return true; };
    deps_server_game.fn_map_available = [](const std::string& str_map) {
      return str_map == "map-uid-b";
    };

    {
      const auto make_yaml = [](std::string_view sv_body) {
        return yaml::MiniYaml::FromString(
            std::string{sv_body}, "test20",
            true, yaml::MiniYaml::GlobalPool());
      };

      const auto opt_alpha = server::GameServerData::TryFromYaml(
          make_yaml(
              "Game:\n"
              "\tProtocol: 2\n"
              "\tName: Alpha Server\n"
              "\tAddress: 10.0.0.1:1234\n"
              "\tPort: 1234\n"
              "\tState: 1\n"
              "\tMaxPlayers: 4\n"
              "\tMap: map-uid-a\n"
              "\tMod: ra\n"
              "\tVersion: 1.0-test\n"
              "\tProtected: True\n"
              "\tAuthentication: False\n"
              "\tPlayers: 2\n"
              "\tBots: 1\n"
              "\tSpectators: 1\n"
              "\tLocation: Testland\n"
              "\tDisabledSpawnPoints: 1, 3\n"
              "\tClients:\n"
              "\t\tClient@0:\n"
              "\t\t\tName: Alice\n"
              "\t\t\tColor: FFFF0000\n"
              "\t\t\tFaction: soviet\n"
              "\t\t\tTeam: 1\n"
              "\t\t\tSpawnPoint: 2\n"
              "\t\t\tIsAdmin: True\n"
              "\t\t\tIsSpectator: False\n"
              "\t\t\tIsBot: False\n"
              "\t\tClient@1:\n"
              "\t\t\tName: bots.easy\n"
              "\t\t\tTeam: 0\n"
              "\t\t\tSpawnPoint: 3\n"
              "\t\t\tIsBot: True\n"
              "\t\t\tPlayer: 0\n")
              .front()
              .Value);
      Check(opt_alpha.has_value(),
            "p6b20: a full server yaml parses");
      if (opt_alpha.has_value()) {
        CheckEq(opt_alpha->Name, std::string{"Alpha Server"},
                "p6b20: the name field loads");
        CheckEq(opt_alpha->Address, std::string{"10.0.0.1:1234"},
                "p6b20: the address field loads");
        CheckEq(opt_alpha->State, 1, "p6b20: the state field loads");
        CheckEq(opt_alpha->Players, 2, "p6b20: the players field loads");
        Check(opt_alpha->Protected && !opt_alpha->Authentication,
              "p6b20: the bool pair loads");
        CheckEq(opt_alpha->vec_disabled_spawn_points.size(),
                std::size_t{2},
                "p6b20: the disabled spawn points split");
        CheckEq(opt_alpha->vec_disabled_spawn_points[1], 3,
                "p6b20: the second disabled spawn point parses");
        CheckEq(opt_alpha->vec_clients.size(), std::size_t{2},
                "p6b20: the client nodes load");
        CheckEq(opt_alpha->vec_clients[0].Faction, std::string{"soviet"},
                "p6b20: the client faction loads");
        Check(opt_alpha->vec_clients[0].IsAdmin &&
                  !opt_alpha->vec_clients[0].IsSpectator,
              "p6b20: the client bools load");
        Check(opt_alpha->vec_clients[1].IsBot,
              "p6b20: the bot flag loads");
        Check(opt_alpha->IsCompatible,
              "p6b20: the external-mod dep marks compatible");
        Check(opt_alpha->IsJoinable,
              "p6b20: waiting + compatible + allow-downloading joins");
        CheckEq(opt_alpha->ModLabel(), std::string{"Red Alert (1.0-test)"},
                "p6b20: the mod label joins title and version");
      }

      const auto opt_playing = server::GameServerData::TryFromYaml(
          make_yaml(
              "Game:\n"
              "\tAddress: 10.0.0.2:1235\n"
              "\tState: 2\n"
              "\tMap: map-uid-b\n"
              "\tMod: ra\n"
              "\tVersion: 1.0-test\n"
              "\tModTitle: Red Alert\n"
              "\tStarted: 2000-01-01T00:00:00\n"
              "\tPlayTime: 600\n")
              .front()
              .Value);
      Check(opt_playing.has_value() && opt_playing->PlayTime == 600 &&
                opt_playing->IsCompatible && !opt_playing->IsJoinable,
            "p6b20: an explicit play time survives and a started game "
            "stays unjoinable");

      const auto opt_backcalc = server::GameServerData::TryFromYaml(
          make_yaml(
              "Game:\n"
              "\tAddress: 10.0.0.2:1235\n"
              "\tState: 2\n"
              "\tMap: map-uid-b\n"
              "\tMod: ra\n"
              "\tVersion: 1.0-test\n"
              "\tModTitle: Red Alert\n"
              "\tStarted: 2000-01-01T00:00:00\n")
              .front()
              .Value);
      Check(opt_backcalc.has_value() && opt_backcalc->PlayTime > 1000,
            "p6b20: a missing play time back-calculates from Started");

      const auto opt_legacy = server::GameServerData::TryFromYaml(
          make_yaml(
              "Game:\n"
              "\tAddress: 10.0.0.3:1236\n"
              "\tState: 1\n"
              "\tMap: map-uid-b\n"
              "\tMod:\n"
              "\tVersion:\n"
              "\tMods: ra@1.0-test\n")
              .front()
              .Value);
      Check(opt_legacy.has_value() &&
                opt_legacy->Mod == "ra" &&
                opt_legacy->Version == "1.0-test" &&
                opt_legacy->ModTitle == "Red Alert",
            "p6b20: the legacy Mods@ field falls back");

      const auto opt_unknown_mod = server::GameServerData::TryFromYaml(
          make_yaml(
              "Game:\n"
              "\tAddress: 10.0.0.4:1237\n"
              "\tState: 1\n"
              "\tMap: map-uid-b\n"
              "\tMod: cnc\n"
              "\tVersion: 9.9\n")
              .front()
              .Value);
      Check(opt_unknown_mod.has_value() &&
                opt_unknown_mod->ModTitle == "Unknown mod: cnc" &&
                !opt_unknown_mod->IsCompatible,
            "p6b20: an unknown mod titles with the formatted fallback");

      const auto opt_bad_int = server::GameServerData::TryFromYaml(
          make_yaml(
              "Game:\n"
              "\tAddress: 10.0.0.5:1238\n"
              "\tState: abc\n"
              "\tMap: map-uid-b\n"
              "\tMod: ra\n"
              "\tVersion: 1.0-test\n")
              .front()
              .Value);
      Check(!opt_bad_int.has_value(),
            "p6b20: a bad int value drops the entry");

      const auto opt_bad_client = server::GameServerData::TryFromYaml(
          make_yaml(
              "Game:\n"
              "\tAddress: 10.0.0.6:1239\n"
              "\tState: 1\n"
              "\tMap: map-uid-b\n"
              "\tMod: ra\n"
              "\tVersion: 1.0-test\n"
              "\tClients:\n"
              "\t\tClient@0:\n"
              "\t\t\tName: Alice\n"
              "\t\t\tTeam: oops\n")
              .front()
              .Value);
      Check(!opt_bad_client.has_value(),
            "p6b20: a bad client value drops the whole entry");

      const auto vec_null_address = yaml::MiniYaml::FromString(
          "Game:\n\tAddress:\n\tState: 1\n\tMap: map-uid-b\n\tMod: ra\n"
          "\tVersion: 1.0-test\n",
          "test20", true, yaml::MiniYaml::GlobalPool());
      const auto opt_null_address = server::GameServerData::TryFromYaml(
          vec_null_address.front().Value);
      Check(opt_null_address.has_value() &&
                opt_null_address->b_address_null,
            "p6b20: a null address node flags the drop gate");
    }

    deps_server_game.fn_map_available = [](const std::string&) {
      return false;
    };

    {
      static ui::ChromeMetrics metrics_server_list20;
      {
        const auto dir_metrics20 = std::filesystem::temp_directory_path() /
                                   "ora_render_test_metrics_server20";
        std::filesystem::create_directories(dir_metrics20);
        {
          std::ofstream out_metrics(dir_metrics20 / "metrics.yaml");
          out_metrics << "Chrome:" << '\n';
          out_metrics << "	TextFont: Regular" << '\n';
          out_metrics << "	TextColor: FFFFFFFF" << '\n';
          out_metrics << "	TextContrast: True" << '\n';
          out_metrics << "	TextShadow: False" << '\n';
          out_metrics << "	TextContrastColorDark: 000000FF" << '\n';
          out_metrics << "	TextContrastColorLight: FFFFFFFF" << '\n';
          out_metrics << "	TextContrastRadius: 1" << '\n';
          out_metrics << "	ButtonDepth: 1" << '\n';
          out_metrics << "	ButtonFont: Regular" << '\n';
          out_metrics << "	ButtonTextColor: FFFFFFFF" << '\n';
          out_metrics << "	ButtonTextColorDisabled: 808080FF" << '\n';
          out_metrics << "	ButtonTextContrast: True" << '\n';
          out_metrics << "	ButtonTextShadow: False" << '\n';
          out_metrics << "	ButtonTextContrastColorDark: 000000FF" << '\n';
          out_metrics << "	ButtonTextContrastColorLight: FFFFFFFF" << '\n';
          out_metrics << "	ButtonTextContrastRadius: 1" << '\n';
          out_metrics << "	ButtonCursor: default" << '\n';
          out_metrics << "	ClickSound: click_sound" << '\n';
          out_metrics << "	ClickDisabledSound: click_disabled_sound" << '\n';
          out_metrics << "\tTextDisabledColor: 808080FF" << '\n';
          out_metrics << "\tTextfieldFont: Regular" << '\n';
          out_metrics << "\tTextfieldColor: FFFFFFFF" << '\n';
          out_metrics << "\tTextfieldColorDisabled: 808080FF" << '\n';
          out_metrics << "\tTextfieldColorInvalid: FFFF00FF" << '\n';
          out_metrics << "\tTextfieldColorHighlight: 808080FF" << '\n';
          out_metrics << "\tIncompatibleVersionColor: 808080FF" << '\n';
          out_metrics << "\tIncompatibleGameColor: 606060FF" << '\n';
          out_metrics << "\tIncompatibleProtectedGameColor: 505050FF"
                      << '\n';
          out_metrics << "\tProtectedGameColor: FFFF00FF" << '\n';
          out_metrics << "\tWaitingGameColor: 00FF00FF" << '\n';
          out_metrics << "\tIncompatibleWaitingGameColor: 404040FF" << '\n';
          out_metrics << "\tGameStartedColor: FF0000FF" << '\n';
          out_metrics << "\tIncompatibleGameStartedColor: 303030FF"
                      << '\n';
        }
        static fs::FileSystem fs_metrics20;
        fs_metrics20.Mount(dir_metrics20.generic_string());
        static const std::vector<std::string> vec_files_metrics20{
            "metrics.yaml"};
        metrics_server_list20.Initialize(
            {&fs_metrics20, &vec_files_metrics20});
      }

      const auto find_row = [](mods::ServerListLogic& ref_logic,
                               std::string_view sv_address) {
        const auto& vec_rows = ref_logic.RowGamesForTest();
        for (std::size_t sz_i = 0; sz_i < vec_rows.size(); ++sz_i)
          if (vec_rows[sz_i]->Address == sv_address)
            return static_cast<int>(sz_i);
        return -1;
      };

      struct Stub20 {
        enum Kind {
          kContainer,
          kLabel,
          kLabelTooltip,
          kButton,
          kImage,
          kCheckbox,
          kDropDown,
          kMapPreview,
          kScrollPanel,
          kScrollItem,
          kLogicTicker
        };

        std::unique_ptr<ui::Widget> MakeNode(std::string_view str_id,
                                             Kind kind_node) {
          ui::WidgetArgs args_stub;
          std::unique_ptr<ui::Widget> up_node;
          switch (kind_node) {
            case kLabel:
              up_node = std::make_unique<ui::LabelWidget>(args_stub);
              break;
            case kLabelTooltip:
              up_node =
                  std::make_unique<ui::LabelWithTooltipWidget>(args_stub);
              break;
            case kButton:
              up_node = std::make_unique<ui::ButtonWidget>(args_stub);
              break;
            case kImage:
              up_node = std::make_unique<ui::ImageWidget>();
              break;
            case kCheckbox:
              up_node = std::make_unique<ui::CheckboxWidget>(args_stub);
              break;
            case kDropDown:
              up_node =
                  std::make_unique<ui::DropDownButtonWidget>(args_stub);
              break;
            case kMapPreview:
              up_node = std::make_unique<ui::MapPreviewWidget>();
              break;
            case kScrollPanel:
              up_node =
                  std::make_unique<ui::ScrollPanelWidget>(args_stub);
              break;
            case kScrollItem:
              up_node =
                  std::make_unique<ui::ScrollItemWidget>(args_stub);
              break;
            case kLogicTicker:
              up_node = std::make_unique<ui::LogicTickerWidget>();
              break;
            default:
              up_node = std::make_unique<ui::ContainerWidget>();
              break;
          }
          up_node->str_id = std::string{str_id};
          up_node->bounds_widget.int4_width = 200;
          up_node->bounds_widget.int4_height = 20;
          return up_node;
        }

        ui::Widget* Attach(ui::Widget& ref_parent, std::string_view str_id,
                           Kind kind_node) {
          std::unique_ptr<ui::Widget> up_node = MakeNode(str_id, kind_node);
          ui::Widget* ptr_node = up_node.get();
          ref_parent.AddChild(std::move(up_node));
          return ptr_node;
        }
      };

      auto up_stub = std::make_unique<Stub20>();
      Stub20* ptr_stub = up_stub.get();

      auto up_root = ptr_stub->MakeNode("ROOT20", Stub20::kContainer);
      ui::Widget* ptr_root = up_root.get();

      std::unique_ptr<ui::Widget> up_list = ptr_stub->MakeNode(
          "SERVER_LIST", Stub20::kScrollPanel);
      ui::Widget* ptr_list = up_list.get();
      ptr_root->AddChild(std::move(up_list));
      ui::Widget* ptr_header_tpl =
          ptr_stub->Attach(*ptr_list, "HEADER_TEMPLATE", Stub20::kScrollItem);
      ptr_stub->Attach(*ptr_header_tpl, "LABEL", Stub20::kLabel);
      ui::Widget* ptr_server_tpl =
          ptr_stub->Attach(*ptr_list, "SERVER_TEMPLATE", Stub20::kScrollItem);
      ptr_stub->Attach(*ptr_server_tpl, "TITLE", Stub20::kLabelTooltip);
      ptr_stub->Attach(*ptr_server_tpl, "PASSWORD_PROTECTED", Stub20::kImage);
      ptr_stub->Attach(*ptr_server_tpl, "REQUIRES_AUTHENTICATION",
                       Stub20::kImage);
      ptr_stub->Attach(*ptr_server_tpl, "PLAYERS", Stub20::kLabelTooltip);
      ptr_stub->Attach(*ptr_server_tpl, "LOCATION", Stub20::kLabel);
      ptr_stub->Attach(*ptr_server_tpl, "STATUS", Stub20::kLabel);

      ui::Widget* ptr_notice =
          ptr_stub->Attach(*ptr_root, "NOTICE_CONTAINER", Stub20::kContainer);
      ptr_stub->Attach(*ptr_notice, "OUTDATED_VERSION_LABEL", Stub20::kLabel);
      ptr_stub->Attach(*ptr_notice, "UNKNOWN_VERSION_LABEL", Stub20::kLabel);
      ptr_stub->Attach(*ptr_notice, "PLAYTEST_AVAILABLE_LABEL",
                       Stub20::kLabel);
      ptr_stub->Attach(*ptr_root, "NOTICE_WATCHER", Stub20::kLogicTicker);
      ptr_stub->Attach(*ptr_root, "JOIN_BUTTON", Stub20::kButton);
      ptr_stub->Attach(*ptr_root, "PROGRESS_LABEL", Stub20::kLabel);
      ptr_stub->Attach(*ptr_root, "FILTERS_DROPDOWNBUTTON", Stub20::kDropDown);
      ui::Widget* ptr_reload =
          ptr_stub->Attach(*ptr_root, "RELOAD_BUTTON", Stub20::kButton);
      ui::Widget* ptr_reload_icon =
          ptr_stub->Attach(*ptr_reload, "IMAGE_RELOAD", Stub20::kImage);
      ptr_stub->Attach(*ptr_reload_icon, "ANIMATION", Stub20::kLogicTicker);
      ptr_stub->Attach(*ptr_root, "PLAYER_COUNT", Stub20::kLabel);
      ptr_stub->Attach(*ptr_root, "SELECTED_MAP_PREVIEW",
                       Stub20::kMapPreview);
      ptr_stub->Attach(*ptr_root, "SELECTED_MAP", Stub20::kLabelTooltip);
      ptr_stub->Attach(*ptr_root, "SELECTED_IP", Stub20::kLabel);
      ptr_stub->Attach(*ptr_root, "SELECTED_STATUS", Stub20::kLabel);
      ptr_stub->Attach(*ptr_root, "SELECTED_MOD_VERSION", Stub20::kLabel);
      ptr_stub->Attach(*ptr_root, "SELECTED_PLAYERS", Stub20::kLabel);
      ptr_stub->Attach(*ptr_root, "CLIENT_LIST_CONTAINER", Stub20::kContainer);
      ptr_stub->Attach(*ptr_root, "DIRECTCONNECT_BUTTON", Stub20::kButton);
      ptr_stub->Attach(*ptr_root, "CREATE_BUTTON", Stub20::kButton);
      ptr_stub->Attach(*ptr_root, "BACK_BUTTON", Stub20::kButton);

      static const std::string str_master_yaml =
          "GameServer@0:\n"
          "\tProtocol: 2\n"
          "\tName: Waiting Game\n"
          "\tAddress: 10.0.0.1:1234\n"
          "\tPort: 1234\n"
          "\tState: 1\n"
          "\tMaxPlayers: 4\n"
          "\tMap: map-uid-a\n"
          "\tMod: ra\n"
          "\tVersion: 1.0-test\n"
          "\tModTitle: Red Alert\n"
          "\tProtected: True\n"
          "\tPlayers: 2\n"
          "\tBots: 1\n"
          "\tSpectators: 1\n"
          "\tLocation: Testland\n"
          "\tClients:\n"
          "\t\tClient@0:\n"
          "\t\t\tName: Alice\n"
          "\t\t\tColor: FFFF0000\n"
          "\t\t\tFaction: soviet\n"
          "\t\t\tTeam: 1\n"
          "\t\t\tSpawnPoint: 2\n"
          "\t\t\tIsSpectator: False\n"
          "\t\tClient@1:\n"
          "\t\t\tName: Bob\n"
          "\t\t\tColor: FF00FF00\n"
          "\t\t\tFaction: allied\n"
          "\t\t\tTeam: 2\n"
          "\t\t\tSpawnPoint: 3\n"
          "\t\tClient@2:\n"
          "\t\t\tName: Spectator Sam\n"
          "\t\t\tIsSpectator: True\n"
          "GameServer@1:\n"
          "\tAddress: 10.0.0.2:1235\n"
          "\tState: 2\n"
          "\tMap: map-uid-b\n"
          "\tMod: ra\n"
          "\tVersion: 1.0-test\n"
          "\tModTitle: Red Alert\n"
          "\tStarted: 2000-01-01T00:00:00\n"
          "\tPlayTime: 600\n"
          "\tLocation: Testland\n"
          "GameServer@2:\n"
          "\tAddress: 10.0.0.3:1236\n"
          "\tState: 1\n"
          "\tPlayers: 0\n"
          "\tMap: map-uid-b\n"
          "\tMod: ra\n"
          "\tVersion: 1.0-test\n"
          "\tModTitle: Red Alert\n"
          "GameServer@3:\n"
          "\tState: 1\n"
          "\tMap: map-uid-b\n"
          "\tMod: ra\n"
          "\tVersion: 1.0-test\n"
          "\tModTitle: Red Alert\n";

      auto deps_ui_20 = ui::Ui::Instance().DepsForTest();
      deps_ui_20.ptr_chrome_metrics = &metrics_server_list20;
      deps_ui_20.fn_run_after_tick =
          [](std::function<void()> fn_action) { fn_action(); };
      deps_ui_20.fn_allow_downloading = [] { return false; };
      deps_ui_20.fn_engine_version = [] { return std::string{"dev"}; };
      deps_ui_20.fn_session_guid = [] { return std::string{"guid-self"}; };
      int int4_filters_state = 15;
      deps_ui_20.fn_mp_game_filters_get =
          [&int4_filters_state] { return int4_filters_state; };
      deps_ui_20.fn_mp_game_filters_set = [&int4_filters_state](
                                              int int4_value) {
        int4_filters_state = int4_value;
      };
      net::ConnectionTarget target_join_record{"", 0};
      deps_ui_20.fn_join_server =
          [&target_join_record](const net::ConnectionTarget& target_endpoint,
                                const std::string&) {
        target_join_record = target_endpoint;
      };
      std::vector<std::string> vec_fetch_urls;
      deps_ui_20.fn_fetch_server_list =
          [&vec_fetch_urls](
              const std::string& str_url,
              std::function<void(bool, std::string)> fn_done) {
            vec_fetch_urls.push_back(str_url);
            fn_done(true, str_master_yaml);
          };
      deps_ui_20.fn_load_widget =
          [ptr_stub](ui::WidgetArgs&, ui::Widget* ptr_parent,
                     const std::string& str_id) -> ui::Widget* {
        if (str_id == "MULTIPLAYER_CLIENT_LIST") {
          std::unique_ptr<ui::Widget> up_panel = ptr_stub->MakeNode(
              "MULTIPLAYER_CLIENT_LIST", Stub20::kScrollPanel);
          ui::Widget* ptr_header =
              ptr_stub->Attach(*up_panel, "HEADER", Stub20::kScrollItem);
          ptr_stub->Attach(*ptr_header, "LABEL", Stub20::kLabel);
          ui::Widget* ptr_item =
              ptr_stub->Attach(*up_panel, "TEMPLATE", Stub20::kScrollItem);
          ptr_stub->Attach(*ptr_item, "LABEL", Stub20::kLabel);
          ptr_stub->Attach(*ptr_item, "NOFLAG_LABEL", Stub20::kLabel);
          ptr_stub->Attach(*ptr_item, "FLAG", Stub20::kImage);
          ui::Widget* ptr_panel = up_panel.get();
          ptr_parent->AddChild(std::move(up_panel));
          return ptr_panel;
        }
        std::unique_ptr<ui::Widget> up_panel =
            ptr_stub->MakeNode("LOADED20", Stub20::kContainer);
        if (str_id == "MULTIPLAYER_FILTER_PANEL") {
          ptr_stub->Attach(*up_panel, "WAITING_FOR_PLAYERS",
                           Stub20::kCheckbox);
          ptr_stub->Attach(*up_panel, "EMPTY", Stub20::kCheckbox);
          ptr_stub->Attach(*up_panel, "ALREADY_STARTED", Stub20::kCheckbox);
          ptr_stub->Attach(*up_panel, "PASSWORD_PROTECTED",
                           Stub20::kCheckbox);
          ptr_stub->Attach(*up_panel, "INCOMPATIBLE_VERSION",
                           Stub20::kCheckbox);
        }
        ui::Widget* ptr_panel = up_panel.get();
        ptr_parent->AddChild(std::move(up_panel));
        return ptr_panel;
      };
      ui::Ui::Instance().Initialize(std::move(deps_ui_20));

      ui::Widget* ptr_panel = ptr_root;

      bool b_on_start = false;
      bool b_on_exit = false;
      std::string str_joined_address;
      auto up_multiplayer = std::make_unique<mods::MultiplayerLogic>(
          *ptr_root, mod_data, [&] { b_on_start = true; },
          [&] { b_on_exit = true; }, nullptr);

      mods::ServerListLogic& ref_list = up_multiplayer->ServerListForTest();
      Check(ref_list.SearchStatusForTest() ==
                    mods::ServerListLogic::SearchStatus::Hidden,
            "p6b20: a successful fetch hides the progress label");
      CheckEq(ref_list.ProgressLabelText(), std::string{},
              "p6b20: the hidden status renders empty progress text");
      Check(!vec_fetch_urls.empty() &&
                vec_fetch_urls.front().find("games?protocol=2&engine=dev&") !=
                    std::string::npos,
            "p6b20: the query url carries protocol and engine");
      CheckEq(ref_list.PlayerCountForTest(), 2,
              "p6b20: the player count sums the listed games");

      const int int4_row_waiting =
          find_row(ref_list, "10.0.0.1:1234");
      const int int4_row_playing =
          find_row(ref_list, "10.0.0.2:1235");
      const int int4_row_empty =
          find_row(ref_list, "10.0.0.3:1236");
      Check(int4_row_waiting >= 0 && int4_row_playing >= 0 &&
                int4_row_empty >= 0,
            "p6b20: the waiting/playing/empty games all render rows");
      Check(find_row(ref_list, "10.0.0.4") == -1,
            "p6b20: no address surfaces for the null-address entry");
      Check(int4_row_empty > int4_row_waiting,
            "p6b20: an empty server sorts below a waiting one");
      Check(int4_row_playing > int4_row_waiting,
            "p6b20: a waiting game sorts above an active one");

      ref_list.SelectServerForTest(static_cast<std::size_t>(
          int4_row_waiting < 0 ? 0 : int4_row_waiting));
      Check(ptr_panel->Get<ui::LabelWidget>("SELECTED_IP")->fn_get_text() ==
                "10.0.0.1:1234",
            "p6b20: the selection panel shows the address");
      Check(ptr_panel->Get<ui::LabelWidget>("SELECTED_STATUS")
                    ->fn_get_text() == "label-password-protected",
            "p6b20: a protected waiting game labels its state");
      Check(ptr_panel->Get<ui::LabelWidget>("SELECTED_PLAYERS")
                    ->fn_get_text()
                    .find("label-players-count") != std::string::npos,
            "p6b20: the selected-players line counts players");

      ref_list.SelectServerForTest(static_cast<std::size_t>(
          int4_row_playing < 0 ? 0 : int4_row_playing));
      Check(ptr_panel->Get<ui::LabelWidget>("SELECTED_STATUS")
                    ->fn_get_text()
                    .find("label-in-progress-for") != std::string::npos,
            "p6b20: a playing game labels the in-progress minutes");
      Check(ptr_panel->Get<ui::ButtonWidget>("JOIN_BUTTON")
                    ->fn_is_disabled(),
            "p6b20: an incompatible-map game disables joining");

      ref_list.SelectServerForTest(static_cast<std::size_t>(
          int4_row_waiting < 0 ? 0 : int4_row_waiting));
      Check(!ptr_panel->Get<ui::ButtonWidget>("JOIN_BUTTON")
                   ->fn_is_disabled(),
            "p6b20: a joinable waiting game enables joining");
      ref_list.JoinClickedForTest();
      CheckEq(target_join_record.ToString(), std::string{"10.0.0.1:1234"},
              "p6b20: the join click connects to the selected address");

      int4_filters_state = 15 & ~8;
      ref_list.RefreshServerList();
      Check(find_row(ref_list, "10.0.0.2:1235") == -1,
            "p6b20: clearing the started filter hides the active game");
      int4_filters_state = 15;
      ref_list.ToggleFilterFlagForTest(2);
      Check((int4_filters_state & 2) == 0,
            "p6b20: the toggle flips the filter bit");
      ref_list.RefreshServerList();
      Check(find_row(ref_list, "10.0.0.3:1236") == -1,
            "p6b20: clearing the empty filter hides the empty game");
      int4_filters_state = 15;

      Check(!up_multiplayer->CreateServerDisabledForTest(),
            "p6b20: lobby maps keep the create-server button enabled");
      up_multiplayer->BackClickedForTest();
      Check(b_on_exit, "p6b20: the back button propagates onExit");

      up_multiplayer.reset();

      deps_server_game.fn_external_mod_compatible =
          deps_server_game_saved.fn_external_mod_compatible;
      deps_server_game.fn_external_mod_id =
          deps_server_game_saved.fn_external_mod_id;
      deps_server_game.fn_mod_title = deps_server_game_saved.fn_mod_title;
      deps_server_game.fn_allow_downloading =
          deps_server_game_saved.fn_allow_downloading;
      deps_server_game.fn_map_available =
          deps_server_game_saved.fn_map_available;
    }
  }

  // ———— Phase 6 第二十一批:ServerCreation + GameInfo 面板族 + AssetBrowser ————
  // ———— Phase 6 batch 21: server creation + the game-info panels + the
  // asset browser ————
  {
    std::println("-- batch 21: server creation + game-info + asset browser --");

    {
      net::Nat::Instance().ResetForTest();
      Check(net::Nat::Instance().Status() == net::NatStatus::Disabled,
            "p6b21: an uninitialized nat reads disabled");
      net::Nat::Instance().Initialize(false);
      Check(net::Nat::Instance().Status() == net::NatStatus::Disabled,
            "p6b21: discovery stays off without the setting");
      net::Nat::Instance().ResetForTest();
      net::Nat::Instance().Initialize(true);
      Check(net::Nat::Instance().Status() == net::NatStatus::NotSupported,
            "p6b21: searching without a device reads not-supported");
      net::Nat::Instance().SetDeviceFoundForTest(true);
      Check(net::Nat::Instance().Status() == net::NatStatus::Enabled,
            "p6b21: a found device reads enabled");
      Check(!net::Nat::Instance().TryForwardPort(1234, 1234) &&
                !net::Nat::Instance().TryRemovePortForward(),
            "p6b21: the port-map pair fails without Mono.Nat");
      net::Nat::Instance().ResetForTest();
    }

    struct PanelStub21 {
      std::vector<std::unique_ptr<ui::Widget>> vec_owned;
      std::vector<std::string> vec_loaded_ids;
      gfx::SheetBuilder builder_sheets{gfx::SheetType::BGRA, 128};
      std::unique_ptr<gfx::SpriteFont> up_font;

      void MakeFont() {
        struct FakeFont21 final : platform::IFont {
          platform::FontGlyph CreateGlyph(std::uint32_t, int,
                                          float) override {
            platform::FontGlyph glyph_ret;
            glyph_ret.offset = int2{1, 2};
            glyph_ret.int4_width = 2;
            glyph_ret.int4_height = 3;
            glyph_ret.fp4_advance = 4.0f;
            glyph_ret.vec_data.assign(2 * 3, 0);
            return glyph_ret;
          }
        };
        up_font = std::make_unique<gfx::SpriteFont>(
            "Regular", std::span<const std::uint8_t>{},
            std::make_unique<FakeFont21>(), 10, 8, 1.0f, builder_sheets,
            [](const gfx::Sprite&, core::Vector3, float, core::Vector3,
               float) {},
            [](const gfx::Sprite&, core::Vector3, core::Vector3,
               core::Vector3, core::Vector3, core::Vector3, float) {});
      }

      enum Kind {
        kContainer,
        kLabel,
        kLabelTooltip,
        kButton,
        kImage,
        kCheckbox,
        kSlider,
        kDropDown,
        kMapPreview,
        kScrollPanel,
        kScrollItem,
        kTextField,
        kPasswordField,
        kSprite,
        kColorBlock,
        kVideoPlayer,
        kLogicTicker
      };

      std::unique_ptr<ui::Widget> MakeNode(std::string_view str_id,
                                           Kind kind_node) {
        ui::WidgetArgs args_stub;
        std::unique_ptr<ui::Widget> up_node;
        switch (kind_node) {
          case kLabel:
            up_node = std::make_unique<ui::LabelWidget>(args_stub);
            break;
          case kLabelTooltip:
            up_node =
                std::make_unique<ui::LabelWithTooltipWidget>(args_stub);
            break;
          case kButton:
            up_node = std::make_unique<ui::ButtonWidget>(args_stub);
            break;
          case kImage:
            up_node = std::make_unique<ui::ImageWidget>();
            break;
          case kCheckbox:
            up_node = std::make_unique<ui::CheckboxWidget>(args_stub);
            break;
          case kSlider:
            up_node = std::make_unique<ui::SliderWidget>();
            break;
          case kDropDown:
            up_node =
                std::make_unique<ui::DropDownButtonWidget>(args_stub);
            break;
          case kMapPreview:
            up_node = std::make_unique<ui::MapPreviewWidget>();
            break;
          case kScrollPanel:
            up_node = std::make_unique<ui::ScrollPanelWidget>(args_stub);
            break;
          case kScrollItem:
            up_node = std::make_unique<ui::ScrollItemWidget>(args_stub);
            break;
          case kTextField:
            up_node = std::make_unique<ui::TextFieldWidget>(args_stub);
            break;
          case kPasswordField:
            up_node = std::make_unique<ui::PasswordFieldWidget>(args_stub);
            break;
          case kSprite:
            up_node = std::make_unique<ui::SpriteWidget>(args_stub);
            break;
          case kColorBlock:
            up_node = std::make_unique<ui::ColorBlockWidget>(args_stub);
            break;
          case kVideoPlayer:
            up_node =
                std::make_unique<mods::VideoPlayerWidget>(args_stub);
            break;
          case kLogicTicker:
            up_node = std::make_unique<ui::LogicTickerWidget>();
            break;
          default:
            up_node = std::make_unique<ui::ContainerWidget>();
            break;
        }
        up_node->str_id = std::string{str_id};
        up_node->bounds_widget.int4_width = 200;
        up_node->bounds_widget.int4_height = 20;
        return up_node;
      }

      ui::Widget* Attach(ui::Widget& ref_parent, std::string_view str_id,
                         Kind kind_node) {
        std::unique_ptr<ui::Widget> up_node = MakeNode(str_id, kind_node);
        ui::Widget* ptr_node = up_node.get();
        ref_parent.AddChild(std::move(up_node));
        return ptr_node;
      }

      ui::Widget* MakeRoot() {
        auto up_root = std::make_unique<ui::ContainerWidget>();
        up_root->str_id = "ROOT21";
        ui::Widget* ptr_root = up_root.get();
        vec_owned.push_back(std::move(up_root));
        return ptr_root;
      }

      ui::Widget* AttachNew(ui::Widget& ref_parent, std::string_view str_id,
                            Kind kind_node) {
        std::unique_ptr<ui::Widget> up_node = MakeNode(str_id, kind_node);
        ui::Widget* ptr_node = up_node.get();
        ref_parent.AddChild(std::move(up_node));
        return ptr_node;
      }

      ui::Widget* MakePromptTree(const std::string& str_id) {
        auto up_root = std::make_unique<ui::ContainerWidget>();
        up_root->str_id = str_id;
        ui::Widget* ptr_root = up_root.get();
        AttachNew(*ptr_root, "PROMPT_TITLE", kLabel);
        AttachNew(*ptr_root, "PROMPT_TEXT", kLabel);
        AttachNew(*ptr_root, "CONFIRM_BUTTON", kButton);
        AttachNew(*ptr_root, "CANCEL_BUTTON", kButton);
        AttachNew(*ptr_root, "OTHER_BUTTON", kButton);
        ui::Widget* ptr_released = up_root.release();
        return ptr_released;
      }

      ui::Widget* MakeLoadedTree(const std::string& str_id) {
        if (str_id == "TWOBUTTON_PROMPT" || str_id == "THREEBUTTON_PROMPT")
          return MakePromptTree(str_id);
        auto up_root = std::make_unique<ui::ContainerWidget>();
        up_root->str_id = str_id;
        ui::Widget* ptr_root = up_root.get();
        if (str_id == "LABEL_DROPDOWN_TEMPLATE") {
          ui::Widget* ptr_panel =
              AttachNew(*ptr_root, "PANEL21", kScrollPanel);
          ui::Widget* ptr_item =
              AttachNew(*ptr_panel, "TEMPLATE", kScrollItem);
          AttachNew(*ptr_item, "LABEL", kLabel);
        } else if (str_id == "ASSET_TYPES_PANEL") {
          AttachNew(*ptr_root, "ASSET_TYPE_TEMPLATE", kCheckbox);
        }
        return up_root.release();
      }
    };

    auto up_stub = std::make_unique<PanelStub21>();
    up_stub->MakeFont();
    PanelStub21* ptr_stub = up_stub.get();
    static std::unique_ptr<PanelStub21> up_stub_holder21;
    up_stub_holder21 = std::move(up_stub);

    static const auto ptr_random_21 =
        std::make_shared<MersenneTwister>(20261022);

    static ui::ChromeMetrics metrics_panel21;
    {
      const auto dir_metrics21 =
          std::filesystem::temp_directory_path() /
          "ora_render_test_metrics_panel21";
      std::filesystem::create_directories(dir_metrics21);
      {
        std::ofstream out_metrics(dir_metrics21 / "metrics.yaml");
        out_metrics << "Chrome:" << '\n';
        out_metrics << "	TextFont: Regular" << '\n';
        out_metrics << "	TextColor: FFFFFFFF" << '\n';
        out_metrics << "	TextContrast: True" << '\n';
        out_metrics << "	TextShadow: False" << '\n';
        out_metrics << "	TextContrastColorDark: 000000FF" << '\n';
        out_metrics << "	TextContrastColorLight: FFFFFFFF" << '\n';
        out_metrics << "	TextContrastRadius: 1" << '\n';
        out_metrics << "	ButtonDepth: 1" << '\n';
        out_metrics << "	ButtonFont: Regular" << '\n';
        out_metrics << "	ButtonTextColor: FFFFFFFF" << '\n';
        out_metrics << "	ButtonTextColorDisabled: 808080FF" << '\n';
        out_metrics << "	ButtonTextContrast: True" << '\n';
        out_metrics << "	ButtonTextShadow: False" << '\n';
        out_metrics << "	ButtonTextContrastColorDark: 000000FF" << '\n';
        out_metrics << "	ButtonTextContrastColorLight: FFFFFFFF" << '\n';
        out_metrics << "	ButtonTextContrastRadius: 1" << '\n';
        out_metrics << "	ButtonCursor: default" << '\n';
        out_metrics << "	ClickSound: click_sound" << '\n';
        out_metrics << "	ClickDisabledSound: click_disabled_sound" << '\n';
        out_metrics << "	TextDisabledColor: 808080FF" << '\n';
        out_metrics << "\tTextfieldFont: Regular" << '\n';
        out_metrics << "\tTextfieldColor: FFFFFFFF" << '\n';
        out_metrics << "\tTextfieldColorDisabled: 808080FF" << '\n';
        out_metrics << "\tTextfieldColorInvalid: FFFF00FF" << '\n';
        out_metrics << "\tTextfieldColorHighlight: 808080FF" << '\n';
        out_metrics << "\tNoticeSuccessColor: 00FF00FF" << '\n';
        out_metrics << "\tNoticeErrorColor: FF0000FF" << '\n';
        out_metrics << "\tNoticeInfoColor: FFFFFFFF" << '\n';
      }
      static fs::FileSystem fs_metrics21;
      fs_metrics21.Mount(dir_metrics21.generic_string());
      static const std::vector<std::string> vec_files_metrics21{
          "metrics.yaml"};
      metrics_panel21.Initialize({&fs_metrics21, &vec_files_metrics21});
    }

    ui::Ui::Deps deps_ui_21;
    deps_ui_21.ptr_chrome_metrics = &metrics_panel21;
    deps_ui_21.fn_window_size = [] { return int2{1280, 720}; };
    deps_ui_21.fn_try_get_font =
        [ptr_font = up_stub_holder21->up_font.get()](const std::string&) {
          return ptr_font;
        };
    deps_ui_21.fn_load_widget =
        [ptr_stub](ui::WidgetArgs&, ui::Widget* ptr_parent,
                   const std::string& str_id) -> ui::Widget* {
      ptr_stub->vec_loaded_ids.push_back(str_id);
      ui::Widget* ptr_panel = ptr_stub->MakeLoadedTree(str_id);
      if (ptr_parent != nullptr)
        ptr_parent->AddChild(std::unique_ptr<ui::Widget>(ptr_panel));
      return ptr_panel;
    };
    deps_ui_21.ptr_map_cache = &cache;
    deps_ui_21.fn_cosmetic_random = [] {
      return ptr_random_21.get();
    };
    deps_ui_21.fn_player_name = [] { return std::string{"Commander"}; };
    static ui::ServerSettingsFace face_server_state21;
    deps_ui_21.fn_server_settings_get = [] {
      return face_server_state21;
    };
    deps_ui_21.fn_server_settings_set = [](
        const ui::ServerSettingsFace& face_next) {
      face_server_state21 = face_next;
    };
    static int int4_create_result21 = 1;  // 1 = 成功 | success
    static int int4_create_error_code21 = 0;
    static std::string str_create_error_message21;
    static net::ConnectionTarget target_created21{"127.0.0.1", 4242};
    deps_ui_21.fn_create_server =
        [](const ui::ServerSettingsFace&, int& int4_code,
           std::string& str_message, net::ConnectionTarget& target_out) {
          int4_code = int4_create_error_code21;
          str_message = str_create_error_message21;
          if (int4_create_result21 == 1) {
            target_out = target_created21;
            return true;
          }
          return false;
        };
    static int int4_settings_save_count21 = 0;
    deps_ui_21.fn_settings_save = [] { ++int4_settings_save_count21; };
    static net::ConnectionTarget target_connected21{};
    static bool b_connect_called21 = false;
    deps_ui_21.fn_join_server =
        [](const net::ConnectionTarget& target_next, const std::string&) {
          target_connected21 = target_next;
          b_connect_called21 = true;
        };
    static bool b_is_host21 = false;
    deps_ui_21.fn_is_host = [] { return b_is_host21; };
    ui::Ui::Instance().Initialize(std::move(deps_ui_21));

    // ———— AssetBrowserData:ra mod.yaml 的 "AssetBrowser" 节点 ————
    // ———— AssetBrowserData: ra mod.yaml's "AssetBrowser" node ————
    {
      const mods::AssetBrowserData data_browser =
          mods::AssetBrowserData::FromManifest(mod_data);
      CheckEq(data_browser.vec_sprite_extensions.size(), std::size_t{6},
              "p6b21: ra lists six sprite extensions");
      CheckEq(data_browser.vec_audio_extensions.size(), std::size_t{10},
              "p6b21: ra lists eight audio extensions");
      CheckEq(data_browser.vec_video_extensions.size(), std::size_t{2},
              "p6b21: ra lists two video extensions");
      Check(data_browser.vec_model_extensions.empty(),
            "p6b21: ra declares no model extensions");
      Check(std::find(data_browser.vec_sprite_extensions.begin(),
                      data_browser.vec_sprite_extensions.end(),
                      ".shp") !=
                data_browser.vec_sprite_extensions.end() &&
                std::find(data_browser.vec_audio_extensions.begin(),
                          data_browser.vec_audio_extensions.end(),
                          ".aud") !=
                    data_browser.vec_audio_extensions.end(),
            "p6b21: the extension tables lowercase and parse");
    }

    // ———— AssetBrowserLogic:真 ra 资产装载 ————
    // ———— AssetBrowserLogic: loading real ra assets ————
    {
      ui::Widget* ptr_root_browser = ptr_stub->MakeRoot();
      ui::Widget* ptr_list =
          ptr_stub->AttachNew(*ptr_root_browser, "ASSET_LIST",
                              PanelStub21::kScrollPanel);
      for (const char* str_key :
           {"ButtonDepth", "ButtonFont", "ButtonTextColor",
            "ButtonTextColorDisabled", "ButtonTextContrast",
            "ButtonTextShadow", "ButtonTextContrastColorDark",
            "ButtonTextContrastColorLight", "ButtonTextContrastRadius",
            "ClickSound", "ClickDisabledSound", "ButtonCursor"}) {
        try {
          metrics_panel21.Get(str_key);
        } catch (const std::exception&) {
        }
      }
      ui::Widget* ptr_item_tpl =
          ptr_stub->AttachNew(*ptr_list, "ASSET_TEMPLATE",
                              PanelStub21::kScrollItem);
      ptr_stub->AttachNew(*ptr_item_tpl, "TITLE",
                          PanelStub21::kLabelTooltip);
      ptr_stub->AttachNew(*ptr_root_browser, "FILENAME_INPUT",
                          PanelStub21::kTextField);
      ptr_stub->AttachNew(*ptr_root_browser, "ANIMATION_TICKER",
                          PanelStub21::kLogicTicker);
      ptr_stub->AttachNew(*ptr_root_browser, "SOURCE_SELECTOR",
                          PanelStub21::kDropDown);
      ptr_stub->AttachNew(*ptr_root_browser, "ASSET_TYPES_DROPDOWN",
                          PanelStub21::kDropDown);
      ptr_stub->AttachNew(*ptr_root_browser, "SPRITE", PanelStub21::kSprite);
      ptr_stub->AttachNew(*ptr_root_browser, "PLAYER",
                          PanelStub21::kVideoPlayer);
      ptr_stub->AttachNew(*ptr_root_browser, "ERROR", PanelStub21::kLabel);
      ptr_stub->AttachNew(*ptr_root_browser, "PALETTE_SELECTOR",
                          PanelStub21::kDropDown);
      ptr_stub->AttachNew(*ptr_root_browser, "PALETTE_DESC",
                          PanelStub21::kLabel);
      ptr_stub->AttachNew(*ptr_root_browser, "COLOR",
                          PanelStub21::kDropDown);
      ptr_stub->AttachNew(*ptr_root_browser, "COLORBLOCK",
                          PanelStub21::kColorBlock);
      ptr_stub->AttachNew(*ptr_root_browser, "FRAME_SELECTOR",
                          PanelStub21::kContainer);
      ptr_stub->AttachNew(*ptr_root_browser, "FRAME_SLIDER",
                          PanelStub21::kSlider);
      ptr_stub->AttachNew(*ptr_root_browser, "FRAME_COUNT",
                          PanelStub21::kLabel);
      ptr_stub->AttachNew(*ptr_root_browser, "BUTTON_PLAY",
                          PanelStub21::kButton);
      ptr_stub->AttachNew(*ptr_root_browser, "BUTTON_PAUSE",
                          PanelStub21::kButton);
      ptr_stub->AttachNew(*ptr_root_browser, "BUTTON_STOP",
                          PanelStub21::kButton);
      ptr_stub->AttachNew(*ptr_root_browser, "BUTTON_NEXT",
                          PanelStub21::kButton);
      ptr_stub->AttachNew(*ptr_root_browser, "BUTTON_PREV",
                          PanelStub21::kButton);
      ptr_stub->AttachNew(*ptr_root_browser, "SPRITE_SCALE_SLIDER",
                          PanelStub21::kSlider);
      ptr_stub->AttachNew(*ptr_root_browser, "SPRITE_SCALE",
                          PanelStub21::kLabel);
      ptr_stub->AttachNew(*ptr_root_browser, "VOXEL_SELECTOR",
                          PanelStub21::kContainer);
      ptr_stub->AttachNew(*ptr_root_browser, "ROLL_SLIDER",
                          PanelStub21::kSlider);
      ptr_stub->AttachNew(*ptr_root_browser, "PITCH_SLIDER",
                          PanelStub21::kSlider);
      ptr_stub->AttachNew(*ptr_root_browser, "YAW_SLIDER",
                          PanelStub21::kSlider);
      ptr_stub->AttachNew(*ptr_root_browser, "MODEL_SCALE_SLIDER",
                          PanelStub21::kSlider);
      ptr_stub->AttachNew(*ptr_root_browser, "MODEL_SCALE",
                          PanelStub21::kLabel);
      ptr_stub->AttachNew(*ptr_root_browser, "CLOSE_BUTTON",
                          PanelStub21::kButton);

      auto logic_browser = std::make_unique<mods::AssetBrowserLogic>(
          *ptr_root_browser, [] {}, mod_data, nullptr);

      Check(!logic_browser->AssetFilePathsForTest().empty(),
            "p6b21: the asset list enumerates mounted ra packages");

      const std::string str_first_path =
          logic_browser->AssetFilePathsForTest().empty()
              ? std::string{}
              : logic_browser->AssetFilePathsForTest().front();
      Check(logic_browser->FilterAssetForTest(str_first_path),
            "p6b21: the empty filter accepts every asset");
      ptr_root_browser->Get<ui::TextFieldWidget>("FILENAME_INPUT")
          ->SetText("zz-no-such-name");
      Check(!logic_browser->FilterAssetForTest(str_first_path),
            "p6b21: a miss filter rejects");
      if (!str_first_path.empty()) {
        const std::string str_token = str_first_path.substr(0, 1);
        ptr_root_browser->Get<ui::TextFieldWidget>("FILENAME_INPUT")
            ->SetText(str_token);
        Check(logic_browser->FilterAssetForTest(str_first_path),
              "p6b21: a substring filter accepts");
      }
      ptr_root_browser->Get<ui::TextFieldWidget>("FILENAME_INPUT")
          ->SetText("");

      const std::string* ptr_shp_path = nullptr;
      const std::string* ptr_aud_path = nullptr;
      for (const std::string& str_path :
           logic_browser->AssetFilePathsForTest()) {
        const std::size_t sz_dot = str_path.rfind('.');
        if (sz_dot == std::string::npos)
          continue;
        const std::string str_ext = str_path.substr(sz_dot);
        if (ptr_shp_path == nullptr && str_ext == ".shp")
          ptr_shp_path = &str_path;
        if (ptr_aud_path == nullptr && str_ext == ".aud")
          ptr_aud_path = &str_path;
      }
      Check(ptr_shp_path != nullptr,
            "p6b21: the ra packages expose a shp asset");

      if (ptr_shp_path != nullptr) {
        fs::IReadOnlyPackage* ptr_package = nullptr;
        std::string str_actual;
        Check(mod_data.ModFiles().TryGetPackageContaining(
                  *ptr_shp_path, ptr_package, str_actual),
              "p6b21: the shp resolves to its package");
        if (ptr_package != nullptr) {
          Check(logic_browser->LoadAssetForTest(ptr_package, str_actual),
                "p6b21: the shp loads");
          Check(logic_browser->CurrentSpriteCountForTest() > 0 &&
                    !logic_browser->IsLoadErrorForTest(),
                "p6b21: the shp decodes into sheet sprites");
          CheckEq(logic_browser->CurrentFilenameForTest(), str_actual,
                  "p6b21: the current filename tracks the load");
          logic_browser->NextFrameForTest();
          CheckEq(logic_browser->CurrentFrameForTest(), 1,
                  "p6b21: the frame stepper advances");
          ptr_root_browser->Get<ui::LabelWidget>("FRAME_COUNT")
              ->fn_get_text();
        }
      }

      if (ptr_aud_path != nullptr) {
        fs::IReadOnlyPackage* ptr_package = nullptr;
        std::string str_actual;
        if (mod_data.ModFiles().TryGetPackageContaining(*ptr_aud_path,
                                                       ptr_package,
                                                       str_actual) &&
            ptr_package != nullptr) {
          const bool b_loaded =
              logic_browser->LoadAssetForTest(ptr_package, str_actual);
          Check(b_loaded && !logic_browser->IsLoadErrorForTest(),
                "p6b21: the aud parses without an engine");
          ptr_root_browser->Get<ui::Widget>("FRAME_SELECTOR")
              ->fn_is_visible();
          ptr_root_browser->Get<ui::ButtonWidget>("BUTTON_PLAY")
              ->fn_on_click();
        }
      }

      logic_browser->SetAnimateFramesForTest(true);
      Check(ptr_root_browser->Get<ui::LogicTickerWidget>("ANIMATION_TICKER")
                    ->fn_on_tick != nullptr,
            "p6b21: the animation ticker wires the tick");
      logic_browser.reset();
    }

    // ———— ServerCreationLogic:公告三标签 + CreateAndJoin 双路 ————
    // ———— ServerCreationLogic: the notice trio + CreateAndJoin's two
    // paths ————
    {
      for (const char* str_key :
           {"TextfieldFont", "TextfieldColor", "TextfieldColorDisabled",
            "TextfieldColorInvalid", "TextfieldColorHighlight",
            "TextFont", "TextContrast", "TextShadow",
            "TextContrastColorDark", "TextContrastColorLight",
            "TextContrastRadius"}) {
        try {
          metrics_panel21.Get(str_key);
        } catch (const std::exception&) {
        }
      }
      ui::Widget* ptr_root_server = ptr_stub->MakeRoot();
      ptr_stub->AttachNew(*ptr_root_server, "MAP_PREVIEW_ROOT",
                          PanelStub21::kContainer);
      ptr_stub->AttachNew(*ptr_root_server, "BACK_BUTTON",
                          PanelStub21::kButton);
      ptr_stub->AttachNew(*ptr_root_server, "CREATE_BUTTON",
                          PanelStub21::kButton);
      ptr_stub->AttachNew(*ptr_root_server, "MAP_BUTTON",
                          PanelStub21::kButton);
      ptr_stub->AttachNew(*ptr_root_server, "SERVER_NAME",
                          PanelStub21::kTextField);
      ptr_stub->AttachNew(*ptr_root_server, "LISTEN_PORT",
                          PanelStub21::kTextField);
      ptr_stub->AttachNew(*ptr_root_server, "ADVERTISE_CHECKBOX",
                          PanelStub21::kCheckbox);
      ptr_stub->AttachNew(*ptr_root_server, "PASSWORD",
                          PanelStub21::kPasswordField);
      ptr_stub->AttachNew(*ptr_root_server, "NOTICES_HEADER_A",
                          PanelStub21::kLabel);
      ptr_stub->AttachNew(*ptr_root_server, "NOTICES_HEADER_B",
                          PanelStub21::kLabel);
      ptr_stub->AttachNew(*ptr_root_server, "NOTICES_HEADER_C",
                          PanelStub21::kLabel);
      ui::Widget* ptr_no_upnp =
          ptr_stub->AttachNew(*ptr_root_server, "NOTICES_NO_UPNP",
                              PanelStub21::kContainer);
      ptr_stub->AttachNew(*ptr_no_upnp, "SETTINGS_A",
                          PanelStub21::kContainer);
      ptr_stub->AttachNew(*ptr_no_upnp, "SETTINGS_B",
                          PanelStub21::kContainer);
      ptr_stub->AttachNew(*ptr_root_server, "NOTICES_UPNP",
                          PanelStub21::kContainer);
      ptr_stub->AttachNew(*ptr_root_server, "NOTICES_LAN",
                          PanelStub21::kContainer);

      face_server_state21 = ui::ServerSettingsFace{};
      int4_create_result21 = 1;
      int4_create_error_code21 = 0;
      str_create_error_message21.clear();
      int4_settings_save_count21 = 0;
      b_connect_called21 = false;

      bool b_opened_lobby21 = false;
      auto logic_server = std::make_unique<mods::ServerCreationLogic>(
          *ptr_root_server, mod_data, [] {},
          [&b_opened_lobby21] { b_opened_lobby21 = true; });

      CheckEq(logic_server->ServerNameForTest(),
              std::string{"Commander's Game"},
              "p6b21: an empty server name sanitizes to the player "
              "fallback");
      Check(logic_server->SelectedMapForTest() != nullptr,
            "p6b21: an initial lobby map is chosen");
      Check(logic_server->AdvertiseOnlineForTest(),
            "p6b21: advertise defaults on");
      CheckEq(logic_server->NoticesLabelBForTest(),
              std::string{"label-internet-server-nat-B-disabled"},
              "p6b21: the disabled nat labels notice B");
      Check(logic_server->NoticesLabelBVisibleForTest(),
            "p6b21: notice B shows while advertising");

      logic_server->AdvertiseClickedForTest();
      CheckEq(logic_server->NoticesLabelAForTest(),
              std::string{"label-local-server"},
              "p6b21: toggling off advertises the local notice");
      Check(!logic_server->NoticesLabelBVisibleForTest(),
            "p6b21: the lan mode hides notices B and C");
      logic_server->AdvertiseClickedForTest();

      ptr_root_server->Get<ui::PasswordFieldWidget>("PASSWORD")
          ->SetText("pw");
      ptr_root_server->Get<ui::TextFieldWidget>("LISTEN_PORT")
          ->SetText("not-a-port");
      logic_server->CreateClickedForTest();
      Check(b_connect_called21 &&
                target_connected21.ToString() == "127.0.0.1:4242",
            "p6b21: a successful create connects to the local endpoint");
      CheckEq(face_server_state21.ListenPort, 1234,
              "p6b21: a bad port text falls back to 1234");
      CheckEq(face_server_state21.Name, std::string{"Commander's Game"},
              "p6b21: the sanitized name persists");
      CheckEq(face_server_state21.Password, std::string{"pw"},
              "p6b21: the password persists");
      Check(!face_server_state21.Map.empty(),
            "p6b21: the chosen map uid persists");
      CheckEq(int4_settings_save_count21, 1,
              "p6b21: the creation path saves settings once");
      logic_server.reset();

      int4_create_result21 = 0;
      int4_create_error_code21 = 10048;
      str_create_error_message21 = "address in use";
      auto logic_server_fail = std::make_unique<mods::ServerCreationLogic>(
          *ptr_root_server, mod_data, [] {}, [] {});
      ptr_root_server->Get<ui::TextFieldWidget>("LISTEN_PORT")
          ->SetText("1234");
      logic_server_fail->CreateClickedForTest();
      Check(ptr_stub->vec_loaded_ids.back() == "TWOBUTTON_PROMPT",
            "p6b21: a port-used failure opens the two-button prompt");
      logic_server_fail.reset();
      int4_create_result21 = 1;
    }

    // ———— GameInfoLogic:ra 世界的可见 tab 装配 + AutoSelect ————
    // ———— GameInfoLogic: the visible-tab assembly over the ra world +
    // AutoSelect ————
    {
      ui::Widget* ptr_root_info = ptr_stub->MakeRoot();
      ui::Widget* ptr_tabs_2 = ptr_stub->AttachNew(
          *ptr_root_info, "TAB_CONTAINER_2", PanelStub21::kContainer);
      ui::Widget* ptr_tab_button_1 =
          ptr_stub->AttachNew(*ptr_tabs_2, "BUTTON1", PanelStub21::kButton);
      ptr_stub->AttachNew(*ptr_tabs_2, "BUTTON2", PanelStub21::kButton);
      for (const char* str_panel :
           {"OBJECTIVES_PANEL", "MAP_PANEL", "LOBBY_OPTIONS_PANEL",
            "DEBUG_PANEL", "CHAT_PANEL"})
        ptr_stub->AttachNew(*ptr_root_info, str_panel,
                            PanelStub21::kContainer);
      ui::Widget* ptr_chat_panel =
          ptr_root_info->Get("CHAT_PANEL");
      ptr_stub->AttachNew(*ptr_chat_panel, "CHAT_TEXTFIELD",
                          PanelStub21::kTextField);
      ptr_stub->AttachNew(*ptr_root_info, "TITLE", PanelStub21::kLabel);

      ptr_stub->vec_loaded_ids.clear();
      auto logic_info = std::make_unique<mods::GameInfoLogic>(
          *ptr_root_info, mod_data, *world,
          mods::IngameInfoPanel::AutoSelect,
          [](bool) {}, [] {});

      CheckEq(logic_info->VisiblePanelsForTest().size(), std::size_t{2},
              "p6b21: the skirmish world shows two info tabs");
      if (logic_info->VisiblePanelsForTest().size() == 2) {
        Check(logic_info->VisiblePanelsForTest()[0] ==
                      mods::IngameInfoPanel::Objectives &&
                  logic_info->VisiblePanelsForTest()[1] ==
                      mods::IngameInfoPanel::LobbyOptions,
              "p6b21: objectives + lobby options order");
      }
      Check(logic_info->ActivePanelForTest() ==
                mods::IngameInfoPanel::Objectives,
            "p6b21: AutoSelect lands on the first tab");
      Check(std::find(ptr_stub->vec_loaded_ids.begin(),
                      ptr_stub->vec_loaded_ids.end(),
                      "SKIRMISH_STATS") != ptr_stub->vec_loaded_ids.end(),
            "p6b21: the objectives panel loads the ra PanelName");
      Check(std::find(ptr_stub->vec_loaded_ids.begin(),
                      ptr_stub->vec_loaded_ids.end(),
                      "LOBBY_OPTIONS_PANEL") != ptr_stub->vec_loaded_ids.end(),
            "p6b21: the lobby options panel loads");

      ptr_tab_button_1->Get<ui::ButtonWidget>("BUTTON1")->fn_on_click();
      ptr_tab_button_1 = ptr_root_info->Get("TAB_CONTAINER_2")->Get(
          "BUTTON1");
      Check(logic_info->ActivePanelForTest() ==
                mods::IngameInfoPanel::Objectives,
            "p6b21: the first tab activates objectives");
      const std::string str_title_expected =
          world->MapPtr()->Categories().empty()
              ? world->MapPtr()->Title()
              : world->MapPtr()->Categories().front() + ": " +
                    world->MapPtr()->Title();
      CheckEq(logic_info->MapTitleForTest(), str_title_expected,
              "p6b21: the title takes the category prefix");
      logic_info.reset();
    }

    // ———— GameInfoObjectivesLogic:模板行重建 + 状态色 ————
    // ———— GameInfoObjectivesLogic: the template-row rebuild + the
    // status color ————
    {
      ui::Widget* ptr_root_obj = ptr_stub->MakeRoot();
      ui::Widget* ptr_panel_obj =
          ptr_stub->AttachNew(*ptr_root_obj, "OBJECTIVES_PANEL",
                              PanelStub21::kScrollPanel);
      ui::Widget* ptr_template_obj =
          ptr_stub->AttachNew(*ptr_panel_obj, "OBJECTIVE_TEMPLATE",
                              PanelStub21::kContainer);
      ptr_stub->AttachNew(*ptr_template_obj, "OBJECTIVE_TYPE",
                          PanelStub21::kLabel);
      ptr_stub->AttachNew(*ptr_template_obj, "OBJECTIVE_STATUS",
                          PanelStub21::kCheckbox);
      ptr_stub->AttachNew(*ptr_root_obj, "MISSION_STATUS",
                          PanelStub21::kLabel);

      auto logic_objectives = std::make_unique<mods::GameInfoObjectivesLogic>(
          *ptr_root_obj, *world);
      Check(logic_objectives->ObjectivesTraitForTest() != nullptr,
            "p6b21: the local player carries MissionObjectives");
      CheckEq(logic_objectives->MissionStatusTextForTest(),
              std::string{"label-mission-in-progress"},
              "p6b21: an undecided player reads in-progress");

      if (mods::MissionObjectives* ptr_mo =
              logic_objectives->ObjectivesTraitForTest()) {
        sim::Player* ptr_player_obj = world->LocalPlayer();
        if (ptr_player_obj != nullptr) {
          const std::size_t sz_before = ptr_mo->Objectives().size();
          ptr_mo->Add(*ptr_player_obj, "find the docs", "secondary");
          CheckEq(ptr_mo->Objectives().size(), sz_before + 1,
                  "p6b21: an added objective lands");
          CheckEq(ptr_panel_obj->Children().size(),
                  ptr_mo->Objectives().size(),
                  "p6b21: the redraw rebuilds one row per objective");

          ptr_player_obj->SetWinState(sim::WinState::Won);
          Check(ptr_root_obj->Get<ui::LabelWidget>("MISSION_STATUS")
                        ->fn_get_text() ==
                    "label-mission-accomplished",
                "p6b21: a won player reads accomplished");
          ptr_player_obj->SetWinState(sim::WinState::Undefined);
        }
      }
      logic_objectives.reset();
    }

    // ———— GameInfoStatsLogic:计分板装配 ————
    // ———— GameInfoStatsLogic: the score board assembly ————
    {
      ui::Widget* ptr_root_stats = ptr_stub->MakeRoot();
      ui::Widget* ptr_player_list =
          ptr_stub->AttachNew(*ptr_root_stats, "PLAYER_LIST",
                              PanelStub21::kScrollPanel);
      ui::Widget* ptr_team_tpl =
          ptr_stub->AttachNew(*ptr_player_list, "TEAM_TEMPLATE",
                              PanelStub21::kScrollItem);
      ptr_stub->AttachNew(*ptr_team_tpl, "TEAM", PanelStub21::kLabel);
      ptr_stub->AttachNew(*ptr_team_tpl, "TEAM_SCORE",
                          PanelStub21::kLabel);
      ui::Widget* ptr_player_tpl =
          ptr_stub->AttachNew(*ptr_player_list, "PLAYER_TEMPLATE",
                              PanelStub21::kContainer);
      ptr_stub->AttachNew(*ptr_player_tpl, "NAME",
                          PanelStub21::kLabelTooltip);
      ptr_stub->AttachNew(*ptr_player_tpl, "FACTIONFLAG",
                          PanelStub21::kImage);
      ptr_stub->AttachNew(*ptr_player_tpl, "FACTION",
                          PanelStub21::kLabelTooltip);
      ptr_stub->AttachNew(*ptr_player_tpl, "SCORE", PanelStub21::kLabel);
      ptr_stub->AttachNew(*ptr_player_tpl, "MUTE", PanelStub21::kCheckbox);
      ptr_stub->AttachNew(*ptr_player_tpl, "KICK", PanelStub21::kButton);
      ui::Widget* ptr_spectator_tpl =
          ptr_stub->AttachNew(*ptr_player_list, "SPECTATOR_TEMPLATE",
                              PanelStub21::kContainer);
      ptr_stub->AttachNew(*ptr_spectator_tpl, "NAME",
                          PanelStub21::kLabelTooltip);
      ptr_stub->AttachNew(*ptr_spectator_tpl, "KICK",
                          PanelStub21::kButton);
      ptr_stub->AttachNew(*ptr_spectator_tpl, "MUTE",
                          PanelStub21::kCheckbox);
      ui::Widget* ptr_headers =
          ptr_stub->AttachNew(*ptr_root_stats, "STATS_HEADERS",
                              PanelStub21::kContainer);
      ptr_stub->AttachNew(*ptr_headers, "ACTIONS", PanelStub21::kLabel);
      ptr_stub->AttachNew(*ptr_root_stats, "STATS_CHECKBOX",
                          PanelStub21::kCheckbox);
      ptr_stub->AttachNew(*ptr_root_stats, "STATS_STATUS",
                          PanelStub21::kLabel);
      ptr_stub->AttachNew(*ptr_root_stats, "OBJECTIVE",
                          PanelStub21::kContainer);

      std::size_t sz_playable21 = 0;
      std::set<int> set_teams21;
      std::size_t sz_spectators21 = 0;
      for (const sim::Player* p : world->Players()) {
        if (!p->NonCombatant() && p->Playable()) {
          ++sz_playable21;
          const net::SessionClient* ptr_client_21 =
              world->LobbyInfo().ClientWithIndex(p->ClientIndex());
          set_teams21.insert(ptr_client_21 != nullptr ? ptr_client_21->Team
                                                       : 0);
        }
      }
      for (const net::SessionClient& client_21 :
           game_.OrderManagerFace()->LobbyInfo().vec_clients)
        if (client_21.IsObserver())
          ++sz_spectators21;

      world->SetLocalPlayerForTest(player_a);
      if (net::SessionClient* ptr_local_client_21 =
              game_.OrderManagerFace()->LocalClient())
        ptr_local_client_21->Slot = "slot0";
      sz_spectators21 = 0;
      for (const net::SessionClient& client_21 :
           game_.OrderManagerFace()->LobbyInfo().vec_clients)
        if (client_21.IsObserver())
          ++sz_spectators21;

      auto logic_stats = std::make_unique<mods::GameInfoStatsLogic>(
          *ptr_root_stats, mod_data, *world, *game_.OrderManagerFace(),
          nullptr, [](bool) {}, [] {});

      CheckEq(logic_stats->PlayerRowsForTest().size(), sz_playable21,
              "p6b21: every playable combatant renders a row");
      Check(logic_stats->TeamHeadersForTest().empty() ==
                (set_teams21.size() <= 1),
            "p6b21: headers appear iff more than one team");
      CheckEq(logic_stats->SpectatorRowsForTest().size(), sz_spectators21,
              "p6b21: spectator rows match the observer clients");
      const bool b_local_combatant21 = world->LocalPlayer() != nullptr &&
                                       !world->LocalPlayer()->NonCombatant();
      CheckEq(logic_stats->StatusTextForTest(),
              b_local_combatant21
                  ? std::string{"label-mission-in-progress"}
                  : std::string{},
              "p6b21: the status line mirrors the local player");

      if (b_local_combatant21) {
        ui::Widget* ptr_stats_status = ptr_root_stats->Get("STATS_STATUS");
        world->LocalPlayer()->SetWinState(sim::WinState::Lost);
        Check(ptr_stats_status->Get<ui::LabelWidget>("STATS_STATUS")
                      ->fn_get_text() == "label-mission-failed",
              "p6b21: a lost local player flips the status line");
        world->LocalPlayer()->SetWinState(sim::WinState::Undefined);
      }
      logic_stats.reset();
    }
  }

  // ———— Phase 6 第二十二批:Lobby 大厅面板族 ————
  // ———— Phase 6 batch 22: the lobby panel family ————
  {
    std::println("-- batch 22: lobby panels --");

    map::MapPreview& preview_spawns = cache.At(str_uid);

    static ui::ChromeMetrics metrics_panel22;
    {
      const auto dir_metrics22 =
          std::filesystem::temp_directory_path() /
          "ora_render_test_metrics_panel22";
      std::filesystem::create_directories(dir_metrics22);
      {
        std::ofstream out_metrics(dir_metrics22 / "metrics.yaml");
        const char sz_line_end = static_cast<char>(10);
        auto fn_line = [&](std::string_view sv_line) {
          out_metrics << sv_line << sz_line_end;
        };
        fn_line("Chrome:");
        fn_line("	TextFont: Regular");
        fn_line("	TextColor: FFFFFFFF");
        fn_line("	TextContrast: True");
        fn_line("	TextShadow: False");
        fn_line("	TextContrastColorDark: 000000FF");
        fn_line("	TextContrastColorLight: FFFFFFFF");
        fn_line("	TextContrastRadius: 1");
        fn_line("	TextDisabledColor: 808080FF");
        fn_line("	ButtonDepth: 1");
        fn_line("	ButtonFont: Regular");
        fn_line("	ButtonTextColor: FFFFFFFF");
        fn_line("	ButtonTextColorDisabled: 808080FF");
        fn_line("	ButtonTextContrast: True");
        fn_line("	ButtonTextShadow: False");
        fn_line("	ButtonTextContrastColorDark: 000000FF");
        fn_line("	ButtonTextContrastColorLight: FFFFFFFF");
        fn_line("	ButtonTextContrastRadius: 1");
        fn_line("	ButtonCursor: default");
        fn_line("	ClickSound: click_sound");
        fn_line("	ClickDisabledSound: click_disabled_sound");
        fn_line("	TextfieldFont: Regular");
        fn_line("	TextfieldColor: FFFFFFFF");
        fn_line("	TextfieldColorDisabled: 808080FF");
        fn_line("	TextfieldColorInvalid: FFFF00FF");
        fn_line("	TextfieldColorHighlight: 808080FF");
        fn_line("	NoticeSuccessColor: 00FF00FF");
        fn_line("	NoticeErrorColor: FF0000FF");
        fn_line("	NoticeInfoColor: FFFFFFFF");
        fn_line("	SpawnColor: FFFFFFFF");
        fn_line("	SpawnContrastColor: 000000FF");
        fn_line("	SpawnFont: Regular");
        fn_line("	SpawnLabelOffset: 0,0");
      }
      static fs::FileSystem fs_metrics22;
      fs_metrics22.Mount(dir_metrics22.generic_string());
      static const std::vector<std::string> vec_files_metrics22{
          "metrics.yaml"};
      metrics_panel22.Initialize(
          {&fs_metrics22, &vec_files_metrics22});
    }

    {
      mods::TabCompletionState state_tab;
      state_tab.opt_names = std::vector<std::string>{"Alpha", "Bob",
                                                     "Alice"};
      Check(mods::CompleteTabCompletion(state_tab, " ") == " ",
            "p6b22: whitespace passes through");
      Check(mods::CompleteTabCompletion(state_tab, "Al") == "Alpha ",
            "p6b22: the first prefix hit completes with a space");
      Check(mods::CompleteTabCompletion(state_tab, "Alpha ") ==
                "Alice ",
            "p6b22: a repeated completion cycles candidates");
      Check(mods::CompleteTabCompletion(state_tab, "Zed") == "Zed",
            "p6b22: a miss returns the text");
      state_tab.opt_commands = std::vector<std::string>{"help",
                                                        "kick"};
      Check(mods::CompleteTabCompletion(state_tab, "/") == "/help",
            "p6b22: a bare slash completes the first command");
      Check(mods::CompleteTabCompletion(state_tab, "/help") ==
                "/kick",
            "p6b22: commands cycle too");
      Check(mods::CompleteTabCompletion(state_tab, "Bob A") ==
                "Bob Alpha",
            "p6b22: the last word of a sentence completes");
    }

    {
      Check(mods::LatencyDescription(nullptr) == "Unknown",
            "p6b22: a null client reads unknown latency");
      net::SessionClient client_quality{};
      client_quality.ConnectionQualityKind =
          net::ConnectionQuality::Moderate;
      Check(mods::LatencyDescription(&client_quality) == "Moderate",
            "p6b22: moderate reads its label");
      CheckEq(mods::LatencyColor(&client_quality).ToString(),
              core::Color::FromArgb(255, 255, 165, 0).ToString(),
              "p6b22: moderate paints orange");
      client_quality.ConnectionQualityKind =
          net::ConnectionQuality::Good;
      CheckEq(mods::LatencyColor(&client_quality).ToString(),
              core::Color::FromArgb(255, 50, 205, 50).ToString(),
              "p6b22: good paints lime");
      client_quality.ConnectionQualityKind =
          net::ConnectionQuality::Poor;
      CheckEq(mods::LatencyColor(&client_quality).ToString(),
              core::Color::FromArgb(255, 255, 0, 0).ToString(),
              "p6b22: poor paints red");
    }

    {
      net::Session session_lobby{};
      session_lobby.vec_disabled_spawn_points = {2};
      const std::vector<int> vec_available =
          mods::AvailableSpawnPoints(4, session_lobby);
      CheckEq(vec_available,
              (std::vector<int>{1, 3, 4}),
              "p6b22: disabled spawn points drop from the pool");
      Check(!mods::InsufficientEnabledSpawnPoints(
                map::MapCache::UnknownMap(), session_lobby),
            "p6b22: a spawnless map is never insufficient");
      net::SessionClient client_obs{};
      client_obs.Name = "Spectator";
      session_lobby.vec_clients.push_back(client_obs);
      session_lobby.vec_clients.push_back(client_obs);
      session_lobby.vec_clients.push_back(client_obs);
      const bool b_insufficient = mods::InsufficientEnabledSpawnPoints(
          preview_spawns, session_lobby);
      Check(!b_insufficient,
            "p6b22: observers alone never starve the spawn pool");
      net::SessionClient client_fighter{};
      client_fighter.Name = "Fighter";
      client_fighter.Slot = "Multi0";
      client_fighter.SpawnPoint = 0;
      session_lobby.vec_clients.push_back(client_fighter);
      session_lobby.vec_clients.push_back(client_fighter);
      session_lobby.vec_clients.push_back(client_fighter);
      const int int4_spawn_count22 =
          static_cast<int>(preview_spawns.SpawnPoints().size());
      Check(mods::InsufficientEnabledSpawnPoints(preview_spawns,
                                                 session_lobby) ==
                (int4_spawn_count22 != 0 &&
                 mods::AvailableSpawnPoints(int4_spawn_count22,
                                            session_lobby)
                         .size() < 3),
            "p6b22: insufficiency compares non-observers with the pool");
    }

    struct PanelStub22 {
      std::vector<std::string> vec_loaded_ids;
      gfx::SheetBuilder builder_sheets{gfx::SheetType::BGRA, 128};
      std::unique_ptr<gfx::SpriteFont> up_font;

      void MakeFont() {
        struct FakeFont22 final : platform::IFont {
          platform::FontGlyph CreateGlyph(std::uint32_t, int,
                                          float) override {
            platform::FontGlyph glyph_ret;
            glyph_ret.offset = int2{1, 2};
            glyph_ret.int4_width = 2;
            glyph_ret.int4_height = 3;
            glyph_ret.fp4_advance = 4.0f;
            glyph_ret.vec_data.assign(2 * 3, 0);
            return glyph_ret;
          }
        };
        up_font = std::make_unique<gfx::SpriteFont>(
            "Regular", std::span<const std::uint8_t>{},
            std::make_unique<FakeFont22>(), 10, 8, 1.0f, builder_sheets,
            [](const gfx::Sprite&, core::Vector3, float, core::Vector3,
               float) {},
            [](const gfx::Sprite&, core::Vector3, core::Vector3,
               core::Vector3, core::Vector3, core::Vector3, float) {});
      }

      enum Kind {
        kContainer,
        kLabel,
        kLabelTooltip,
        kButton,
        kImage,
        kCheckbox,
        kDropDown,
        kScrollPanel,
        kScrollItem,
        kTextField,
        kColorBlock,
        kProgressBar,
        kClientTooltip,
        kMapPreview
      };

      std::unique_ptr<ui::Widget> MakeNode(std::string_view str_id,
                                           Kind kind_node) {
        ui::WidgetArgs args_stub;
        std::unique_ptr<ui::Widget> up_node;
        switch (kind_node) {
          case kLabel:
            up_node = std::make_unique<ui::LabelWidget>(args_stub);
            break;
          case kLabelTooltip:
            up_node =
                std::make_unique<ui::LabelWithTooltipWidget>(args_stub);
            break;
          case kButton:
            up_node = std::make_unique<ui::ButtonWidget>(args_stub);
            break;
          case kImage:
            up_node = std::make_unique<ui::ImageWidget>();
            break;
          case kCheckbox:
            up_node = std::make_unique<ui::CheckboxWidget>(args_stub);
            break;
          case kDropDown:
            up_node =
                std::make_unique<ui::DropDownButtonWidget>(args_stub);
            break;
          case kScrollPanel:
            up_node =
                std::make_unique<ui::ScrollPanelWidget>(args_stub);
            break;
          case kScrollItem:
            up_node =
                std::make_unique<ui::ScrollItemWidget>(args_stub);
            break;
          case kTextField:
            up_node = std::make_unique<ui::TextFieldWidget>(args_stub);
            break;
          case kColorBlock:
            up_node =
                std::make_unique<ui::ColorBlockWidget>(args_stub);
            break;
          case kProgressBar:
            up_node = std::make_unique<ui::ProgressBarWidget>();
            break;
          case kClientTooltip:
            up_node =
                std::make_unique<ui::ClientTooltipRegionWidget>();
            break;
          case kMapPreview:
            up_node = std::make_unique<ui::MapPreviewWidget>();
            break;
          default:
            up_node = std::make_unique<ui::ContainerWidget>();
            break;
        }
        up_node->str_id = std::string{str_id};
        up_node->bounds_widget.int4_width = 200;
        up_node->bounds_widget.int4_height = 20;
        return up_node;
      }

      ui::Widget* AttachNew(ui::Widget& ref_parent, std::string_view str_id,
                           Kind kind_node) {
        std::unique_ptr<ui::Widget> up_node = MakeNode(str_id, kind_node);
        ui::Widget* ptr_node = up_node.get();
        ref_parent.AddChild(std::move(up_node));
        return ptr_node;
      }

      void MakePlayerRowTree(ui::Widget& ref_row, bool b_editable) {
        ui::Widget* ptr_latency =
            AttachNew(ref_row, "LATENCY", PanelStub22::kContainer);
        AttachNew(*ptr_latency, "LATENCY_COLOR", PanelStub22::kColorBlock);
        AttachNew(ref_row, "LATENCY_REGION", PanelStub22::kClientTooltip);
        AttachNew(ref_row, "PROFILE", PanelStub22::kImage);
        AttachNew(ref_row, "PROFILE_TOOLTIP", PanelStub22::kClientTooltip);
        if (b_editable)
          AttachNew(ref_row, "NAME", PanelStub22::kTextField);
        else
          AttachNew(ref_row, "NAME", PanelStub22::kLabel);
        AttachNew(ref_row, "SLOT_OPTIONS", PanelStub22::kDropDown);
        AttachNew(ref_row, "PLAYER_ACTION", PanelStub22::kDropDown);
        ui::Widget* ptr_color =
            AttachNew(ref_row, "COLOR", PanelStub22::kDropDown);
        AttachNew(*ptr_color, "COLORBLOCK", PanelStub22::kColorBlock);
        ui::Widget* ptr_faction =
            AttachNew(ref_row, "FACTION", PanelStub22::kDropDown);
        AttachNew(*ptr_faction, "FACTIONNAME", PanelStub22::kLabel);
        AttachNew(*ptr_faction, "FACTIONFLAG", PanelStub22::kImage);
        AttachNew(ref_row, "TEAM", PanelStub22::kLabel);
        AttachNew(ref_row, "TEAM_DROPDOWN", PanelStub22::kDropDown);
        AttachNew(ref_row, "HANDICAP", PanelStub22::kLabel);
        AttachNew(ref_row, "HANDICAP_DROPDOWN", PanelStub22::kDropDown);
        AttachNew(ref_row, "SPAWN", PanelStub22::kLabel);
        AttachNew(ref_row, "SPAWN_DROPDOWN", PanelStub22::kDropDown);
        AttachNew(ref_row, "STATUS_CHECKBOX", PanelStub22::kCheckbox);
        AttachNew(ref_row, "STATUS_IMAGE", PanelStub22::kImage);
        AttachNew(ref_row, "JOIN", PanelStub22::kButton);
        AttachNew(ref_row, "SPECTATE", PanelStub22::kButton);
        AttachNew(ref_row, "TOGGLE_SPECTATORS", PanelStub22::kCheckbox);
      }

      ui::Widget* MakeLoadedTree(const std::string& str_id) {
        auto up_root = std::make_unique<ui::ContainerWidget>();
        up_root->str_id = str_id;
        ui::Widget* ptr_root = up_root.get();
        if (str_id == "MAP_PREVIEW") {
          for (const char* sz_variant : {"MAP_LARGE", "MAP_SMALL"}) {
            ui::Widget* ptr_variant =
                AttachNew(*ptr_root, sz_variant, kContainer);
            AttachNew(*ptr_variant, "MAP_PREVIEW", kMapPreview);
            AttachNew(*ptr_variant, "MAP_TITLE", kLabelTooltip);
          }
          const char* vec_panels[] = {
              "MAP_AVAILABLE",
              "MAP_INCOMPATIBLE",
              "MAP_VALIDATING",
              "MAP_GENERATING",
              "MAP_GENERATION_ERROR",
              "MAP_UPDATE_AVAILABLE",
              "MAP_DOWNLOAD_AVAILABLE",
              "MAP_UPDATE_DOWNLOAD_AVAILABLE",
              "MAP_SEARCHING",
              "MAP_DOWNLOADING",
              "MAP_UNAVAILABLE",
              "MAP_ERROR",
          };
          for (const char* sz_panel : vec_panels) {
            ui::Widget* ptr_panel =
                AttachNew(*ptr_root, sz_panel, PanelStub22::kContainer);
            if (std::string{sz_panel}.find("DOWNLOAD_AVAILABLE") !=
                std::string::npos)
              AttachNew(*ptr_panel, "MAP_INSTALL", PanelStub22::kButton);
            if (std::string{sz_panel} == "MAP_AVAILABLE" ||
                std::string{sz_panel} == "MAP_DOWNLOAD_AVAILABLE")
              AttachNew(*ptr_panel, "MAP_TYPE", PanelStub22::kLabel);
            if (std::string{sz_panel} == "MAP_AVAILABLE" ||
                std::string{sz_panel} ==
                    "MAP_DOWNLOAD_AVAILABLE")
              AttachNew(*ptr_panel, "MAP_AUTHOR", PanelStub22::kLabel);
            if (std::string{sz_panel} == "MAP_DOWNLOADING") {
              AttachNew(*ptr_panel, "MAP_PROGRESSBAR", PanelStub22::kProgressBar);
              AttachNew(*ptr_panel, "MAP_STATUS_DOWNLOADING", PanelStub22::kLabel);
            }
          }
          AttachNew(*ptr_root, "MAP_TITLE", PanelStub22::kLabelTooltip);
          AttachNew(*ptr_root, "MAP_UPDATE", PanelStub22::kButton);
          AttachNew(*ptr_root, "MAP_RETRY", PanelStub22::kButton);
        } else if (str_id == "LOBBY_PLAYER_BIN") {
          ui::Widget* ptr_players =
              AttachNew(*ptr_root, "LOBBY_PLAYERS", PanelStub22::kScrollPanel);
          const char* vec_templates[] = {
              "TEMPLATE_EDITABLE_PLAYER",
              "TEMPLATE_NONEDITABLE_PLAYER",
              "TEMPLATE_EMPTY",
              "TEMPLATE_EDITABLE_SPECTATOR",
              "TEMPLATE_NONEDITABLE_SPECTATOR",
              "TEMPLATE_NEW_SPECTATOR",
          };
          for (const char* sz_template : vec_templates) {
            ui::Widget* ptr_row =
                AttachNew(*ptr_players, sz_template, PanelStub22::kContainer);
            const bool b_editable =
                std::string{sz_template}.find("NONEDITABLE") ==
                std::string::npos;
            MakePlayerRowTree(*ptr_row, b_editable);
          }
        } else if (str_id == "LOBBY_OPTIONS_BIN") {
          ui::Widget* ptr_panel =
              AttachNew(*ptr_root, "", PanelStub22::kScrollPanel);
          ui::Widget* ptr_container =
              AttachNew(*ptr_panel, "LOBBY_OPTIONS", PanelStub22::kContainer);
          ptr_container->bounds_widget.int4_y = 7;
          ui::Widget* ptr_checkbox_row =
              AttachNew(*ptr_container, "CHECKBOX_ROW_TEMPLATE",
                        kContainer);
          AttachNew(*ptr_checkbox_row, "CHECKBOX_1", PanelStub22::kCheckbox);
          AttachNew(*ptr_checkbox_row, "CHECKBOX_2", PanelStub22::kCheckbox);
          ui::Widget* ptr_dropdown_row =
              AttachNew(*ptr_container, "DROPDOWN_ROW_TEMPLATE",
                        kContainer);
          AttachNew(*ptr_dropdown_row, "DROPDOWN_1", PanelStub22::kDropDown);
          AttachNew(*ptr_dropdown_row, "DROPDOWN_1_DESC", PanelStub22::kLabel);
        } else if (str_id == "FORCE_START_DIALOG") {
          AttachNew(*ptr_root, "KICK_WARNING", PanelStub22::kLabel);
          AttachNew(*ptr_root, "OK_BUTTON", PanelStub22::kButton);
          AttachNew(*ptr_root, "CANCEL_BUTTON", PanelStub22::kButton);
        } else if (str_id == "LABEL_DROPDOWN_TEMPLATE") {
          ui::Widget* ptr_panel =
              AttachNew(*ptr_root, "PANEL22", PanelStub22::kScrollPanel);
          ui::Widget* ptr_item =
              AttachNew(*ptr_panel, "TEMPLATE", PanelStub22::kScrollItem);
          AttachNew(*ptr_item, "LABEL", PanelStub22::kLabel);
        }
        return up_root.release();
      }
    };

    auto up_stub22 = std::make_unique<PanelStub22>();
    up_stub22->MakeFont();
    PanelStub22* ptr_stub22 = up_stub22.get();
    static std::unique_ptr<PanelStub22> up_stub_holder22;
    up_stub_holder22 = std::move(up_stub22);

    ui::Ui::Deps deps_ui_22;
    deps_ui_22.ptr_chrome_metrics = &metrics_panel22;
    static gfx::ChromeProvider chrome_provider22;
    chrome_provider22.Initialize(
        {.ptr_file_system = &mod_data.ModFiles(),
         .vec_chrome_files = &mod_data.ManifestRef().Chrome(),
         .fp4_dpi_scale = 1.0f});
    deps_ui_22.ptr_chrome_provider = &chrome_provider22;
    deps_ui_22.fn_window_size = [] { return int2{1280, 720}; };
    deps_ui_22.fn_try_get_font =
        [ptr_font = up_stub_holder22->up_font.get()](
            const std::string&) { return ptr_font; };
    deps_ui_22.fn_load_widget =
        [ptr_stub22](ui::WidgetArgs&, ui::Widget* ptr_parent,
                     const std::string& str_id) -> ui::Widget* {
      ptr_stub22->vec_loaded_ids.push_back(str_id);
      ui::Widget* ptr_panel = ptr_stub22->MakeLoadedTree(str_id);
      if (ptr_parent != nullptr)
        ptr_parent->AddChild(
            std::unique_ptr<ui::Widget>(ptr_panel));
      return ptr_panel;
    };
    deps_ui_22.ptr_map_cache = &cache;
    deps_ui_22.fn_is_host = [] { return true; };
    deps_ui_22.fn_player_name = [] {
      return std::string{"Commander"};
    };
    static int int4_lobby_saves22 = 0;
    deps_ui_22.fn_settings_save = [] { ++int4_lobby_saves22; };
    static std::string str_lobby_player_name22;
    deps_ui_22.fn_player_name_set = [](
        const std::string& str_next) {
      str_lobby_player_name22 = str_next;
    };
    deps_ui_22.fn_clock = [] { return std::int64_t{1000}; };
    ui::Ui::Instance().Initialize(std::move(deps_ui_22));

    net::TextNotificationsManager::Instance().ResetForTest();

    {
      ui::Widget* ptr_map_preview_root = ptr_stub22->MakeLoadedTree(
          "MAP_PREVIEW");

      auto logic_preview_available =
          std::make_unique<mods::MapPreviewLogic>(
              *ptr_map_preview_root, mod_data,
              [&preview_spawns]() {
                return std::make_pair(
                    &preview_spawns,
                    static_cast<int>(
                        net::SessionMapStatus::Playable));
              },
              nullptr, nullptr, false, nullptr, nullptr, true);
      logic_preview_available->Tick();
      Check(logic_preview_available->StatusForTest() ==
                mods::MapPreviewLogic::PreviewStatus::Playable,
            "p6b22: an available map with the playable flag reads "
            "playable");
      Check(!logic_preview_available->RetryTriggeredForTest(),
            "p6b22: playable never trips the retry latch");

      auto logic_preview_unknown =
          std::make_unique<mods::MapPreviewLogic>(
              *ptr_map_preview_root, mod_data,
              []() {
                return std::make_pair(
                    &map::MapCache::UnknownMap(), 0);
              },
              nullptr, nullptr, false, nullptr, nullptr, true);
      logic_preview_unknown->Tick();
      Check(logic_preview_unknown->StatusForTest() ==
                mods::MapPreviewLogic::PreviewStatus::Unknown,
            "p6b22: the unknown map reads unknown");

      auto logic_preview_incompatible =
          std::make_unique<mods::MapPreviewLogic>(
              *ptr_map_preview_root, mod_data,
              [&preview_spawns]() {
                return std::make_pair(
                    &preview_spawns,
                    static_cast<int>(
                        net::SessionMapStatus::Incompatible));
              },
              nullptr, nullptr, false, nullptr, nullptr, true);
      logic_preview_incompatible->Tick();
      Check(logic_preview_incompatible->StatusForTest() ==
                mods::MapPreviewLogic::PreviewStatus::Incompatible,
            "p6b22: the incompatible flag reads incompatible");
      logic_preview_incompatible.reset();
      logic_preview_available.reset();
      logic_preview_unknown.reset();
      ui::Widget* ptr_released_map_tree = ptr_map_preview_root;
      delete ptr_released_map_tree;
    }

    {
      static std::function<
          std::vector<server::LobbyOptionData>(const map::MapPreview&)>
          fn_collector_options = [](const map::MapPreview&) {
            std::vector<server::LobbyOptionData> vec_options;
            vec_options.push_back(
                server::MakeLobbyBooleanOption(
                    "cheats", "label-cheats", "", true, 0, false,
                    false));
            server::LobbyOptionData option_difficulty{};
            option_difficulty.Id = "difficulty";
            option_difficulty.Name = "label-difficulty";
            option_difficulty.vec_values = {{"e", "Easy"},
                                            {"h", "Hard"}};
            option_difficulty.DefaultValue = "e";
            vec_options.push_back(option_difficulty);
            return vec_options;
          };
      mods::SetMapLobbyOptionsCollectorForTest(&fn_collector_options);

      net::OrderManager& om_options =
          *game_.OrderManagerFace();
      om_options.LobbyInfo().global_settings.map_lobby_options.clear();
      net::SessionLobbyOption option_cheats{};
      option_cheats.Value = "False";
      om_options.LobbyInfo()
          .global_settings.map_lobby_options.emplace("cheats",
                                                     option_cheats);
      net::SessionLobbyOption option_difficulty_state{};
      option_difficulty_state.Value = "h";
      om_options.LobbyInfo()
          .global_settings.map_lobby_options.emplace(
              "difficulty", option_difficulty_state);

      ui::Widget* ptr_options_bin = ptr_stub22->MakeLoadedTree(
          "LOBBY_OPTIONS_BIN");
      ui::Widget* ptr_options_panel =
          ptr_options_bin->GetOrNull("");
      ui::ScrollPanelWidget* ptr_options_scroll =
          dynamic_cast<ui::ScrollPanelWidget*>(ptr_options_panel);
      mods::LobbyOptionsLogic logic_options(
          dynamic_cast<ui::Widget&>(*ptr_options_scroll), om_options,
          [&preview_spawns]() { return &preview_spawns; },
          [] { return false; });

      CheckEq(logic_options.RowCountForTest(), std::size_t{2},
              "p6b22: the options panel stacks a checkbox and a "
              "dropdown row");
      ui::Widget* ptr_container_rows =
          ptr_options_panel->Get("LOBBY_OPTIONS");
      const auto vec_rows = ptr_container_rows->Children();
      ui::CheckboxWidget* ptr_checkbox_cheats =
          dynamic_cast<ui::CheckboxWidget*>(
              vec_rows[0]->Children()[0]);
      Check(ptr_checkbox_cheats != nullptr &&
                ptr_checkbox_cheats->fn_get_text() == "label-cheats",
            "p6b22: the checkbox row carries the option name");
      Check(!ptr_checkbox_cheats->fn_is_checked(),
            "p6b22: the checkbox mirrors the session value");
      ptr_checkbox_cheats->fn_on_click();
      Check(ptr_checkbox_cheats->fn_is_checked(),
            "p6b22: a click predicts the toggled state");
      ui::DropDownButtonWidget* ptr_dropdown_difficulty =
          dynamic_cast<ui::DropDownButtonWidget*>(
              vec_rows[1]->Children()[0]);
      Check(ptr_dropdown_difficulty != nullptr &&
                ptr_dropdown_difficulty->fn_get_text() == "Hard",
            "p6b22: the dropdown reads the value label");
      Check(ptr_dropdown_difficulty->str_id == "DROPDOWN_1" &&
            ptr_container_rows->GetOrNull("DROPDOWN_1_DESC") !=
                nullptr,
            "p6b22: the row wires the dropdown description label");

      logic_options.Tick();
      mods::SetMapLobbyOptionsCollectorForTest(nullptr);
      delete ptr_options_bin;
    }

    {
      net::OrderManager& om_lobby = *game_.OrderManagerFace();
      auto& session_lobby = om_lobby.LobbyInfo();
      session_lobby.vec_clients.clear();
      session_lobby.vec_slots.clear();
      session_lobby.vec_disabled_spawn_points.clear();
      session_lobby.global_settings.ServerName = "Batch 22 Server";
      session_lobby.global_settings.Map = str_uid;
      session_lobby.global_settings.MapStatusFlags =
          static_cast<int>(net::SessionMapStatus::Playable);
      session_lobby.global_settings.AllowSpectators = true;
      session_lobby.global_settings.EnableSingleplayer = true;
      session_lobby.global_settings.map_lobby_options.clear();

      net::SessionClient client_host{};
      client_host.Index = 1;
      client_host.Name = "Host Commander";
      client_host.Slot = "Multi0";
      client_host.IsAdmin = true;
      client_host.Faction = "allies";
      client_host.Team = 1;
      session_lobby.vec_clients.push_back(client_host);

      net::SessionClient client_spectator{};
      client_spectator.Index = 2;
      client_spectator.Name = "Watcher";
      session_lobby.vec_clients.push_back(client_spectator);

      net::SessionSlot slot_first{};
      slot_first.PlayerReference = "Multi0";
      session_lobby.vec_slots.emplace_back("Multi0", slot_first);
      net::SessionSlot slot_second{};
      slot_second.PlayerReference = "Multi1";
      session_lobby.vec_slots.emplace_back("Multi1", slot_second);

      auto up_lobby_root = std::make_unique<ui::ContainerWidget>();
      up_lobby_root->str_id = "SERVER_LOBBY";
      ui::Widget* ptr_lobby_root = up_lobby_root.release();
      ptr_stub22->AttachNew(*ptr_lobby_root, "SERVER_NAME", PanelStub22::kLabel);
      ptr_stub22->AttachNew(*ptr_lobby_root, "MAP_PREVIEW_ROOT", PanelStub22::kContainer);
      ptr_stub22->AttachNew(*ptr_lobby_root, "TOP_PANELS_ROOT", PanelStub22::kContainer);
      ptr_stub22->AttachNew(*ptr_lobby_root, "CHANGEMAP_BUTTON", PanelStub22::kButton);
      ptr_stub22->AttachNew(*ptr_lobby_root, "SLOTS_DROPDOWNBUTTON", PanelStub22::kDropDown);
      ptr_stub22->AttachNew(*ptr_lobby_root, "RESET_OPTIONS_BUTTON", PanelStub22::kButton);
      ptr_stub22->AttachNew(*ptr_lobby_root, "START_GAME_BUTTON", PanelStub22::kButton);
      ptr_stub22->AttachNew(*ptr_lobby_root, "SETTINGS_BUTTON", PanelStub22::kButton);
      ui::Widget* ptr_skirmish_tabs =
          ptr_stub22->AttachNew(*ptr_lobby_root, "SKIRMISH_TABS", PanelStub22::kContainer);
      ptr_stub22->AttachNew(*ptr_skirmish_tabs, "OPTIONS_TAB", PanelStub22::kButton);
      ptr_stub22->AttachNew(*ptr_skirmish_tabs, "PLAYERS_TAB", PanelStub22::kButton);
      ptr_stub22->AttachNew(*ptr_skirmish_tabs, "MUSIC_TAB", PanelStub22::kButton);
      ptr_stub22->AttachNew(*ptr_skirmish_tabs, "SERVERS_TAB", PanelStub22::kButton);
      ptr_stub22->AttachNew(*ptr_lobby_root, "CHAT_MODE", PanelStub22::kButton);
      ptr_stub22->AttachNew(*ptr_lobby_root, "CHAT_TEXTFIELD", PanelStub22::kTextField);
      ui::Widget* ptr_chat_display =
          ptr_stub22->AttachNew(*ptr_lobby_root, "CHAT_DISPLAY", PanelStub22::kScrollPanel);
      ptr_chat_display->bounds_widget.int4_width = 400;
      ptr_stub22->AttachNew(*ptr_lobby_root, "DISCONNECT_BUTTON", PanelStub22::kButton);

      int int4_on_start_count22 = 0;
      auto logic_lobby = std::make_unique<mods::LobbyLogic>(
          *ptr_lobby_root, mod_data, nullptr, om_lobby,
          [] {}, [&int4_on_start_count22] { ++int4_on_start_count22; },
          true, nullptr);

      Check(logic_lobby->PanelForTest() ==
                mods::LobbyLogic::PanelType::Players,
            "p6b22: the lobby opens on the players panel");
      Check(std::find(ptr_stub22->vec_loaded_ids.begin(),
                      ptr_stub22->vec_loaded_ids.end(),
                      "MAP_PREVIEW") !=
                ptr_stub22->vec_loaded_ids.end() &&
                std::find(ptr_stub22->vec_loaded_ids.begin(),
                          ptr_stub22->vec_loaded_ids.end(),
                          "LOBBY_PLAYER_BIN") !=
                    ptr_stub22->vec_loaded_ids.end(),
            "p6b22: the lobby assembles its preview and player bins");

      logic_lobby->UpdatePlayerListForTest();

      ui::ScrollPanelWidget* ptr_players_list =
          ptr_lobby_root->Get<ui::ScrollPanelWidget>("LOBBY_PLAYERS");
      ui::Widget* ptr_player_bin = ptr_players_list->ptr_parent;
      Check(ptr_player_bin != nullptr &&
                ptr_player_bin->str_id == "LOBBY_PLAYER_BIN",
            "p6b22: the player bin attaches below the top-panels "
            "root");
      CheckEq(ptr_players_list->Children().size(), std::size_t{4},
              "p6b22: two slots plus a spectator plus the spectate "
              "button");
      Check(ptr_players_list->Children()[0]->str_id ==
                "TEMPLATE_EDITABLE_PLAYER" &&
            ptr_players_list->Children()[1]->str_id ==
                "TEMPLATE_EMPTY" &&
            ptr_players_list->Children()[2]->str_id ==
                "TEMPLATE_NONEDITABLE_SPECTATOR" &&
            ptr_players_list->Children()[3]->str_id ==
                "TEMPLATE_NEW_SPECTATOR",
            "p6b22: the local editable row, the empty slot, the "
            "spectator and the spectate button stack in order");
      ui::CheckboxWidget* ptr_status_checkbox =
          ptr_players_list->Children()[0]->GetOrNull<
              ui::CheckboxWidget>("STATUS_CHECKBOX");
      Check(ptr_status_checkbox != nullptr &&
                ptr_status_checkbox->fn_is_visible(),
            "p6b22: the editable row exposes its ready checkbox");

      CheckEq(logic_lobby->TabCompletionForTest()
                  .opt_names->size(),
              std::size_t{2},
              "p6b22: tab completion deduplicates player names");

      logic_lobby->UpdateOptionsForTest();
      Check(logic_lobby->InsufficientPlayerSpawnsForTest() ==
                (mods::AvailableSpawnPoints(
                     static_cast<int>(
                         preview_spawns.SpawnPoints().size()),
                     session_lobby)
                     .size() < 2),
            "p6b22: the spawn pool sizes against the lobby");

      net::TextNotificationsManager::Instance()
          .SetChatDisabledUntil(1000 + 5000);
      logic_lobby->TickForTest();
      Check(!logic_lobby->ChatEnabledForTest(),
            "p6b22: chat stays disabled while the cooldown runs");
      Check(!ptr_lobby_root->Get<ui::TextFieldWidget>("CHAT_TEXTFIELD")
                  ->Text()
                  .empty(),
            "p6b22: the text field carries the countdown");
      net::TextNotificationsManager::Instance()
          .SetChatDisabledUntil(0);
      logic_lobby->TickForTest();
      Check(logic_lobby->ChatEnabledForTest(),
            "p6b22: chat re-enables once the cooldown lapses");

      logic_lobby.reset();
      delete ptr_lobby_root;
    }

    // ———— Phase 6 第二十三批:Settings 面板族 ————
    {
      std::println("-- batch 23: settings panels --");
      struct PanelStub23 {
        std::vector<std::string> vec_loaded_ids;

        enum Kind {
          kContainer,
          kLabel,
          kButton,
          kCheckbox,
          kDropDown,
          kScrollPanel,
          kTextField,
          kPassword,
          kColorBlock,
          kSlider,
          kHotkeyEntry
        };

        std::unique_ptr<ui::Widget> MakeNode(std::string_view str_id,
                                             Kind kind_node) {
          ui::WidgetArgs args_stub;
          std::unique_ptr<ui::Widget> up_node;
          switch (kind_node) {
            case kLabel:
              up_node = std::make_unique<ui::LabelWidget>(args_stub);
              break;
            case kButton:
              up_node = std::make_unique<ui::ButtonWidget>(args_stub);
              break;
            case kCheckbox:
              up_node = std::make_unique<ui::CheckboxWidget>(args_stub);
              break;
            case kDropDown:
              up_node =
                  std::make_unique<ui::DropDownButtonWidget>(args_stub);
              break;
            case kScrollPanel:
              up_node =
                  std::make_unique<ui::ScrollPanelWidget>(args_stub);
              break;
            case kTextField:
              up_node = std::make_unique<ui::TextFieldWidget>(args_stub);
              break;
            case kPassword:
              up_node =
                  std::make_unique<ui::PasswordFieldWidget>(args_stub);
              break;
            case kColorBlock:
              up_node =
                  std::make_unique<ui::ColorBlockWidget>(args_stub);
              break;
            case kSlider:
              up_node = std::make_unique<ui::SliderWidget>(args_stub);
              break;
            case kHotkeyEntry:
              up_node = std::make_unique<ui::HotkeyEntryWidget>();
              break;
            default:
              up_node = std::make_unique<ui::ContainerWidget>();
              break;
          }
          up_node->str_id = std::string{str_id};
          up_node->bounds_widget.int4_width = 200;
          up_node->bounds_widget.int4_height = 20;
          return up_node;
        }

        ui::Widget* AttachNew(ui::Widget& ref_parent, std::string_view str_id,
                             Kind kind_node) {
          std::unique_ptr<ui::Widget> up_node = MakeNode(str_id, kind_node);
          ui::Widget* ptr_node = up_node.get();
          ref_parent.AddChild(std::move(up_node));
          return ptr_node;
        }

        void AddCheckboxes(ui::Widget& ref_parent,
                           std::initializer_list<std::string_view> vec_ids) {
          for (const std::string_view sv_id : vec_ids)
            AttachNew(ref_parent, sv_id, kCheckbox);
        }

        ui::Widget* MakePanelTree(const std::string& str_id) {
          auto up_root = std::make_unique<ui::ContainerWidget>();
          up_root->str_id = str_id;
          ui::Widget* ptr_root = up_root.get();

          if (str_id == "AUDIO_PANEL") {
            AttachNew(*ptr_root, "SETTINGS_SCROLLPANEL", kScrollPanel);
            AddCheckboxes(*ptr_root,
                          {"CASH_TICKS", "MUTE_SOUND",
                           "MUTE_BACKGROUND_MUSIC"});
            AttachNew(*ptr_root, "SOUND_VOLUME", kSlider);
            AttachNew(*ptr_root, "MUSIC_VOLUME", kSlider);
            AttachNew(*ptr_root, "VIDEO_VOLUME", kSlider);
            AttachNew(*ptr_root, "AUDIO_DEVICE", kDropDown);
            AttachNew(*ptr_root, "AUDIO_RESTART_REQUIRED_DESC", kLabel);
            for (const std::string_view sv_id :
                 {"NO_AUDIO_DEVICE_CONTAINER", "CASH_TICKS_CONTAINER",
                  "MUTE_SOUND_CONTAINER",
                  "MUTE_BACKGROUND_MUSIC_CONTAINER",
                  "SOUND_VOLUME_CONTAINER", "MUSIC_VOLUME_CONTAINER",
                  "VIDEO_VOLUME_CONTAINER"})
              AttachNew(*ptr_root, sv_id, kContainer);
          } else if (str_id == "ADVANCED_PANEL") {
            AttachNew(*ptr_root, "SETTINGS_SCROLLPANEL", kScrollPanel);
            AddCheckboxes(*ptr_root,
                          {"NAT_DISCOVERY", "PERFTEXT_CHECKBOX",
                           "PERFGRAPH_CHECKBOX", "FETCH_NEWS_CHECKBOX",
                           "SENDSYSINFO_CHECKBOX",
                           "CHECK_VERSION_CHECKBOX",
                           "ENABLE_DISCORD_SERVICE",
                           "BOTDEBUG_CHECKBOX", "LUADEBUG_CHECKBOX",
                           "REPLAY_COMMANDS_CHECKBOX",
                           "CHECKUNSYNCED_CHECKBOX",
                           "CHECKBOTSYNC_CHECKBOX",
                           "PERFLOGGING_CHECKBOX"});
            for (const std::string_view sv_id :
                 {"BOTDEBUG_CHECKBOX_CONTAINER",
                  "CHECKUNSYNCED_CHECKBOX_CONTAINER",
                  "CHECKBOTSYNC_CHECKBOX_CONTAINER",
                  "LUADEBUG_CHECKBOX_CONTAINER",
                  "REPLAY_COMMANDS_CHECKBOX_CONTAINER",
                  "PERFLOGGING_CHECKBOX_CONTAINER",
                  "DEBUG_HIDDEN_CONTAINER"})
              AttachNew(*ptr_root, sv_id, kContainer);
          } else if (str_id == "DISPLAY_PANEL") {
            AttachNew(*ptr_root, "SETTINGS_SCROLLPANEL", kScrollPanel);
            AddCheckboxes(*ptr_root,
                          {"CURSORDOUBLE_CHECKBOX", "VSYNC_CHECKBOX",
                           "FRAME_LIMIT_CHECKBOX",
                           "FRAME_LIMIT_GAMESPEED_CHECKBOX",
                           "PLAYER_STANCE_COLORS_CHECKBOX",
                           "PAUSE_SHELLMAP_CHECKBOX"});
            AttachNew(*ptr_root, "FRAME_LIMIT_SLIDER", kSlider);
            AttachNew(*ptr_root, "MODE_DROPDOWN", kDropDown);
            AttachNew(*ptr_root, "DISPLAY_SELECTION_DROPDOWN", kDropDown);
            AttachNew(*ptr_root, "GL_PROFILE_DROPDOWN", kDropDown);
            AttachNew(*ptr_root, "STATUS_BAR_DROPDOWN", kDropDown);
            AttachNew(*ptr_root, "TARGET_LINES_DROPDOWN", kDropDown);
            AttachNew(*ptr_root, "BATTLEFIELD_CAMERA_DROPDOWN", kDropDown);
            AttachNew(*ptr_root, "UI_SCALE_DROPDOWN", kDropDown);
            AttachNew(*ptr_root, "RESOLUTION_PRESET_DROPDOWN", kDropDown);
            AttachNew(*ptr_root, "WINDOW_WIDTH", kTextField);
            AttachNew(*ptr_root, "WINDOW_HEIGHT", kTextField);
            AttachNew(*ptr_root, "VIDEO_RESTART_REQUIRED_DESC", kLabel);
            AttachNew(*ptr_root, "DISPLAY_SELECTION_CONTAINER", kContainer);
            AttachNew(*ptr_root, "WINDOW_RESOLUTION_CONTAINER", kContainer);
          } else if (str_id == "INPUT_PANEL") {
            AttachNew(*ptr_root, "SETTINGS_SCROLLPANEL", kScrollPanel);
            AddCheckboxes(*ptr_root,
                          {"ALTERNATE_SCROLL_CHECKBOX",
                           "EDGESCROLL_CHECKBOX", "LOCKMOUSE_CHECKBOX"});
            AttachNew(*ptr_root, "ZOOMSPEED_SLIDER", kSlider);
            AttachNew(*ptr_root, "SCROLLSPEED_SLIDER", kSlider);
            AttachNew(*ptr_root, "UI_SCROLLSPEED_SLIDER", kSlider);
            AttachNew(*ptr_root, "MOUSE_CONTROL_DROPDOWN", kDropDown);
            AttachNew(*ptr_root, "MOUSE_SCROLL_TYPE_DROPDOWN", kDropDown);
            AttachNew(*ptr_root, "ZOOM_MODIFIER", kDropDown);
            for (const std::string_view sv_id :
                 {"MOUSE_CONTROL_DESC_CLASSIC",
                  "MOUSE_CONTROL_DESC_MODERN",
                  "MOUSE_CONTROL_DESC_OTHERRTS"}) {
              ui::Widget* ptr_desc =
                  AttachNew(*ptr_root, sv_id, kContainer);
              AttachNew(*ptr_desc, "DESC_SCROLL_RIGHT", kLabel);
              AttachNew(*ptr_desc, "DESC_SCROLL_MIDDLE", kLabel);
              AttachNew(*ptr_desc, "DESC_ZOOM", kLabel);
              AttachNew(*ptr_desc, "DESC_ZOOM_MODIFIER", kLabel);
              AttachNew(*ptr_desc, "DESC_EDGESCROLL", kLabel);
            }
          } else if (str_id == "GAMEPLAY_PANEL") {
            AttachNew(*ptr_root, "SETTINGS_SCROLLPANEL", kScrollPanel);
            AttachNew(*ptr_root, "PLAYERNAME", kTextField);
            AttachNew(*ptr_root, "USERNAME", kTextField);
            AttachNew(*ptr_root, "PASSWORD", kPassword);
            ui::Widget* ptr_color =
                AttachNew(*ptr_root, "PLAYERCOLOR", kDropDown);
            AttachNew(*ptr_color, "COLORBLOCK", kColorBlock);
            AttachNew(*ptr_root, "HIDE_REPLAY_CHAT_CHECKBOX", kCheckbox);
            AttachNew(*ptr_root, "CHECKING_FINGERPRINT", kLabel);
            AttachNew(*ptr_root, "CONNECTION_ERROR", kLabel);
            AttachNew(*ptr_root, "PROFILE_STATUS", kLabel);
            AttachNew(*ptr_root, "LINK_BUTTON", kButton);
            AttachNew(*ptr_root, "RETRY_BUTTON", kButton);
            AttachNew(*ptr_root, "FORUM_BUTTON", kButton);
            AttachNew(*ptr_root, "AUTO_SAVE_INTERVAL_DROP_DOWN", kDropDown);
            AttachNew(*ptr_root, "AUTO_SAVE_FILE_NUMBER_DROP_DOWN",
                      kDropDown);
          } else if (str_id == "HOTKEYS_PANEL") {
            ui::Widget* ptr_list =
                AttachNew(*ptr_root, "HOTKEY_LIST", kScrollPanel);
            ui::Widget* ptr_header =
                AttachNew(*ptr_list, "HEADER", kContainer);
            AttachNew(*ptr_header, "LABEL", kLabel);
            ui::Widget* ptr_template =
                AttachNew(*ptr_list, "TEMPLATE", kContainer);
            AttachNew(*ptr_template, "FUNCTION", kLabel);
            AttachNew(*ptr_template, "HOTKEY", kButton);
            AttachNew(*ptr_root, "HOTKEY_EMPTY_LIST", kContainer);
            ui::Widget* ptr_dialog =
                AttachNew(*ptr_root, "HOTKEY_REMAP_DIALOG", kContainer);
            AttachNew(*ptr_dialog, "HOTKEY_LABEL", kLabel);
            AttachNew(*ptr_dialog, "DUPLICATE_NOTICE", kLabel);
            AttachNew(*ptr_dialog, "ORIGINAL_NOTICE", kLabel);
            AttachNew(*ptr_dialog, "READONLY_NOTICE", kLabel);
            AttachNew(*ptr_dialog, "RESET_HOTKEY_BUTTON", kButton);
            AttachNew(*ptr_dialog, "CLEAR_HOTKEY_BUTTON", kButton);
            AttachNew(*ptr_dialog, "OVERRIDE_HOTKEY_BUTTON", kButton);
            AttachNew(*ptr_dialog, "HOTKEY_ENTRY", kHotkeyEntry);
            AttachNew(*ptr_root, "FILTER_INPUT", kTextField);
            AttachNew(*ptr_root, "CONTEXT_DROPDOWN", kDropDown);
          }

          up_root.release();
          return ptr_root;
        }

        ui::Widget* MakePromptTree(const std::string& str_id) {
          auto up_root = std::make_unique<ui::ContainerWidget>();
          up_root->str_id = str_id;
          ui::Widget* ptr_root = up_root.get();
          AttachNew(*ptr_root, "PROMPT_TITLE", kLabel);
          AttachNew(*ptr_root, "PROMPT_TEXT", kLabel);
          AttachNew(*ptr_root, "CONFIRM_BUTTON", kButton);
          AttachNew(*ptr_root, "CANCEL_BUTTON", kButton);
          AttachNew(*ptr_root, "OTHER_BUTTON", kButton);
          up_root.release();
          return ptr_root;
        }
      };

      struct SettingsHostStub23 final : mods::ISettingsLogic {
        std::function<std::function<bool()>(ui::Widget&)> fn_init;
        std::function<std::function<void()>(ui::Widget&)> fn_reset;

        void RegisterSettingsPanel(
            const std::string&, const std::string&,
            std::function<std::function<bool()>(ui::Widget&)> fn_init_in,
            std::function<std::function<void()>(ui::Widget&)> fn_reset_in)
            override {
          fn_init = std::move(fn_init_in);
          fn_reset = std::move(fn_reset_in);
        }

        void CaptureForTest(ui::Widget& ref_panel,
                            std::function<bool()>& fn_leave_out,
                            std::function<void()>& fn_reset_out) {
          fn_leave_out = fn_init(ref_panel);
          fn_reset_out = fn_reset(ref_panel);
        }
      };

      static ui::ChromeMetrics metrics_panel23;
      {
        const auto dir_metrics23 =
            std::filesystem::temp_directory_path() /
            "ora_render_test_metrics_panel23";
        std::filesystem::create_directories(dir_metrics23);
        {
          std::ofstream out_metrics(dir_metrics23 / "metrics.yaml");
          const char sz_line_end = static_cast<char>(10);
          auto fn_line = [&](std::string_view sv_line) {
            out_metrics << sv_line << sz_line_end;
          };
          fn_line("Chrome:");
          fn_line("	TextFont: Regular");
          fn_line("	TextColor: FFFFFFFF");
          fn_line("	TextContrast: True");
          fn_line("	TextShadow: False");
          fn_line("	TextContrastColorDark: 000000FF");
          fn_line("	TextContrastColorLight: FFFFFFFF");
          fn_line("	TextContrastRadius: 1");
          fn_line("	TextDisabledColor: 808080FF");
          fn_line("	ButtonDepth: 1");
          fn_line("	ButtonFont: Regular");
          fn_line("	ButtonTextColor: FFFFFFFF");
          fn_line("	ButtonTextColorDisabled: 808080FF");
          fn_line("	ButtonTextContrast: True");
          fn_line("	ButtonTextShadow: False");
          fn_line("	ButtonTextContrastColorDark: 000000FF");
          fn_line("	ButtonTextContrastColorLight: FFFFFFFF");
          fn_line("	ButtonTextContrastRadius: 1");
          fn_line("	ButtonCursor: default");
          fn_line("	ClickSound: click_sound");
          fn_line("	ClickDisabledSound: click_disabled_sound");
          fn_line("	TextfieldFont: Regular");
          fn_line("	TextfieldColor: FFFFFFFF");
          fn_line("	TextfieldColorDisabled: 808080FF");
          fn_line("	TextfieldColorInvalid: FFFF00FF");
          fn_line("	TextfieldColorHighlight: 808080FF");
          fn_line("	NoticeSuccessColor: 00FF00FF");
          fn_line("	NoticeErrorColor: FF0000FF");
          fn_line("	NoticeInfoColor: FFFFFFFF");
          fn_line("	HotkeyFont: Regular");
          fn_line("	HotkeyColor: FFFFFFFF");
          fn_line("	HotkeyColorDisabled: 808080FF");
          fn_line("	HotkeyColorInvalid: FF0000FF");
        }
        static fs::FileSystem fs_metrics23;
        fs_metrics23.Mount(dir_metrics23.generic_string());
        static const std::vector<std::string> vec_files_metrics23{
            "metrics.yaml"};
        metrics_panel23.Initialize(
            {&fs_metrics23, &vec_files_metrics23});
      }

      auto up_stub23 = std::make_unique<PanelStub23>();
      PanelStub23* ptr_stub23 = up_stub23.get();
      static std::unique_ptr<PanelStub23> up_stub_holder23;
      up_stub_holder23 = std::move(up_stub23);

      static int int4_settings_saves23 = 0;
      int4_settings_saves23 = 0;

      ui::Ui::Deps deps_ui_23;
      deps_ui_23.ptr_chrome_metrics = &metrics_panel23;
      deps_ui_23.ptr_chrome_provider = &chrome_provider22;
      deps_ui_23.fn_window_size = [] { return int2{1280, 720}; };
      deps_ui_23.fn_try_get_font =
          [ptr_font = up_stub_holder22->up_font.get()](
              const std::string&) { return ptr_font; };
      deps_ui_23.fn_settings_save = [] { ++int4_settings_saves23; };
      deps_ui_23.fn_load_widget =
          [ptr_stub23](ui::WidgetArgs&, ui::Widget* ptr_parent,
                       const std::string& str_id) -> ui::Widget* {
        ptr_stub23->vec_loaded_ids.push_back(str_id);
        ui::Widget* ptr_panel =
            str_id == "TWOBUTTON_PROMPT" || str_id == "THREEBUTTON_PROMPT"
                ? ptr_stub23->MakePromptTree(str_id)
                : ptr_stub23->MakePanelTree(str_id);
        if (ptr_parent != nullptr)
          ptr_parent->AddChild(
              std::unique_ptr<ui::Widget>(ptr_panel));
        return ptr_panel;
      };
      deps_ui_23.ptr_map_cache = &cache;
      deps_ui_23.fn_is_host = [] { return true; };
      deps_ui_23.fn_clock = [] { return std::int64_t{1000}; };
      ui::Ui::Instance().Initialize(std::move(deps_ui_23));

      mods::ResetSettingsPanelSnapshotsForTest();
      game::Settings::Instance().ModuleOrCreate("AutoSave");

      {
        yaml::StringPool pool_settings23;
        yaml::MiniYaml yaml_panels23{};
        yaml_panels23.Nodes.emplace_back(
            pool_settings23.Intern("GAMEPLAY_PANEL"),
            yaml::MiniYaml{
                pool_settings23.Intern("button-panel-gameplay")});
        yaml_panels23.Nodes.emplace_back(
            pool_settings23.Intern("AUDIO_PANEL"),
            yaml::MiniYaml{
                pool_settings23.Intern("button-panel-audio")});
        yaml::MiniYaml yaml_stride23{pool_settings23.Intern("10, 5")};
        const ui::YamlDictionary vec_args23{
            {std::string_view{"ButtonStride"}, &yaml_stride23},
            {std::string_view{"Panels"}, &yaml_panels23}};

        auto up_settings_root = std::make_unique<ui::ContainerWidget>();
        ui::Widget* ptr_settings_root = up_settings_root.release();
        ui::Widget* ptr_panel_container =
            ptr_stub23->AttachNew(*ptr_settings_root, "PANEL_CONTAINER",
                                  PanelStub23::kContainer);
        ptr_stub23->AttachNew(*ptr_panel_container, "PANEL_TEMPLATE",
                              PanelStub23::kContainer);
        ui::Widget* ptr_tab_container =
            ptr_stub23->AttachNew(*ptr_settings_root,
                                  "SETTINGS_TAB_CONTAINER",
                                  PanelStub23::kContainer);
        ptr_stub23->AttachNew(*ptr_tab_container, "BUTTON_TEMPLATE",
                              PanelStub23::kButton);
        auto* ptr_back_button = static_cast<ui::ButtonWidget*>(
            ptr_stub23->AttachNew(*ptr_settings_root, "BACK_BUTTON",
                                  PanelStub23::kButton));
        auto* ptr_reset_button = static_cast<ui::ButtonWidget*>(
            ptr_stub23->AttachNew(*ptr_settings_root, "RESET_BUTTON",
                                  PanelStub23::kButton));

        int int4_on_exit23 = 0;
        auto logic_settings = std::make_unique<mods::SettingsLogic>(
            *ptr_settings_root, [&int4_on_exit23] { ++int4_on_exit23; },
            nullptr, mod_data, &vec_args23);

        Check(ptr_panel_container->GetOrNull("PANEL_TEMPLATE") ==
                      nullptr &&
                  ptr_tab_container->GetOrNull("BUTTON_TEMPLATE") ==
                      nullptr,
              "p6b23: the settings root removes its templates");
        Check(ptr_panel_container->GetOrNull("GAMEPLAY_PANEL") !=
                  nullptr &&
                  ptr_panel_container->GetOrNull("AUDIO_PANEL") !=
                      nullptr,
              "p6b23: each declared panel clones into the container");
        Check(std::find(ptr_stub23->vec_loaded_ids.begin(),
                        ptr_stub23->vec_loaded_ids.end(),
                        "GAMEPLAY_PANEL") !=
                  ptr_stub23->vec_loaded_ids.end() &&
                  std::find(ptr_stub23->vec_loaded_ids.begin(),
                            ptr_stub23->vec_loaded_ids.end(),
                            "AUDIO_PANEL") !=
                      ptr_stub23->vec_loaded_ids.end(),
              "p6b23: the panels load their widget trees");
        CheckEq(ptr_tab_container->Children().size(), std::size_t{0},
                "p6b23: no tabs exist until panels register");

        bool b_leave_one_tripped = false;
        int int4_reset_one_runs = 0;
        logic_settings->RegisterSettingsPanel(
            "GAMEPLAY_PANEL", "Gameplay",
            [&b_leave_one_tripped](ui::Widget&) {
              return [&b_leave_one_tripped] {
                b_leave_one_tripped = true;
                return false;
              };
            },
            [&int4_reset_one_runs](ui::Widget&) {
              return [&int4_reset_one_runs] { ++int4_reset_one_runs; };
            });

        bool b_leave_two_tripped = false;
        bool b_leave_two_demands_restart = false;
        logic_settings->RegisterSettingsPanel(
            "AUDIO_PANEL", "Audio",
            [&b_leave_two_tripped,
             &b_leave_two_demands_restart](ui::Widget&) {
              return [&b_leave_two_tripped,
                      &b_leave_two_demands_restart] {
                b_leave_two_tripped = true;
                return b_leave_two_demands_restart;
              };
            },
            [](ui::Widget&) { return [] {}; });

        CheckEq(ptr_tab_container->Children().size(), std::size_t{2},
                "p6b23: two registered panels gain two tabs");
        Check(ptr_tab_container->Children()[1]->bounds_widget.int4_x ==
                      ptr_tab_container->Children()[0]->bounds_widget
                          .int4_x +
                          10 &&
                  ptr_tab_container->Children()[1]->bounds_widget
                          .int4_y ==
                      ptr_tab_container->Children()[0]->bounds_widget
                          .int4_y +
                      5,
              "p6b23: the second tab strides from the first");
        Check(ptr_panel_container->Get("GAMEPLAY_PANEL")
                      ->fn_is_visible() &&
                  !ptr_panel_container->Get("AUDIO_PANEL")
                       ->fn_is_visible(),
              "p6b23: the first registered panel starts active");

        auto* ptr_tab_one = static_cast<ui::ButtonWidget*>(
            ptr_tab_container->Children()[0]);
        auto* ptr_tab_two = static_cast<ui::ButtonWidget*>(
            ptr_tab_container->Children()[1]);
        ptr_tab_two->fn_on_click();
        Check(b_leave_one_tripped && int4_settings_saves23 == 1,
              "p6b23: switching tabs runs the leaving panel and "
              "saves");
        Check(!ptr_panel_container->Get("GAMEPLAY_PANEL")
                       ->fn_is_visible() &&
                  ptr_panel_container->Get("AUDIO_PANEL")
                      ->fn_is_visible() &&
                  ptr_tab_two->fn_is_highlighted(),
              "p6b23: the clicked tab becomes the active panel");

        ptr_reset_button->fn_on_click();
        Check(int4_reset_one_runs == 0,
              "p6b23: reset defers its panel action until "
              "confirmed");
        Check(ptr_stub23->vec_loaded_ids.back() == "TWOBUTTON_PROMPT",
              "p6b23: reset opens the confirmation prompt");
        ptr_tab_one->fn_on_click();
        int4_settings_saves23 = 0;
        ptr_back_button->fn_on_click();
        Check(b_leave_one_tripped && int4_settings_saves23 == 1 &&
                  int4_on_exit23 == 1,
              "p6b23: back without a pending restart closes and "
              "exits");

        ptr_tab_two->fn_on_click();
        b_leave_two_demands_restart = true;
        ptr_tab_one->fn_on_click();
        ptr_stub23->vec_loaded_ids.clear();
        ptr_back_button->fn_on_click();
        Check(int4_on_exit23 == 1 &&
                  ptr_stub23->vec_loaded_ids.back() ==
                      "TWOBUTTON_PROMPT",
              "p6b23: back with a pending restart opens the prompt "
              "instead of exiting");

        logic_settings.reset();
        ora::ui::Ui::Instance().CloseWindow();
        ora::ui::Ui::Instance().CloseWindow();
        delete ptr_settings_root;
      }

      {
        auto up_panel = std::unique_ptr<ui::Widget>(
            ptr_stub23->MakePanelTree("ADVANCED_PANEL"));
        ui::Widget* ptr_panel = up_panel.get();
        SettingsHostStub23 host_advanced;
        mods::AdvancedSettingsLogic logic_advanced(
            mod_data, host_advanced, "ADVANCED_PANEL", "Advanced");

        game::SettingsModule& module_server =
            *game::Settings::Instance().Module("Server");
        const std::size_t sz_slot_nat =
            module_server.SlotIndexOf("DiscoverNatDevices").value();
        module_server.SetBool(sz_slot_nat, false);
        game::SettingsModule& module_debug =
            *game::Settings::Instance().Module("Debug");
        const std::size_t sz_slot_developer =
            module_debug.SlotIndexOf("DisplayDeveloperSettings").value();

        std::function<bool()> fn_leave_advanced;
        std::function<void()> fn_reset_advanced;
        host_advanced.CaptureForTest(*ptr_panel, fn_leave_advanced,
                                     fn_reset_advanced);

        Check(!fn_leave_advanced(),
              "p6b23: advanced reports no restart while NAT matches "
              "the snapshot");
        ptr_panel->Get<ui::CheckboxWidget>("NAT_DISCOVERY")->fn_on_click();
        Check(module_server.GetBool(sz_slot_nat),
              "p6b23: the NAT checkbox flips the server preference");
        Check(fn_leave_advanced(),
              "p6b23: advanced demands a restart once NAT diverges "
              "from the snapshot");

        module_debug.SetBool(sz_slot_developer, true);
        Check(ptr_panel->Get("BOTDEBUG_CHECKBOX_CONTAINER")
                      ->fn_is_visible() &&
                  !ptr_panel->Get("DEBUG_HIDDEN_CONTAINER")
                       ->fn_is_visible(),
              "p6b23: developer containers surface the developer "
              "settings");
        module_debug.SetBool(sz_slot_developer, false);

        module_server.SetBool(sz_slot_nat, false);

        game::SettingsModule& module_game =
            *game::Settings::Instance().Module("Game");
        const std::size_t sz_slot_discord =
            module_game.SlotIndexOf("EnableDiscordService").value();
        const std::size_t sz_slot_fetch_news =
            module_game.SlotIndexOf("FetchNews").value();
        module_game.SetBool(sz_slot_discord, false);
        module_game.SetBool(sz_slot_fetch_news, false);
        Check(ptr_panel->Get<ui::CheckboxWidget>("SENDSYSINFO_CHECKBOX")
                  ->fn_is_disabled(),
              "p6b23: send-sysinfo disables without fetch-news");

        fn_reset_advanced();
        Check(module_game.GetBool(sz_slot_discord),
              "p6b23: the advanced reset restores the defaults");
      }

      {
        auto up_panel = std::unique_ptr<ui::Widget>(
            ptr_stub23->MakePanelTree("AUDIO_PANEL"));
        ui::Widget* ptr_panel = up_panel.get();
        SettingsHostStub23 host_audio;
        mods::AudioSettingsLogic logic_audio(
            mod_data, host_audio, "AUDIO_PANEL", "Audio", nullptr);

        std::function<bool()> fn_leave_audio;
        std::function<void()> fn_reset_audio;
        host_audio.CaptureForTest(*ptr_panel, fn_leave_audio,
                                  fn_reset_audio);

        game::SettingsModule& module_sound =
            *game::Settings::Instance().Module("Sound");
        const std::size_t sz_slot_cash =
            module_sound.SlotIndexOf("CashTicks").value();
        module_sound.SetBool(sz_slot_cash, true);
        ptr_panel->Get<ui::CheckboxWidget>("CASH_TICKS")->fn_on_click();
        Check(!module_sound.GetBool(sz_slot_cash),
              "p6b23: the cash-ticks checkbox toggles the sound "
              "preference");

        ui::SliderWidget* ptr_volume_slider =
            ptr_panel->Get<ui::SliderWidget>("SOUND_VOLUME");
        ptr_volume_slider->fn_on_change(0.75f);
        Check(module_sound.GetFloat(
                  module_sound.SlotIndexOf("SoundVolume").value()) ==
                  0.75f,
              "p6b23: the volume slider writes the sound preference");
        ptr_volume_slider->fn_on_change(0.5f);

        Check(!fn_leave_audio(),
              "p6b23: audio reports no restart while the device "
              "matches");
        fn_reset_audio();
        Check(module_sound.GetBool(sz_slot_cash) &&
                  module_sound.GetFloat(
                      module_sound.SlotIndexOf("SoundVolume").value()) ==
                      0.5f,
              "p6b23: the audio reset restores the defaults");
      }

      {
        auto up_panel = std::unique_ptr<ui::Widget>(
            ptr_stub23->MakePanelTree("DISPLAY_PANEL"));
        ui::Widget* ptr_panel = up_panel.get();
        SettingsHostStub23 host_display;
        mods::DisplaySettingsLogic logic_display(
            mod_data, host_display, "DISPLAY_PANEL", "Display", nullptr);

        std::function<bool()> fn_leave_display;
        std::function<void()> fn_reset_display;
        host_display.CaptureForTest(*ptr_panel, fn_leave_display,
                                    fn_reset_display);

        game::SettingsModule& module_graphics =
            *game::Settings::Instance().Module("Graphics");
        const std::size_t sz_slot_mode =
            module_graphics.SlotIndexOf("Mode").value();
        const std::size_t sz_slot_windowed =
            module_graphics.SlotIndexOf("WindowedSize").value();
        module_graphics.SetInt(sz_slot_mode, 2);
        module_graphics.SetInt2(sz_slot_windowed, int2{1024, 768});

        Check(!fn_leave_display(),
              "p6b23: display reports no restart while graphics match "
              "the snapshot");
        ptr_panel->Get<ui::CheckboxWidget>("VSYNC_CHECKBOX")->fn_on_click();
        Check(!module_graphics.GetBool(
                  module_graphics.SlotIndexOf("VSync").value()),
              "p6b23: the vsync checkbox toggles the graphics "
              "preference");
        module_graphics.SetBool(
            module_graphics.SlotIndexOf("VSync").value(), true);

        ui::TextFieldWidget* ptr_width_field =
            ptr_panel->Get<ui::TextFieldWidget>("WINDOW_WIDTH");
        ui::TextFieldWidget* ptr_height_field =
            ptr_panel->Get<ui::TextFieldWidget>("WINDOW_HEIGHT");
        ptr_width_field->SetText("1280");
        ptr_height_field->SetText("720");
        Check(ptr_panel->Get<ui::DropDownButtonWidget>(
                  "RESOLUTION_PRESET_DROPDOWN")
                      ->fn_get_text() == "1280x720",
              "p6b23: a common width/height pair names its preset");
        ptr_width_field->SetText("1024");
        ptr_height_field->SetText("768");

        module_graphics.SetInt(sz_slot_mode, 0);
        Check(fn_leave_display(),
              "p6b23: display demands a restart once the mode diverges");
        module_graphics.SetInt(sz_slot_mode, 2);

        fn_reset_display();
        Check(module_graphics.GetInt(
                      module_graphics.SlotIndexOf("MaxFramerate")
                          .value()) == 60 &&
                  module_graphics.GetInt2(sz_slot_windowed).X == 1024,
              "p6b23: the display reset restores the defaults");
      }

      {
        auto up_panel = std::unique_ptr<ui::Widget>(
            ptr_stub23->MakePanelTree("INPUT_PANEL"));
        ui::Widget* ptr_panel = up_panel.get();
        SettingsHostStub23 host_input;
        mods::InputSettingsLogic logic_input(
            mod_data, host_input, "INPUT_PANEL", "Input");

        std::function<bool()> fn_leave_input;
        std::function<void()> fn_reset_input;
        host_input.CaptureForTest(*ptr_panel, fn_leave_input,
                                  fn_reset_input);

        game::SettingsModule& module_game =
            *game::Settings::Instance().Module("Game");
        const std::size_t sz_slot_edge =
            module_game.SlotIndexOf("ViewportEdgeScroll").value();
        const std::size_t sz_slot_style =
            module_game.SlotIndexOf("MouseControlStyle").value();
        module_game.SetBool(sz_slot_edge, true);
        module_game.SetInt(sz_slot_style, 0);
        ptr_panel->Get<ui::CheckboxWidget>("EDGESCROLL_CHECKBOX")
            ->fn_on_click();
        Check(!module_game.GetBool(sz_slot_edge),
              "p6b23: the edge-scroll checkbox toggles the game "
              "preference");

        ui::Widget* ptr_desc_classic =
            ptr_panel->Get("MOUSE_CONTROL_DESC_CLASSIC");
        Check(ptr_desc_classic->fn_is_visible() &&
                  ptr_desc_classic->Get("DESC_SCROLL_RIGHT")
                      ->fn_is_visible() &&
                  !ptr_desc_classic->Get("DESC_SCROLL_MIDDLE")
                       ->fn_is_visible(),
              "p6b23: the classic scheme shows the right-button "
              "scroll");

        ptr_panel->Get<ui::SliderWidget>("SCROLLSPEED_SLIDER")
            ->fn_on_change(40.0f);
        Check(module_game.GetFloat(
                  module_game.SlotIndexOf("ViewportEdgeScrollStep")
                      .value()) == 40.0f,
              "p6b23: the scroll-speed slider writes the game "
              "preference");

        fn_reset_input();
        Check(module_game.GetInt(sz_slot_style) == 1 &&
                  module_game.GetFloat(
                      module_game.SlotIndexOf("ViewportEdgeScrollStep")
                          .value()) == 30.0f,
              "p6b23: the input reset restores the defaults");
      }

      {
        auto up_panel = std::unique_ptr<ui::Widget>(
            ptr_stub23->MakePanelTree("GAMEPLAY_PANEL"));
        ui::Widget* ptr_panel = up_panel.get();
        SettingsHostStub23 host_gameplay;
        mods::GameplaySettingsLogic logic_gameplay(
            mod_data, host_gameplay, "GAMEPLAY_PANEL", "Gameplay",
            nullptr);

        std::function<bool()> fn_leave_gameplay;
        std::function<void()> fn_reset_gameplay;
        host_gameplay.CaptureForTest(*ptr_panel, fn_leave_gameplay,
                                     fn_reset_gameplay);

        game::SettingsModule& module_player =
            *game::Settings::Instance().Module("Player");
        const std::size_t sz_slot_name =
            module_player.SlotIndexOf("Name").value();
        module_player.SetString(sz_slot_name, "Commander");

        ui::TextFieldWidget* ptr_name_field =
            ptr_panel->Get<ui::TextFieldWidget>("PLAYERNAME");
        Check(ptr_name_field->fn_is_disabled(),
              "p6b23: the name field stays disabled outside the "
              "shellmap");
        ptr_name_field->SetText("  Napoleon  ");
        ptr_name_field->fn_on_lose_focus();
        Check(module_player.GetString(sz_slot_name) == "Napoleon",
              "p6b23: losing focus sanitizes and stores the name");

        ptr_name_field->SetText("   ");
        ptr_name_field->fn_on_lose_focus();
        Check(module_player.GetString(sz_slot_name) == "Napoleon" &&
                  ptr_name_field->Text() == "Napoleon",
              "p6b23: an emptied field falls back to the stored name");

        game::SettingsModule& module_autosave =
            *game::Settings::Instance().Module("AutoSave");
        const std::size_t sz_slot_interval =
            module_autosave.SlotIndexOf("AutoSaveInterval").value();
        module_autosave.SetInt(sz_slot_interval, 30);
        Check(ptr_panel->Get<ui::DropDownButtonWidget>(
                      "AUTO_SAVE_INTERVAL_DROP_DOWN")
                      ->fn_get_text() ==
                  "auto-save-interval.options",
              "p6b23: the interval dropdown names the seconds");
        module_autosave.SetInt(sz_slot_interval, 120);
        Check(ptr_panel->Get<ui::DropDownButtonWidget>(
                      "AUTO_SAVE_INTERVAL_DROP_DOWN")
                      ->fn_get_text() ==
                  "auto-save-interval.minute-options",
              "p6b23: the interval dropdown names the minutes");
        module_autosave.SetInt(sz_slot_interval, 0);
        Check(ptr_panel->Get<ui::DropDownButtonWidget>(
                      "AUTO_SAVE_INTERVAL_DROP_DOWN")
                      ->fn_get_text() == "auto-save-interval.disabled",
              "p6b23: a zero interval names the disabled option");
        Check(ptr_panel->Get<ui::DropDownButtonWidget>(
                      "AUTO_SAVE_FILE_NUMBER_DROP_DOWN")
                      ->fn_is_disabled(),
              "p6b23: the file-count dropdown disables on interval "
              "zero");

        fn_reset_gameplay();
        Check(module_player.GetString(sz_slot_name) == "Commander" &&
                  ptr_name_field->Text() == "Commander",
              "p6b23: the gameplay reset restores the default name");
      }

      {
        yaml::StringPool pool_hotkeys23;
        yaml::MiniYaml yaml_groups23{};
        yaml::MiniYamlNode node_game_group{};
        node_game_group.Key = pool_hotkeys23.Intern(
            "hotkey-group-game-commands");
        node_game_group.Value.Nodes.emplace_back(
            pool_hotkeys23.Intern("Types"),
            yaml::MiniYaml{
                pool_hotkeys23.Intern("OrderGenerator, World, Menu")});
        yaml::MiniYamlNode node_production_group{};
        node_production_group.Key = pool_hotkeys23.Intern(
            "hotkey-group-production-commands");
        node_production_group.Value.Nodes.emplace_back(
            pool_hotkeys23.Intern("Types"),
            yaml::MiniYaml{
                pool_hotkeys23.Intern("Production, ProductionSlot")});
        yaml_groups23.Nodes.push_back(std::move(node_game_group));
        yaml_groups23.Nodes.push_back(
            std::move(node_production_group));
        const ui::YamlDictionary vec_args_hotkeys23{
            {std::string_view{"HotkeyGroups"}, &yaml_groups23}};

        auto up_panel = std::unique_ptr<ui::Widget>(
            ptr_stub23->MakePanelTree("HOTKEYS_PANEL"));
        ui::Widget* ptr_panel = up_panel.get();
        SettingsHostStub23 host_hotkeys;
        mods::HotkeysSettingsLogic logic_hotkeys(
            mod_data, host_hotkeys, "HOTKEYS_PANEL", "Hotkeys",
            &vec_args_hotkeys23);

        std::function<bool()> fn_leave_hotkeys;
        std::function<void()> fn_reset_hotkeys;
        host_hotkeys.CaptureForTest(*ptr_panel, fn_leave_hotkeys,
                                    fn_reset_hotkeys);

        ui::ScrollPanelWidget* ptr_hotkey_list =
            ptr_panel->Get<ui::ScrollPanelWidget>("HOTKEY_LIST");
        Check(ptr_hotkey_list->Children().size() > 2,
              "p6b23: the hotkey list assembles its groups");

        const std::string* ptr_first_key = nullptr;
        for (const auto& [str_key, definition] :
             mod_data.Hotkeys().Definitions())
          if (std::find(definition.vec_types.begin(),
                        definition.vec_types.end(), "Production") !=
              definition.vec_types.end()) {
            ptr_first_key = &str_key;
            break;
          }
        Check(ptr_first_key != nullptr,
              "p6b23: the real ra hotkey table carries production "
              "keys");
        if (ptr_first_key != nullptr)
          Check(ptr_hotkey_list->GetOrNull(*ptr_first_key) != nullptr &&
                    ptr_hotkey_list->GetOrNull(*ptr_first_key)
                            ->GetOrNull<ui::ButtonWidget>("HOTKEY") !=
                        nullptr,
                "p6b23: the production hotkey gains a remap row");

        Check(ptr_panel->Get<ui::DropDownButtonWidget>(
                      "CONTEXT_DROPDOWN")
                      ->fn_get_text()
                      .find("any") != std::string::npos,
              "p6b23: the context dropdown opens on any-context");

        fn_reset_hotkeys();
        bool b_all_default23 = true;
        for (const auto& [str_key, definition] :
             mod_data.Hotkeys().Definitions()) {
          const ui::Hotkey hk_current =
              mod_data.Hotkeys().Get(str_key).GetValue();
          const bool b_matches_default =
              hk_current == definition.hk_default ||
              (!hk_current.IsValid() &&
               !definition.hk_default.IsValid());
          if (!b_matches_default)
            b_all_default23 = false;
        }
        Check(b_all_default23,
              "p6b23: the hotkey reset restores every default");
      }
    }

  }


    std::println("render_test: {} failures", g_failures);


  return g_failures != 0;
}

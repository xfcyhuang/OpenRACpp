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
#include "mods/place_building.hpp"
#include "mods/primary_building.hpp"
#include "mods/player_resources.hpp"
#include "mods/production_icon_overlay.hpp"
#include "mods/production_palette_widget.hpp"
#include "mods/production_tabs_widget.hpp"
#include "mods/production_type_button_widget.hpp"
#include "mods/world_interaction_controller.hpp"
#include "mods/radar.hpp"
#include "mods/radar_widget.hpp"
#include "mods/support_powers.hpp"
#include "mods/support_powers_widget.hpp"
#include "mods/player_statistics.hpp"
#include "mods/tooltip.hpp"
#include "net/order.hpp"
#include "net/order_io.hpp"
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
    ++g_failures;
  }
}

void Check(bool ok, std::string_view what) {
  if (!ok) {
    std::println("FAIL: {}", what);
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
    mods::SupportPowerManager* spm =
        player_a->PlayerActor()->TraitOrDefault<mods::SupportPowerManager>();
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
      delete vec_orders[0];
      Check(world->OrderGenerator() != selector_ptr,
            "p6b11: OrderInner cancels the input mode");
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

  std::println("render_test: {} failures", g_failures);


  return g_failures != 0;
}

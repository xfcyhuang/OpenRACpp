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
#include "game/manifest.hpp"
#include "game/mod_data.hpp"
#include "game/settings.hpp"
#include "render_sequences_fixture.hpp"
#include "gfx/palette.hpp"
#include "gfx/renderable.hpp"
#include "gfx/sequence_set.hpp"
#include "gfx/sprite_loader.hpp"
#include "gfx/world_renderer.hpp"
#include "map/map.hpp"
#include "map/map_cache.hpp"
#include "meta/field_saver.hpp"
#include "meta/field_loader.hpp"
#include "mods/body_orientation.hpp"
#include "mods/create_map_players.hpp"
#include "mods/mobile.hpp"
#include "mods/proximity_capturable.hpp"
#include "mods/render_sprites.hpp"
#include "mods/sequence_loader_factory.hpp"
#include "mods/turreted.hpp"
#include "mods/with_infantry_body.hpp"
#include "mods/with_make_animation.hpp"
#include "mods/with_sprite_body.hpp"
#include "mods/with_sprite_turret.hpp"
#include "net/order.hpp"
#include "net/order_io.hpp"
#include "net/replay_recorder.hpp"
#include "net/sync_report.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/player.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/world.hpp"

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
  cache.LoadMaps(mod_data);
  std::string str_uid;
  for (map::MapPreview* p : cache.Previews())
    if (p->Status() == map::MapStatus::Available) {
      str_uid = p->Uid();
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
    recorder.Dispose();

    // 读回:包序 client/length/data 逐字节一致
    // The read-back: the client/length/data order byte-for-byte.
    net::ReplayConnection connection{recorder.Bytes()};
    int client = 0;
    std::vector<std::uint8_t> vec_data;
    Check(connection.TryReadNext(client, vec_data) && client == 7 &&
              vec_data == vec_pre,
          "packet 1 = the pre-start payload");
    Check(connection.TryReadNext(client, vec_data) && client == 1 &&
              vec_data == vec_start,
          "packet 2 = the game-start payload");
    Check(connection.TryReadNext(client, vec_data) && client == 1,
          "packet 3 client");
    Check(vec_data.size() == 4 + vec_post.size() &&
              std::equal(vec_post.begin(), vec_post.end(),
                         vec_data.begin() + 4),
          "packet 3 = frame(3) + payload");
    Check(!connection.TryReadNext(client, vec_data), "stream drained");
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

  std::println("render_test: {} failures", g_failures);
  return g_failures != 0;
}

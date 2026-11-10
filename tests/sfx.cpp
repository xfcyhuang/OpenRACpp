
import std;


import ora.game;
import ora.game.combat;
import ora.game.miniyaml;
import ora.game.pathfinding;
import ora.game.sound;
import ora.game.support;
import ora.game.world;
import ora.platforms;
import ora.test.util;
namespace {

using ora::game::make_dummy_facade;
using ora::game::sound_facade;
using ora::game::sound_play;
using ora::game::sound_play_to_player;
using ora::game::sound_type;
using ora::test::check;

constexpr std::uint32_t kLocalPlayer{2};
constexpr std::uint32_t kPlayerActorId{2};
constexpr std::int32_t kSeed{42};

ora::platforms::wpos wpos_at(std::int32_t int4_x, std::int32_t int4_y) {
  return ora::platforms::wpos{.int4_x = int4_x, .int4_y = int4_y, .int4_z = 0};
}

std::size_t count_lines(const sound_facade& facade, std::string_view str_prefix) {
  std::size_t sz_count{0};
  for (const std::string& str_line : facade.vec_log)
    if (str_line.starts_with(str_prefix)) ++sz_count;
  return sz_count;
}

void test_play_to_player_ownership() {
  sound_facade facade{make_dummy_facade()};
  facade.u4_local_player = kLocalPlayer;

  const auto snd_center{sound_play_to_player(facade, sound_type::world, kLocalPlayer,
                                             "placedown1")};
  check(snd_center.has_value(), "playtoplayer no-pos returns sound");
  check(facade.vec_log[0] == "  play2d name=placedown1 pos=0,0,0 loop=0 rel=1 vol=0.5 atten=1",
        std::format("no-pos play line: {}", facade.vec_log[0]));

  facade.vec_log.clear();
  const auto snd_own{sound_play_to_player(facade, sound_type::world, kLocalPlayer, "placedown1",
                                          wpos_at(4096, 2048))};
  check(snd_own.has_value(), "playtoplayer own actor returns sound");
  check(count_lines(facade, "  play2d ") == 1u, "own play logged once");
  check(facade.vec_log[0] ==
            "  play2d name=placedown1 pos=4096,2048,0 loop=0 rel=0 vol=0.5 atten=1",
        std::format("own play line: {}", facade.vec_log[0]));

  facade.vec_log.clear();
  const auto snd_foe{sound_play_to_player(facade, sound_type::world, kLocalPlayer + 1u,
                                          "placedown1", wpos_at(0, 0))};
  check(!snd_foe.has_value(), "playtoplayer foe blocked");
  check(facade.vec_log.empty(), "foe play not logged");

  facade.vec_log.clear();
  facade.u4_local_player = ora::game::kSfxNoPlayer;
  const auto snd_spec{sound_play_to_player(facade, sound_type::world, kLocalPlayer, "placedown1",
                                           wpos_at(0, 0))};
  check(!snd_spec.has_value(), "spectator playtoplayer blocked");

  facade.u4_local_player = kLocalPlayer;
  facade.vec_log.clear();
  const auto snd_world{sound_play(facade, sound_type::world, "explosion1", wpos_at(1024, 2048))};
  check(snd_world.has_value(), "world play returns sound");
  check(facade.vec_log.size() == 1u,
        std::format("world play lines: {}", facade.vec_log.size()));
  check(facade.vec_log[0] ==
            "  play2d name=explosion1 pos=1024,2048,0 loop=0 rel=0 vol=0.5 atten=1",
        std::format("world play line: {}", facade.vec_log[0]));
}

void test_play_gates() {
  sound_facade facade{make_dummy_facade()};
  facade.u4_local_player = kLocalPlayer;

  check(!sound_play(facade, sound_type::world, "", wpos_at(0, 0)).has_value(),
        "empty name blocked");
  check(facade.vec_log.empty(), "empty name not logged");

  facade.b_disable_all_sounds = true;
  check(!sound_play(facade, sound_type::world, "click1", wpos_at(0, 0)).has_value(),
        "disable all blocked");
  facade.b_disable_all_sounds = false;

  facade.b_disable_world_sounds = true;
  check(!sound_play(facade, sound_type::world, "click1", wpos_at(0, 0)).has_value(),
        "disable world blocked");
  check(sound_play(facade, sound_type::ui, "click1", wpos_at(0, 0)).has_value(),
        "disable world keeps ui");
}

void test_play_volume_chain() {
  sound_facade facade{make_dummy_facade()};
  facade.u4_local_player = kLocalPlayer;
  facade.fp_sound_volume_modifier = 0.8f;
  const auto snd{sound_play(facade, sound_type::world, "click1", wpos_at(0, 0), 0.5f)};
  check(snd.has_value(), "volume chain play returns sound");
  check(facade.vec_log.size() == 1u && facade.vec_log[0] ==
                                           "  play2d name=click1 pos=0,0,0 loop=0 rel=0 vol=0.2 atten=1",
        std::format("volume chain line: {}", facade.vec_log.empty() ? "-" : facade.vec_log[0]));
}


using ora::game::sound_play_notification;
using ora::game::sound_play_predefined;
using ora::game::sfx_sound_rules;
using ora::game::sfx_voiced;
using ora::game::sound_info;
using ora::game::support::MersenneTwister;

constexpr std::uint32_t kSfxNoPlayerRef = ora::game::kSfxNoPlayer;

std::string_view str_voices_yaml() {
  return R"yaml(GenericVoice:
	Voices:
		Select: await1,ready,report1
		Action: ackno,affirm1
		Die: dedman1,dedman2
	Variants:
		allies: .v01,.v03
	DisableVariants: Die
)yaml";
}

std::string_view str_notifications_yaml() {
  return R"yaml(Speech:
	Notifications:
		UnitReady: unready1
		RadarUp: radaron2
		OverlapLine: overlap1
			VolumeModifier: 0.33
			InterruptType: Overlap
		ClickLine: rabeep1
			InterruptType: Interrupt
		Vacant:
		TwoClip: two1,two2
Sounds:
	Notifications:
		CashTickUp: cashup1
			VolumeModifier: 0.5
			InterruptType: Interrupt
)yaml";
}

sfx_sound_rules load_rules() {
  sfx_sound_rules rules{};
  const auto res_voices{ora::game::miniyaml_from_string(str_voices_yaml(), "sfx-voices")};
  check(res_voices.has_value(), "voices yaml parses");
  for (const auto& node : *res_voices) {
    const auto res_info{ora::game::sound_info_from_yaml(node)};
    check(res_info.has_value(), "voice sound_info parses");
    rules.map_voices.insert_or_assign(ora::game::string_to_lower_ascii(node.str_key), *res_info);
  }
  const auto res_notif{ora::game::miniyaml_from_string(str_notifications_yaml(), "sfx-notif")};
  check(res_notif.has_value(), "notifications yaml parses");
  for (const auto& node : *res_notif) {
    const auto res_info{ora::game::sound_info_from_yaml(node)};
    check(res_info.has_value(), "notification sound_info parses");
    rules.map_notifications.insert_or_assign(ora::game::string_to_lower_ascii(node.str_key),
                                             *res_info);
  }
  return rules;
}

void test_play_predefined_notifications() {
  sound_facade facade{make_dummy_facade()};
  facade.u4_local_player = kLocalPlayer;
  sfx_sound_rules rules{load_rules()};
  MersenneTwister rng_cosmetic{ora::game::support::make_mersenne_twister(20261107)};

  const auto res1{sound_play_predefined(facade, rules, rng_cosmetic, sound_type::world,
                                        kSfxNoPlayerRef, std::nullopt, "speech", "RadarUp",
                                        nullptr, true, wpos_at(0, 0), 1.0f, false)};
  check(res1.has_value() && *res1, "radarup play returns true");
  check(facade.vec_log.size() == 1u, "radarup one play2d");
  check(facade.vec_log[0].starts_with("  play2d name=radaron2") &&
            facade.vec_log[0].ends_with("pos=0,0,0 loop=0 rel=1 vol=0.5 atten=0"),
        std::format("radarup line: {}", facade.vec_log[0]));

  facade.vec_log.clear();
  const auto res2{sound_play_predefined(facade, rules, rng_cosmetic, sound_type::world,
                                        kSfxNoPlayerRef, std::nullopt, "speech", "UnitReady",
                                        nullptr, true, wpos_at(0, 0), 1.0f, false)};
  check(res1.has_value() && *res2, "unitready first plays");
  const std::size_t sz_first{facade.vec_log.size()};
  const auto res3{sound_play_predefined(facade, rules, rng_cosmetic, sound_type::world,
                                        kSfxNoPlayerRef, std::nullopt, "speech", "UnitReady",
                                        nullptr, true, wpos_at(0, 0), 1.0f, false)};
  check(res3.has_value() && !*res3, "unitready second blocked (DoNotPlay)");
  check(facade.vec_log.size() == sz_first, "DoNotPlay added no play2d");

  facade.vec_log.clear();
  const auto res4{sound_play_predefined(facade, rules, rng_cosmetic, sound_type::world,
                                        kSfxNoPlayerRef, std::nullopt, "speech", "OverlapLine",
                                        nullptr, true, wpos_at(0, 0), 1.0f, false)};
  const auto res5{sound_play_predefined(facade, rules, rng_cosmetic, sound_type::world,
                                        kSfxNoPlayerRef, std::nullopt, "speech", "OverlapLine",
                                        nullptr, true, wpos_at(0, 0), 1.0f, false)};
  check(res4.has_value() && *res4 && res5.has_value() && *res5, "overlap plays twice");
  check(facade.vec_log.size() == 2u, "overlap two play2d");
  check(facade.vec_log[1].ends_with("vol=0.165 atten=0"),
        std::format("overlap vol line: {}", facade.vec_log[1]));

  facade.vec_log.clear();
  const auto res6{sound_play_predefined(facade, rules, rng_cosmetic, sound_type::world,
                                        kSfxNoPlayerRef, std::nullopt, "speech", "ClickLine",
                                        nullptr, true, wpos_at(0, 0), 1.0f, false)};
  const auto res7{sound_play_predefined(facade, rules, rng_cosmetic, sound_type::world,
                                        kSfxNoPlayerRef, std::nullopt, "speech", "ClickLine",
                                        nullptr, true, wpos_at(0, 0), 1.0f, false)};
  check(res6.has_value() && *res6 && res7.has_value() && *res7, "interrupt plays twice");
  check(facade.vec_log.size() == 3u && facade.vec_log[1] == "  stopsound",
        std::format("interrupt stopsound: {} / {}", facade.vec_log.size(),
                    facade.vec_log.size() > 1 ? facade.vec_log[1] : "-"));

  facade.vec_log.clear();
  const std::int32_t int4_rng_before{rng_cosmetic.int4_total_count};
  const auto res8{sound_play_predefined(facade, rules, rng_cosmetic, sound_type::world,
                                        kSfxNoPlayerRef, std::nullopt, "speech", "Vacant",
                                        nullptr, true, wpos_at(0, 0), 1.0f, false)};
  check(res8.has_value() && !*res8, "vacant returns false");
  check(facade.vec_log.empty(), "vacant no play2d");
  check(rng_cosmetic.int4_total_count == int4_rng_before, "vacant no rng draw");

  facade.vec_log.clear();
  const std::int32_t int4_rng_before_foe{rng_cosmetic.int4_total_count};
  const auto res9{sound_play_predefined(facade, rules, rng_cosmetic, sound_type::world,
                                        kLocalPlayer + 7u, std::nullopt, "speech", "TwoClip",
                                        nullptr, true, wpos_at(0, 0), 1.0f, false)};
  check(res9.has_value() && *res9, "foe notification returns true");
  check(facade.vec_log.empty(), "foe notification silent");
  check(rng_cosmetic.int4_total_count == int4_rng_before_foe + 1,
        std::format("foe consumed pool draw: {} -> {}", int4_rng_before_foe,
                    rng_cosmetic.int4_total_count));

  facade.vec_log.clear();
  const auto res10{sound_play_predefined(facade, rules, rng_cosmetic, sound_type::world,
                                         kSfxNoPlayerRef, std::nullopt, "speech", "NoSuchDef",
                                         nullptr, true, wpos_at(0, 0), 1.0f, false)};
  check(!res10.has_value(), "missing definition errors");
  const auto res11{sound_play_predefined(facade, rules, rng_cosmetic, sound_type::world,
                                         kSfxNoPlayerRef, std::nullopt, "NoType", "RadarUp",
                                         nullptr, true, wpos_at(0, 0), 1.0f, false)};
  check(!res11.has_value() &&
            res11.error() == "The given key 'NoType' was not present in the dictionary.",
        "missing type errors (upstream dictionary indexer throw)");
}

void test_play_predefined_voices() {
  sound_facade facade{make_dummy_facade()};
  facade.u4_local_player = kLocalPlayer;
  sfx_sound_rules rules{load_rules()};
  MersenneTwister rng_cosmetic{ora::game::support::make_mersenne_twister(20261107)};

  const std::string str_variant_allies{"allies"};

  const sfx_voiced voiced5{.u4_actor_id = 5, .b_selected = false};
  const auto res1{sound_play_predefined(facade, rules, rng_cosmetic, sound_type::world,
                                        kSfxNoPlayerRef, voiced5, "genericvoice", "Select",
                                        &str_variant_allies, false, wpos_at(2048, 4096), 1.0f,
                                        true)};
  check(res1.has_value() && *res1, "voice play returns true");
  check(facade.vec_log.size() == 1u &&
            (facade.vec_log[0].contains("await1.v03") || facade.vec_log[0].contains("ready.v03") ||
             facade.vec_log[0].contains("report1.v03")),
        std::format("voice variant line: {}", facade.vec_log[0]));

  facade.vec_log.clear();
  const sfx_voiced voiced7{.u4_actor_id = 7, .b_selected = true};
  const sfx_voiced voiced9{.u4_actor_id = 9, .b_selected = true};
  const auto res2{sound_play_predefined(facade, rules, rng_cosmetic, sound_type::world,
                                        kSfxNoPlayerRef, voiced7, "genericvoice", "Action",
                                        &str_variant_allies, false, wpos_at(0, 0), 1.0f, true)};
  const auto res3{sound_play_predefined(facade, rules, rng_cosmetic, sound_type::world,
                                        kSfxNoPlayerRef, voiced9, "genericvoice", "Action",
                                        &str_variant_allies, false, wpos_at(0, 0), 1.0f, true)};
  check(res2.has_value() && *res2, "selected voice 1 plays");
  check(res3.has_value() && !*res3, "selected voice 2 blocked via shared slot");
  check(facade.vec_log.size() == 1u, "selected shared slot one play2d");

  facade.vec_log.clear();
  const sfx_voiced voiced11{.u4_actor_id = 11, .b_selected = false};
  const sfx_voiced voiced12{.u4_actor_id = 12, .b_selected = false};
  const auto res4{sound_play_predefined(facade, rules, rng_cosmetic, sound_type::world,
                                        kSfxNoPlayerRef, voiced11, "genericvoice", "Action",
                                        &str_variant_allies, false, wpos_at(0, 0), 1.0f, true)};
  const auto res5{sound_play_predefined(facade, rules, rng_cosmetic, sound_type::world,
                                        kSfxNoPlayerRef, voiced12, "genericvoice", "Action",
                                        &str_variant_allies, false, wpos_at(0, 0), 1.0f, true)};
  check(res4.has_value() && *res4 && res5.has_value() && *res5,
        "unselected distinct slots both play");
  check(facade.vec_log.size() == 2u, "distinct slots two play2d");

  const std::string str_variant_none{"none"};
  facade.vec_log.clear();
  const auto res6{sound_play_predefined(facade, rules, rng_cosmetic, sound_type::world,
                                        kSfxNoPlayerRef, voiced5, "genericvoice", "Die",
                                        &str_variant_allies, false, wpos_at(0, 0), 1.0f, true)};
  check(res6.has_value(), "die definition handled");
  if (!facade.vec_log.empty())
    check(facade.vec_log[0].contains(".aud"),
          std::format("disable variants keeps default suffix: {}", facade.vec_log[0]));
}

void test_play_notification_lowering() {
  sound_facade facade{make_dummy_facade()};
  facade.u4_local_player = kLocalPlayer;
  sfx_sound_rules rules{load_rules()};
  MersenneTwister rng_cosmetic{ora::game::support::make_mersenne_twister(20261107)};

  const auto res{sound_play_notification(facade, rules, rng_cosmetic, kLocalPlayer, "Speech",
                                         "RadarUp", nullptr)};
  check(res.has_value() && *res, "notification returns true");
  check(facade.vec_log.size() == 1u && facade.vec_log[0].ends_with("rel=1 vol=0.5 atten=0"),
        std::format("notification line: {}", facade.vec_log[0]));
}


using ora::game::sfx_context;
using ora::game::sfx_on_building_placed;
using ora::game::sfx_on_production_blocked;
using ora::game::sfx_on_production_ready;
using ora::game::sfx_on_selection;
using ora::game::sfx_on_warhead_impact;
using ora::game::sfx_on_weapon_fired;
using ora::game::sfx_order_voice;
using ora::game::sfx_play_voice_for_orders;
using ora::game::sfx_select_candidate;

struct bridge_fixture {
  sound_facade facade{make_dummy_facade()};
  sfx_sound_rules rules{load_rules()};
  MersenneTwister rng{ora::game::support::make_mersenne_twister(20261108)};
  sfx_context ctx{};
  bridge_fixture() {
    facade.u4_local_player = kLocalPlayer;
    ctx.ptr_facade = &facade;
    ctx.ptr_rules = &rules;
    ctx.ptr_rng_cosmetic = &rng;
  }
};

void test_bridge_weapon_fired() {
  bridge_fixture fx;
  sfx_on_weapon_fired(fx.ctx, {}, wpos_at(1024, 2048));
  check(fx.facade.vec_log.empty(), "empty report silent");

  const std::int32_t int4_rng_before{fx.rng.int4_total_count};
  sfx_on_weapon_fired(fx.ctx, {"cannon1.aud", "cannon2.aud"}, wpos_at(1024, 2048));
  check(fx.facade.vec_log.size() == 1u, "fired one play2d");
  const std::string& str_line{fx.facade.vec_log[0]};
  check(str_line.ends_with("pos=1024,2048,0 loop=0 rel=0 vol=0.5 atten=1"),
        std::format("fired shape: {}", str_line));
  check(str_line.contains("name=cannon1.aud") || str_line.contains("name=cannon2.aud"),
        std::format("fired picked report: {}", str_line));
  check(fx.rng.int4_total_count == int4_rng_before + 1, "fired consumed one draw");
}

void test_bridge_warhead_impact() {
  bridge_fixture fx;
  ora::game::sfx_impact_sounds def1{.vec_explosions = {"x1", "x2"},
                                    .vec_impact_sounds = {},
                                    .int4_chance = 100};
  const std::int32_t int4_rng_before{fx.rng.int4_total_count};
  sfx_on_warhead_impact(fx.ctx, def1, wpos_at(0, 0));
  check(fx.facade.vec_log.empty(), "no impact sounds silent");
  check(fx.rng.int4_total_count == int4_rng_before + 1,
        std::format("explosion draw only: {} -> {}", int4_rng_before, fx.rng.int4_total_count));

  ora::game::sfx_impact_sounds def2{.vec_explosions = {"x1"},
                                    .vec_impact_sounds = {"boom1.aud", "boom2.aud", "boom3.aud"},
                                    .int4_chance = 0};
  fx.facade.vec_log.clear();
  const std::int32_t int4_rng_before2{fx.rng.int4_total_count};
  sfx_on_warhead_impact(fx.ctx, def2, wpos_at(4096, 4096));
  check(fx.facade.vec_log.empty(), "chance 0 silent");
  check(fx.rng.int4_total_count == int4_rng_before2 + 2,
        std::format("explosion(1 elem no draw)+impact+chance: {} -> {}", int4_rng_before2,
                    fx.rng.int4_total_count));

  ora::game::sfx_impact_sounds def3{.vec_explosions = {},
                                    .vec_impact_sounds = {"boom1.aud", "boom2.aud", "boom3.aud"},
                                    .int4_chance = 100};
  fx.facade.vec_log.clear();
  sfx_on_warhead_impact(fx.ctx, def3, wpos_at(4096, 4096));
  check(fx.facade.vec_log.size() == 1u, "impact plays once");
  check(fx.facade.vec_log[0].ends_with("pos=4096,4096,0 loop=0 rel=0 vol=0.5 atten=1") &&
            (fx.facade.vec_log[0].contains("name=boom1.aud") ||
             fx.facade.vec_log[0].contains("name=boom2.aud") ||
             fx.facade.vec_log[0].contains("name=boom3.aud")),
        std::format("impact line: {}", fx.facade.vec_log[0]));
}

void test_bridge_building_placed() {
  bridge_fixture fx;
  sfx_on_building_placed(fx.ctx, kLocalPlayer, {"placedown1.aud", "placedown2.aud"},
                         wpos_at(8192, 4096));
  check(fx.facade.vec_log.size() == 2u, "placement two play2d");
  check(fx.facade.vec_log[0] ==
            "  play2d name=placedown1.aud pos=8192,4096,0 loop=0 rel=0 vol=0.5 atten=1",
        std::format("placement line 1: {}", fx.facade.vec_log[0]));
  check(fx.facade.vec_log[1] ==
            "  play2d name=placedown2.aud pos=8192,4096,0 loop=0 rel=0 vol=0.5 atten=1",
        std::format("placement line 2: {}", fx.facade.vec_log[1]));

  fx.facade.vec_log.clear();
  sfx_on_building_placed(fx.ctx, kLocalPlayer + 1u, {"placedown1.aud"}, wpos_at(0, 0));
  check(fx.facade.vec_log.empty(), "placement foe silent");
}

void test_bridge_selection() {
  bridge_fixture fx;
  const std::vector<sfx_select_candidate> vec_all_foe{
      sfx_select_candidate{.u4_actor_id = 5, .u4_owner = kLocalPlayer + 1u,
                           .b_in_world = true, .b_has_voice = true,
                           .str_voice_type = "genericvoice", .str_phrase = "Select",
                           .str_faction = "allies"}};
  sfx_on_selection(fx.ctx, vec_all_foe);
  check(fx.facade.vec_log.empty(), "selection foe silent");

  const std::vector<sfx_select_candidate> vec_mixed{
      sfx_select_candidate{.u4_actor_id = 5, .u4_owner = kLocalPlayer + 1u,
                           .b_in_world = true, .b_has_voice = true,
                           .str_voice_type = "genericvoice", .str_phrase = "Select",
                           .str_faction = "allies"},
      sfx_select_candidate{.u4_actor_id = 7, .u4_owner = kLocalPlayer,
                           .b_in_world = true, .b_has_voice = true,
                           .str_voice_type = "genericvoice", .str_phrase = "Select",
                           .str_faction = "allies"},
      sfx_select_candidate{.u4_actor_id = 9, .u4_owner = kLocalPlayer,
                           .b_in_world = true, .b_has_voice = true,
                           .str_voice_type = "genericvoice", .str_phrase = "Action",
                           .str_faction = "allies"}};
  const std::int32_t int4_rng_before{fx.rng.int4_total_count};
  sfx_on_selection(fx.ctx, vec_mixed);
  check(fx.facade.vec_log.size() == 1u, "selection first eligible only");
  check(fx.facade.vec_log[0].ends_with("pos=0,0,0 loop=0 rel=1 vol=0.5 atten=1"),
        std::format("selection shape: {}", fx.facade.vec_log[0]));
  check(fx.facade.vec_log[0].contains(".v0"),
        std::format("selection voice variant: {}", fx.facade.vec_log[0]));
  check(fx.rng.int4_total_count == int4_rng_before + 1, "selection one pool draw");

  const std::vector<sfx_select_candidate> vec_not_in_world{
      sfx_select_candidate{.u4_actor_id = 7, .u4_owner = kLocalPlayer,
                           .b_in_world = false, .b_has_voice = true,
                           .str_voice_type = "genericvoice", .str_phrase = "Select",
                           .str_faction = "allies"}};
  fx.facade.vec_log.clear();
  sfx_on_selection(fx.ctx, vec_not_in_world);
  check(fx.facade.vec_log.empty(), "selection not-in-world skipped");
}

void test_bridge_order_voice() {
  bridge_fixture fx;
  const std::vector<sfx_order_voice> vec_orders{
      sfx_order_voice{.u4_actor_id = 5, .u4_owner = kLocalPlayer,
                      .str_voice_type = "genericvoice", .b_has_phrase = false,
                      .str_phrase = "", .str_faction = "allies"},
      sfx_order_voice{.u4_actor_id = 7, .u4_owner = kLocalPlayer,
                      .str_voice_type = "genericvoice", .b_has_phrase = true,
                      .str_phrase = "Action", .str_faction = "allies"}};
  const bool b_played{sfx_play_voice_for_orders(fx.ctx, vec_orders)};
  check(b_played, "order voice played via second subject");
  check(fx.facade.vec_log.size() == 1u, "order voice one play2d");

  const std::vector<sfx_order_voice> vec_none{
      sfx_order_voice{.u4_actor_id = 5, .u4_owner = kLocalPlayer,
                      .str_voice_type = "genericvoice", .b_has_phrase = false,
                      .str_phrase = "", .str_faction = "allies"}};
  fx.facade.vec_log.clear();
  check(!sfx_play_voice_for_orders(fx.ctx, vec_none), "no phrase no voice");
  check(fx.facade.vec_log.empty(), "no phrase silent");
}

void test_bridge_production_ready() {
  bridge_fixture fx;
  sfx_on_production_ready(fx.ctx, kLocalPlayer, "allies", "");
  check(fx.facade.vec_log.empty(), "empty ready audio silent");

  sfx_on_production_ready(fx.ctx, kLocalPlayer, "allies", "UnitReady");
  check(fx.facade.vec_log.size() == 1u &&
            fx.facade.vec_log[0].ends_with("rel=1 vol=0.5 atten=0"),
        std::format("ready line: {}", fx.facade.vec_log[0]));

  fx.facade.vec_log.clear();
  sfx_on_production_blocked(fx.ctx, kLocalPlayer + 3u, "allies", "RadarUp");
  check(fx.facade.vec_log.empty(), "blocked foe silent");
  sfx_on_production_blocked(fx.ctx, kLocalPlayer, "allies", "RadarUp");
  check(fx.facade.vec_log.size() == 1u, "blocked own plays");
}


#ifndef ORA_COMBAT_GOLDEN_DIR
#define ORA_COMBAT_GOLDEN_DIR "tests/golden/combat"
#endif

std::optional<std::string> read_file_bytes(const std::string& str_path) {
  std::ifstream fs_file{str_path, std::ios::binary};
  if (!fs_file) return std::nullopt;
  std::string str_text;
  fs_file.seekg(0, std::ios::end);
  const std::streampos pos_end{fs_file.tellg()};
  str_text.resize(static_cast<std::size_t>(pos_end));
  fs_file.seekg(0, std::ios::beg);
  fs_file.read(str_text.data(), static_cast<std::streamsize>(str_text.size()));
  return str_text;
}

std::vector<std::string> read_lines(const std::string& str_text) {
  std::vector<std::string> vec_lines;
  std::size_t sz_pos{0};
  while (sz_pos <= str_text.size()) {
    const std::size_t sz_end{str_text.find('\n', sz_pos)};
    std::string str_line{str_text.substr(
        sz_pos, sz_end == std::string::npos ? std::string::npos : sz_end - sz_pos)};
    if (!str_line.empty() && str_line.back() == '\r') str_line.pop_back();
    vec_lines.push_back(std::move(str_line));
    if (sz_end == std::string::npos) break;
    sz_pos = sz_end + 1;
  }
  return vec_lines;
}

std::optional<ora::game::combat::WeaponTable> load_weapon_table() {
  const auto str_manifest{
      read_file_bytes(std::format("{}/weapons-manifest.txt", ORA_COMBAT_GOLDEN_DIR))};
  if (!str_manifest.has_value()) return std::nullopt;

  std::vector<std::string> vec_texts;
  std::vector<std::string> vec_names;
  for (const std::string& str_entry : read_lines(*str_manifest)) {
    if (str_entry.empty()) continue;
    std::string str_rel{str_entry};
    const std::size_t idx_bar{str_rel.find('|')};
    if (idx_bar != std::string::npos) str_rel[idx_bar] = '_';
    const auto str_text{
        read_file_bytes(std::format("{}/weapons/{}", ORA_COMBAT_GOLDEN_DIR, str_rel))};
    if (!str_text.has_value()) return std::nullopt;
    vec_texts.push_back(*str_text);
    vec_names.push_back(str_entry);
  }

  auto res{ora::game::combat::weapon_table_load(vec_texts, vec_names)};
  if (!res.has_value()) return std::nullopt;
  return std::move(*res);
}

std::size_t count_lines(const std::vector<std::string>& vec_log, std::string_view str_needle) {
  std::size_t sz_count{0};
  for (const std::string& str_line : vec_log)
    if (str_line.contains(str_needle)) ++sz_count;
  return sz_count;
}

std::size_t first_line_index(const std::vector<std::string>& vec_log,
                             std::string_view str_needle) {
  for (std::size_t sz_i{0}; sz_i < vec_log.size(); ++sz_i)
    if (vec_log[sz_i].contains(str_needle)) return sz_i;
  return vec_log.size();
}

void test_chain_arena_sfx() {
  namespace cbt = ora::game::combat;
  const auto opt_table{load_weapon_table()};
  check(opt_table.has_value(), "weapon table loads for arena chain");
  if (!opt_table.has_value()) return;
  const ora::game::combat::WeaponTable& table{*opt_table};

  const auto run_arena{[&](ora::game::sfx_context* ptr_sfx) {
    cbt::Arena arena;
    arena.ptr_weapons = &table;
    arena.rng_shared = ora::game::support::make_mersenne_twister(0);
    arena.ptr_sfx = ptr_sfx;
    const auto res_a{cbt::arena_spawn(arena, "arty", 0, ora::game::pathfinding::CPos{30, 30})};
    const auto res_b{cbt::arena_spawn(arena, "arty", 1, ora::game::pathfinding::CPos{30, 38})};
    if (!res_a.has_value() || !res_b.has_value()) return std::pair{arena, false};
    cbt::arena_attack_order(arena, *res_a, *res_b);
    cbt::arena_attack_order(arena, *res_b, *res_a);
    for (int int4_tick{0}; int4_tick < 280; ++int4_tick) cbt::arena_tick(arena);
    return std::pair{arena, true};
  }};

  bridge_fixture fx;
  fx.ctx.ptr_selection = nullptr;
  const auto [arena_sfx, b_ok1]{run_arena(&fx.ctx)};
  check(b_ok1, "arena chain spawns");
  const auto [arena_plain, b_ok2]{run_arena(nullptr)};
  check(b_ok2, "arena chain spawns without sfx");

  check(count_lines(fx.facade.vec_log, "name=tank5.aud") >= 1u,
        std::format("fire reports heard: {}", count_lines(fx.facade.vec_log, "name=tank5.aud")));
  check(count_lines(fx.facade.vec_log, "name=kaboom15.aud") >= 1u,
        std::format("impact sounds heard: {}",
                    count_lines(fx.facade.vec_log, "name=kaboom15.aud")));
  check(count_lines(fx.facade.vec_log, "  play2d ") ==
            count_lines(fx.facade.vec_log, "name="),
        "only play2d lines logged");

  check(arena_sfx.rng_shared.int4_total_count == arena_plain.rng_shared.int4_total_count,
        std::format("shared rng count unchanged: {} vs {}",
                    arena_sfx.rng_shared.int4_total_count,
                    arena_plain.rng_shared.int4_total_count));
  check(arena_sfx.rng_shared.int4_last == arena_plain.rng_shared.int4_last,
        "shared rng last unchanged");
  check(arena_sfx.rng_shared.int4_total_count > 0, "shared rng actually exercised");
  check(cbt::arena_sync_hash(arena_sfx) == cbt::arena_sync_hash(arena_plain),
        "arena sync hash unchanged by sfx");
}

constexpr std::string_view kSfxChainRules{
    "world:\n"
    "\tFactionInfo@allies:\n"
    "\tFactionInfo@soviet:\n"
    "player:\n"
    "\tClassicProductionQueue@Building:\n"
    "\t\tType: Building\n"
    "\t\tLowPowerModifier: 300\n"
    "\t\tReadyAudio: UnitReady\n"
    "\t\tBlockedAudio: NoBuild\n"
    "\tPlaceBuilding:\n"
    "\tPlayerResources:\n"
    "\tPowerManager:\n"
    "\tProvidesTechPrerequisite@unrestricted:\n"
    "\t\tId: unrestricted\n"
    "\t\tPrerequisites: techlevel.infonly\n"
    "mcv:\n"
    "mpspawn:\n"
    "fact:\n"
    "\tBuilding:\n"
    "\t\tFootprint: xxX xxx XxX ===\n"
    "\t\tDimensions: 3,4\n"
    "\t\tLocalCenterOffset: 0,-512,0\n"
    "\tBaseProvider:\n"
    "\t\tRange: 16c0\n"
    "\tProduction:\n"
    "\t\tProduces: Building, Defense\n"
    "\tGivesBuildableArea:\n"
    "\tPower:\n"
    "\t\tAmount: 0\n"
    "powr:\n"
    "\tValued:\n"
    "\t\tCost: 300\n"
    "\tBuildable:\n"
    "\t\tQueue: Building\n"
    "\t\tPrerequisites: ~techlevel.infonly\n"
    "\tPower:\n"
    "\t\tAmount: 100\n"
    "\tBuilding:\n"
    "\t\tFootprint: xx xx ==\n"
    "\t\tDimensions: 2,3\n"
    "\t\tLocalCenterOffset: 0,-512,0\n"
    "\t\tRequiresBaseProvider: True\n"
    "\t\tBuildSounds: placedown1.aud,placedown2.aud\n"
    "\tGivesBuildableArea:\n"};

constexpr std::string_view kSfxChainMap{
    "Title: t19-sfx-chain\n"
    "Players:\n"
    "\tPlayerReference@Neutral:\n"
    "\t\tName: Neutral\n"
    "\t\tOwnsWorld: True\n"
    "\t\tNonCombatant: True\n"
    "\t\tFaction: allies\n"
    "\tPlayerReference@Multi0:\n"
    "\t\tName: Multi0\n"
    "\t\tPlayable: True\n"
    "\t\tFaction: Random\n"
    "Actors:\n"
    "\tActor0: mpspawn\n"
    "\t\tLocation: 8,8\n"
    "\t\tOwner: Neutral\n"
    "\tActor1: fact\n"
    "\t\tLocation: 30,30\n"
    "\t\tOwner: Multi0\n"};

ora::game::network::OrderModel make_chain_start_production() {
  ora::game::network::OrderModel order;
  order.str_order = "StartProduction";
  order.b_has_subject = true;
  order.u4_subject = kPlayerActorId;
  order.opt_str_target = "powr";
  order.u4_extra_data = 1;
  return order;
}

std::int32_t chain_cell_bits(std::int32_t int4_x, std::int32_t int4_y) {
  return static_cast<std::int32_t>(((int4_x & 0xfff) << 20) | ((int4_y & 0xfff) << 8));
}

ora::game::network::OrderModel make_chain_place_building() {
  ora::game::network::OrderModel order;
  order.str_order = "PlaceBuilding";
  order.b_has_subject = true;
  order.u4_subject = kPlayerActorId;
  order.target.kind = ora::game::network::TargetKind::kTerrain;
  order.target.b_cell = true;
  order.target.int4_cell_bits = chain_cell_bits(33, 30);
  order.target.u1_subcell = 0;
  order.opt_str_target = "powr";
  order.u4_extra_data = kPlayerActorId;
  return order;
}

std::expected<ora::game::world::WorldModel, std::string> make_chain_world() {
  const auto res_map{ora::game::world::synthetic_map_from_yaml(kSfxChainRules, kSfxChainMap)};
  if (!res_map.has_value()) return std::unexpected(res_map.error());
  ora::game::production::LobbyModel lobby;
  lobby.vec_playable_factions.emplace_back("Multi0", "russia");
  lobby.int4_starting_cash = 5000;
  lobby.str_techlevel = "unrestricted";
  return ora::game::world::world_create(*res_map, kSeed, lobby);
}

std::pair<ora::game::world::WorldModel, std::vector<std::string>> run_chain_world(
    ora::game::sfx_context* ptr_sfx, bool b_with_order) {
  auto res_world{make_chain_world()};
  if (!res_world.has_value()) return {ora::game::world::WorldModel{}, {}};
  ora::game::world::WorldModel w{std::move(*res_world)};
  w.ptr_sfx = ptr_sfx;

  ora::game::sfx_context ctx_local{};
  ora::game::sfx_context* ptr_ctx{ptr_sfx};
  if (ptr_ctx == nullptr && ptr_sfx == nullptr && false) ptr_ctx = &ctx_local;

  ora::game::network::OrderManagerModel om;
  const std::vector<std::int32_t> vec_clients{1};
  ora::game::network::order_manager_start_game(om, w, vec_clients);
  for (std::int32_t int4_tick{0}; int4_tick < 230; ++int4_tick) {
    if (b_with_order) {
      if (static_cast<std::int32_t>(w.u4_net_frame) == 2)
        ora::game::network::issue_order(om, make_chain_start_production());
      if (static_cast<std::int32_t>(w.u4_net_frame) == 200)
        ora::game::network::issue_order(om, make_chain_place_building());
    }
    ora::game::network::run_frame(om, w);
  }
  return {std::move(w), ptr_sfx != nullptr && ptr_sfx->ptr_facade != nullptr
                            ? ptr_sfx->ptr_facade->vec_log
                            : std::vector<std::string>{}};
}

void test_chain_world_production_sfx() {
  bridge_fixture fx;
  fx.ctx.ptr_selection = nullptr;
  fx.facade.u4_local_player = 1; 
  auto [w_sfx, vec_log]{run_chain_world(&fx.ctx, true)};
  check(!vec_log.empty(), "world chain emitted sounds");
  if (vec_log.empty()) return;

  const std::size_t sz_ready{first_line_index(vec_log, "name=unready1.aud")};
  check(sz_ready < vec_log.size(), std::format("ready notification heard: {}", vec_log.size()));
  check(vec_log[sz_ready].ends_with("pos=0,0,0 loop=0 rel=1 vol=0.5 atten=0"),
        std::format("ready notification shape: {}", vec_log[sz_ready]));
  const std::size_t sz_place{first_line_index(vec_log, "name=placedown1.aud")};
  check(sz_place < vec_log.size(), "placement sound heard");
  check(sz_ready < sz_place, "ready before placement");
  check(vec_log[sz_place] ==
            "  play2d name=placedown1.aud pos=34304,31232,0 loop=0 rel=0 vol=0.5 atten=1",
        std::format("placement shape: {}", vec_log[sz_place]));
  check(count_lines(vec_log, "name=placedown2.aud") == 1u, "second build sound heard");
  check(first_line_index(vec_log, "name=placedown2.aud") == sz_place + 1,
        "build sounds contiguous");

  auto [w_plain, vec_log_plain]{run_chain_world(nullptr, true)};
  check(w_sfx.rng_shared.int4_total_count == w_plain.rng_shared.int4_total_count,
        std::format("world shared rng count unchanged: {} vs {}",
                    w_sfx.rng_shared.int4_total_count, w_plain.rng_shared.int4_total_count));
  check(w_sfx.rng_shared.int4_last == w_plain.rng_shared.int4_last,
        "world shared rng last unchanged");
  check(ora::game::world::sync_hash_upstream(w_sfx) == ora::game::world::sync_hash_upstream(w_plain),
        "world sync hash unchanged by sfx");
  check(vec_log_plain.empty(), "no sfx run logs nothing");
}


constexpr std::string_view kSfxBaselineCommit{"1f594e32065615ed1847962c80ecfe6e868a11c3"};
constexpr std::int32_t kSfxDumpSeed{20261107};

const std::string& str_variant_allies_dump() {
  static const std::string str_s{"allies"};
  return str_s;
}

std::string sfx_dump_sequence() {
  sound_facade facade{make_dummy_facade()};
  sfx_sound_rules rules{};
  for (const auto& [str_key, info] :
       ora::game::load_sound_infos(std::string(ORA_AUDIO_GOLDEN_DIR) +
                                   "/yaml/notifications.yaml")
           .value_or({}))
    rules.map_notifications.emplace(str_key, info);
  for (const auto& [str_key, info] :
       ora::game::load_sound_infos(std::string(ORA_AUDIO_GOLDEN_DIR) + "/yaml/voices.yaml")
           .value_or({}))
    rules.map_voices.emplace(str_key, info);
  MersenneTwister rng{ora::game::support::make_mersenne_twister(kSfxDumpSeed)};

  std::vector<std::string> lines;
  lines.emplace_back(std::format("# baseline {} sfx sequence dump v1", kSfxBaselineCommit));
  lines.emplace_back(std::format("# seed {}", kSfxDumpSeed));

  std::size_t sz_step{0};
  const auto emit_step{[&](const std::string& str_head, auto&& fn_run) {
    lines.push_back(str_head);
    const std::string str_ret{fn_run()};
    for (const std::string& str_line : facade.vec_log) lines.push_back(str_line);
    facade.vec_log.clear();
    lines.push_back(std::format("  ret {}", str_ret));
    ++sz_step;
  }};

  const auto wpos0{wpos_at(0, 0)};
  const auto wpos_fire{wpos_at(1024, 2048)};
  const auto wpos_place{wpos_at(4096, 2048)};
  const auto wpos_voice{wpos_at(2048, 4096)};

  const auto ret_play{[](const std::optional<ora::platforms::sound>& snd) {
    return snd.has_value() ? std::string{"sound"} : std::string{"null"};
  }};
  const auto ret_predef{[](const std::expected<bool, std::string>& res) {
    if (!res.has_value()) return std::format("error {}", res.error());
    return *res ? std::string{"true"} : std::string{"false"};
  }};

  const auto predef_head{[&](const char* str_st, std::uint32_t u4_player_or_none, bool b_has_player,
                             int int4_voiced, bool b_selected, std::string_view str_type,
                             std::string_view str_def, std::string_view str_variant, bool b_rel,
                             ora::platforms::wpos pos, float f4_volmod, bool b_atten) {
    return std::format(
        "step {} predefined st={} player={} voiced={} selected={} type={} def={} variant={} "
        "rel={} pos={},{},{} volmod={} atten={}",
        sz_step, str_st, b_has_player ? std::to_string(u4_player_or_none) : "-", int4_voiced,
        b_selected ? 1 : 0, str_type, str_def, str_variant, b_rel ? 1 : 0, pos.int4_x, pos.int4_y,
        pos.int4_z, ora::game::sfx_fmt_float_r(f4_volmod), b_atten ? 1 : 0);
  }};

  const auto run_predef{[&](sound_type kind, bool b_has_player, std::uint32_t u4_player,
                            std::optional<sfx_voiced> opt_voiced, std::string_view str_type,
                            std::string_view str_def, const std::string* ptr_variant, bool b_rel,
                            ora::platforms::wpos pos, float f4_volmod, bool b_atten) {
    return ret_predef(sound_play_predefined(
        facade, rules, rng, kind, b_has_player ? u4_player : kSfxNoPlayerRef, opt_voiced,
        str_type, str_def, ptr_variant, b_rel, pos, f4_volmod, b_atten));
  }};

  facade.u4_local_player = kLocalPlayer;
  lines.emplace_back("local 2");

  const std::string str_no_variant_flag{};

  emit_step(std::format("step {} playtoplayer name=placedown1.aud player=2 pos=4096,2048,0",
                        sz_step),
            [&] {
              return ret_play(sound_play_to_player(facade, sound_type::world, kLocalPlayer,
                                                   "placedown1.aud", wpos_place));
            });
  emit_step(std::format("step {} playtoplayer name=placedown1.aud player=3 pos=4096,2048,0",
                        sz_step),
            [&] {
              return ret_play(sound_play_to_player(facade, sound_type::world, kLocalPlayer + 1u,
                                                   "placedown1.aud", wpos_place));
            });
  emit_step(std::format("step {} play name=explosion1.aud pos=1024,2048,0 volmod=1", sz_step),
            [&] { return ret_play(sound_play(facade, sound_type::world, "explosion1.aud", wpos_fire, 1.0f)); });
  emit_step(std::format("step {} play name= pos=0,0,0 volmod=1", sz_step),
            [&] { return ret_play(sound_play(facade, sound_type::world, "", wpos0, 1.0f)); });

  for (const int int4_repeat : {0, 1}) {
    static_cast<void>(int4_repeat);
    emit_step(predef_head("World", 0, false, -1, false, "sounds", "RadarUp", "-", true, wpos0, 1.0f,
                          false),
              [&] {
                return run_predef(sound_type::world, false, 0, std::nullopt, "sounds", "RadarUp",
                                  nullptr, true, wpos0, 1.0f, false);
              });
  }
  for (const int int4_repeat : {0, 1}) {
    static_cast<void>(int4_repeat);
    emit_step(predef_head("World", 0, false, -1, false, "sounds", "CashTickUp", "-", true, wpos0,
                          1.0f, false),
              [&] {
                return run_predef(sound_type::world, false, 0, std::nullopt, "sounds", "CashTickUp",
                                  nullptr, true, wpos0, 1.0f, false);
              });
  }
  for (const int int4_repeat : {0, 1}) {
    static_cast<void>(int4_repeat);
    emit_step(predef_head("World", 0, false, -1, false, "sounds", "ChatLine", "-", true, wpos0, 1.0f,
                          false),
              [&] {
                return run_predef(sound_type::world, false, 0, std::nullopt, "sounds", "ChatLine",
                                  nullptr, true, wpos0, 1.0f, false);
              });
  }
  emit_step(predef_head("World", 0, false, -1, false, "speech", "HarvesterAttack", "-", true, wpos0,
                        1.0f, false),
            [&] {
              return run_predef(sound_type::world, false, 0, std::nullopt, "speech",
                                "HarvesterAttack", nullptr, true, wpos0, 1.0f, false);
            });
  emit_step(predef_head("World", 0, false, -1, false, "speech", "NoSuchDef", "-", true, wpos0, 1.0f,
                        false),
            [&] {
              return run_predef(sound_type::world, false, 0, std::nullopt, "speech", "NoSuchDef",
                                nullptr, true, wpos0, 1.0f, false);
            });
  emit_step(predef_head("World", 0, false, -1, false, "notype", "RadarUp", "-", true, wpos0, 1.0f,
                        false),
            [&] {
              return run_predef(sound_type::world, false, 0, std::nullopt, "notype", "RadarUp",
                                nullptr, true, wpos0, 1.0f, false);
            });

  emit_step(predef_head("World", 0, false, 5, false, "genericvoice", "Select", "allies", false,
                        wpos_voice, 1.0f, true),
            [&] {
              return run_predef(sound_type::world, false, 0,
                                sfx_voiced{.u4_actor_id = 5, .b_selected = false}, "genericvoice",
                                "Select", &str_variant_allies_dump(), false, wpos_voice, 1.0f, true);
            });
  emit_step(predef_head("World", 0, false, 7, true, "genericvoice", "Select", "allies", false, wpos0,
                        1.0f, true),
            [&] {
              return run_predef(sound_type::world, false, 0,
                                sfx_voiced{.u4_actor_id = 7, .b_selected = true}, "genericvoice",
                                "Select", &str_variant_allies_dump(), false, wpos0, 1.0f, true);
            });
  emit_step(predef_head("World", 0, false, 7, true, "genericvoice", "Select", "allies", false, wpos0,
                        1.0f, true),
            [&] {
              return run_predef(sound_type::world, false, 0,
                                sfx_voiced{.u4_actor_id = 7, .b_selected = true}, "genericvoice",
                                "Select", &str_variant_allies_dump(), false, wpos0, 1.0f, true);
            });
  emit_step(predef_head("World", 0, false, 9, false, "genericvoice", "Action", "allies", false, wpos0,
                        1.0f, true),
            [&] {
              return run_predef(sound_type::world, false, 0,
                                sfx_voiced{.u4_actor_id = 9, .b_selected = false}, "genericvoice",
                                "Action", &str_variant_allies_dump(), false, wpos0, 1.0f, true);
            });
  emit_step(predef_head("World", 0, false, 5, true, "genericvoice", "Action", "allies", false, wpos0,
                        1.0f, true),
            [&] {
              return run_predef(sound_type::world, false, 0,
                                sfx_voiced{.u4_actor_id = 5, .b_selected = true}, "genericvoice",
                                "Action", &str_variant_allies_dump(), false, wpos0, 1.0f, true);
            });

  facade.u4_local_player = kSfxNoPlayerRef;
  lines.emplace_back("local -");
  emit_step(predef_head("World", 2, true, -1, false, "sounds", "RadarUp", "-", true, wpos0, 1.0f,
                        false),
            [&] {
              return run_predef(sound_type::world, true, kLocalPlayer, std::nullopt, "sounds",
                                "RadarUp", nullptr, true, wpos0, 1.0f, false);
            });
  emit_step(std::format("step {} notification player=2 type=Sounds def=RadarUp variant=-", sz_step),
            [&] {
              return ret_predef(sound_play_notification(facade, rules, rng, kLocalPlayer, "Sounds",
                                                        "RadarUp", nullptr));
            });

  facade.u4_local_player = kLocalPlayer;
  lines.emplace_back("local 2");
  emit_step(std::format("step {} notification player=2 type=Sounds def=RadarUp variant=-", sz_step),
            [&] {
              return ret_predef(sound_play_notification(facade, rules, rng, kLocalPlayer, "Sounds",
                                                        "RadarUp", nullptr));
            });

  std::string str_out;
  for (const std::string& str_line : lines) str_out += str_line + "\n";
  return str_out;
}

void test_sfx_golden_sequence() {
  const auto str_dump{sfx_dump_sequence()};
  const auto opt_golden{
      read_file_bytes(std::string(ORA_SFX_GOLDEN_DIR) + "/sequence.txt")};
  check(opt_golden.has_value(), "sfx golden sequence readable");
  if (!opt_golden.has_value()) return;
  check(str_dump == *opt_golden, [&] {
    const std::string& str_g{*opt_golden};
    std::size_t sz_i{0};
    while (sz_i < str_dump.size() && sz_i < str_g.size() && str_dump[sz_i] == str_g[sz_i]) ++sz_i;
    const std::size_t sz_line_start{str_dump.rfind('\n', sz_i == 0 ? 0 : sz_i - 1)};
    const std::string str_ctx{str_dump.substr(
        sz_line_start == std::string::npos ? 0 : sz_line_start + 1,
        std::min<std::size_t>(120, str_dump.size() - (sz_line_start == std::string::npos
                                                          ? 0
                                                          : sz_line_start + 1)))};
    return std::format("sfx sequence byte-identical (first diff @{}: cpp=[{}])", sz_i, str_ctx);
  }());
}

}

int main() {
  test_play_to_player_ownership();
  test_play_gates();
  test_play_volume_chain();
  test_play_predefined_notifications();
  test_play_predefined_voices();
  test_play_notification_lowering();
  test_bridge_weapon_fired();
  test_bridge_warhead_impact();
  test_bridge_building_placed();
  test_bridge_selection();
  test_bridge_order_voice();
  test_bridge_production_ready();
  test_chain_arena_sfx();
  test_chain_world_production_sfx();
  test_sfx_golden_sequence();
  return ora::test::test_exit_code("ora_sfx");
}

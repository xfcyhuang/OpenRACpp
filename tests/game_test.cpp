import std;
#include "game/mod_data.hpp"
#include "game/game_records.hpp"
#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include <cstdio>

namespace ora::gen {
void RegisterGeneratedAll();
}

namespace {

using namespace ora;

int int4_failures = 0;

void Check(bool b_cond, std::string_view sv_what) {
  if (!b_cond) {
    std::println("FAIL: {}", sv_what);
    int4_failures++;
  }
}

const meta::GenericValue* SlotOf(const meta::RecordObject& rec, std::string_view sv_field) {
  const auto* rec_generated = dynamic_cast<const meta::GeneratedRecord*>(&rec);
  if (rec_generated == nullptr)
    return nullptr;
  const std::vector<const meta::FieldDesc*> vec_fields =
      meta::CollectFields(rec_generated->record_desc());
  const std::vector<meta::GenericValue>& vec_values = rec_generated->Values();
  for (std::size_t int4_i{}; int4_i < vec_fields.size() && int4_i < vec_values.size(); int4_i++)
    if (vec_fields[int4_i]->str_name == sv_field)
      return &vec_values[int4_i];
  return nullptr;
}

const meta::GeneratedRecord* TraitOf(const game::ActorInfo& actor,
                                     std::string_view sv_class, std::string_view sv_instance = {}) {
  const auto& vec_traits = actor.Traits();
  const auto& vec_instances = actor.TraitInstanceNames();
  for (std::size_t int4_i{}; int4_i < vec_traits.size(); int4_i++) {
    if (actor.Traits()[int4_i]->record_desc().str_name != sv_class)
      continue;
    if (vec_instances[int4_i] != sv_instance)
      continue;
    return dynamic_cast<const meta::GeneratedRecord*>(vec_traits[int4_i].get());
  }
  return nullptr;
}

std::int64_t AsInt(const meta::GenericValue& val) {
  return std::get<std::int64_t>(val.val);
}

const std::string& AsString(const meta::GenericValue& val) {
  return std::get<std::string>(val.val);
}

}

int main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 2) {
    std::println("usage: game_test <upstream-root>");
    return 1;
  }

  gen::RegisterGeneratedAll();
  game::RegisterGameLoaders();

  game::InstalledMods mods_installed{std::string{argv[1]} + "/mods"};
  const game::Manifest* manifest = mods_installed.Find("ra");
  Check(manifest != nullptr, "ra mod found");
  if (manifest == nullptr)
    return 1;

  game::ModData data_mod{*manifest, mods_installed, argv[1]};
  game::Ruleset& rules = data_mod.DefaultRules();

  const game::ActorInfo* actor_v2rl = rules.FindActor("v2rl");
  Check(actor_v2rl != nullptr, "actor v2rl present");

  if (actor_v2rl != nullptr) {

    const meta::GeneratedRecord* rec_health = TraitOf(*actor_v2rl, "HealthInfo");
    Check(rec_health != nullptr, "v2rl HealthInfo present");
    if (rec_health != nullptr) {
      Check(AsInt(*SlotOf(*rec_health, "HP")) == 20000, "HealthInfo.HP == 20000");
      Check(std::get<bool>(SlotOf(*rec_health, "NotifyAppliedDamage")->val) == true,
            "HealthInfo.NotifyAppliedDamage == true (C# 默认)");
      Check(AsInt(*SlotOf(*rec_health, "EditorHealthDisplayOrder")) == 2,
            "HealthInfo.EditorHealthDisplayOrder == 2 (C# 默认)");
    }

    const meta::GeneratedRecord* rec_mobile = TraitOf(*actor_v2rl, "MobileInfo");
    Check(rec_mobile != nullptr, "v2rl MobileInfo present");
    if (rec_mobile != nullptr) {
      Check(AsString(*SlotOf(*rec_mobile, "Locomotor")) == "wheeled",
            "MobileInfo.Locomotor == wheeled (^Vehicle 继承)");
      Check(AsInt(*SlotOf(*rec_mobile, "Speed")) == 72, "MobileInfo.Speed == 72 (自身覆盖)");
      Check(AsInt(*SlotOf(*rec_mobile, "TurnSpeed")) == 20,
            "MobileInfo.TurnSpeed == 20 (^Vehicle 继承)");
      Check(AsString(*SlotOf(*rec_mobile, "PauseOnCondition")) == "being-captured",
            "MobileInfo.PauseOnCondition == being-captured (表达式文本)");
      Check(AsInt(*SlotOf(*rec_mobile, "InitialFacing")) == 0,
            "MobileInfo.InitialFacing == 0 (C# 默认 WAngle.Zero)");
    }

    const meta::GeneratedRecord* rec_armament = TraitOf(*actor_v2rl, "ArmamentInfo");
    Check(rec_armament != nullptr, "v2rl ArmamentInfo present");
    if (rec_armament != nullptr) {
      Check(AsString(*SlotOf(*rec_armament, "Weapon")) == "SCUD", "ArmamentInfo.Weapon == SCUD");
      Check(AsString(*SlotOf(*rec_armament, "ReloadingCondition")) == "reloading",
            "ArmamentInfo.ReloadingCondition == reloading");
      Check(AsString(*SlotOf(*rec_armament, "Name")) == "primary",
            "ArmamentInfo.Name == primary (C# 默认)");
      Check(AsInt(*SlotOf(*rec_armament, "TargetRelationships")) == 1,
            "ArmamentInfo.TargetRelationships == Enemy(1)");
      Check(AsInt(*SlotOf(*rec_armament, "ForceTargetRelationships")) == 7,
            "ArmamentInfo.ForceTargetRelationships == Enemy|Neutral|Ally(7)");
    }

    const meta::GeneratedRecord* rec_bodyLoaded = TraitOf(*actor_v2rl, "WithFacingSpriteBodyInfo");
    Check(rec_bodyLoaded != nullptr, "WithFaceSpriteBody(default) present");
    if (rec_bodyLoaded != nullptr)
      Check(AsString(*SlotOf(*rec_bodyLoaded, "RequiresCondition")) == "!reloading",
            "WithFaceSpriteBody.RequiresCondition == !reloading");
    const meta::GeneratedRecord* rec_bodyEmpty =
        TraitOf(*actor_v2rl, "WithFacingSpriteBodyInfo", "EMPTY");
    Check(rec_bodyEmpty != nullptr, "WithFaceSpriteBody@EMPTY present");
    if (rec_bodyEmpty != nullptr) {
      Check(AsString(*SlotOf(*rec_bodyEmpty, "Sequence")) == "empty-idle",
            "WithFaceSpriteBody@EMPTY.Sequence == empty-idle");
      Check(AsString(*SlotOf(*rec_bodyEmpty, "Name")) == "reloading",
            "WithFaceSpriteBody@EMPTY.Name == reloading");
    }

    try {
      const std::size_t int4_count = actor_v2rl->TraitsInConstructOrder().size();
      Check(int4_count == actor_v2rl->Traits().size(),
            "TraitsInConstructOrder resolves every trait");
    } catch (const std::exception& e_except) {
      Check(false, std::format("TraitsInConstructOrder threw: {}", e_except.what()));
    }
  }

  const game::WeaponInfo* weapon_scud = rules.FindWeapon("scud");
  Check(weapon_scud != nullptr, "weapon SCUD present");
  if (weapon_scud != nullptr) {
    Check(weapon_scud->int4_reloadDelay == 215, "SCUD.ReloadDelay == 215 (自身覆盖 50)");
    Check(weapon_scud->int4_range == 10240, "SCUD.Range == 10c0 == 10240");
    Check(weapon_scud->int4_minRange == 4096, "SCUD.MinRange == 4c0 == 4096 (覆盖 0c512)");
    Check(weapon_scud->vec_report.size() == 1 && weapon_scud->vec_report[0] == "missile1.aud",
          "SCUD.Report == [missile1.aud] (覆盖继承的 missile6.aud)");
    Check(weapon_scud->int4_burst == 1, "SCUD.Burst == 1 (C# 默认)");

    Check(weapon_scud->rec_projectile != nullptr, "SCUD.Projectile present");
    if (weapon_scud->rec_projectile != nullptr) {
      Check(std::string_view{weapon_scud->rec_projectile->record_desc().str_name} == "BulletInfo",
            "SCUD.Projectile is BulletInfo (not the inherited Missile)");
      Check(AsString(*SlotOf(*weapon_scud->rec_projectile, "TrailImage")) == "smokey",
            "BulletInfo.TrailImage == smokey");
      Check(AsInt(*SlotOf(*weapon_scud->rec_projectile, "TrailDelay")) == 5,
            "BulletInfo.TrailDelay == 5");
      Check(AsInt(*SlotOf(*weapon_scud->rec_projectile, "Inaccuracy")) == 213,
            "BulletInfo.Inaccuracy == 213");

      const meta::GenericValue* val_speed = SlotOf(*weapon_scud->rec_projectile, "Speed");
      Check(val_speed != nullptr &&
                std::get<std::vector<meta::GenericValue>>(val_speed->val).size() == 1 &&
                AsInt(std::get<std::vector<meta::GenericValue>>(val_speed->val)[0]) == 170,
            "BulletInfo.Speed == [170]");
    }

    Check(weapon_scud->vec_warheads.size() >= 1, "SCUD.Warheads non-empty (继承)");
  }

  Check(rules.FindActor("world") != nullptr, "system actor world backfilled");
  Check(rules.FindActor("editorplayer") != nullptr, "system actor editorplayer backfilled");

  if (int4_failures == 0) {
    std::println("game_test: all passed");
    return 0;
  }
  std::println("game_test: {} FAILURES", int4_failures);
  return 1;
}

module ora.game.combat;

import std;
import ora.game.miniyaml;

namespace ora::game::combat {

namespace {

std::optional<std::int32_t> parse_int(std::string_view str_value) {
  std::int32_t int4_out{};
  const char* ptr_first{str_value.data()};
  const char* ptr_last{str_value.data() + str_value.size()};
  const auto res{std::from_chars(ptr_first, ptr_last, int4_out)};
  if (res.ec != std::errc{} || res.ptr != ptr_last) return std::nullopt;
  return int4_out;
}

std::optional<bool> parse_bool(std::string_view str_value) {

  if (str_value == "True" || str_value == "true") return true;
  if (str_value == "False" || str_value == "false") return false;
  return std::nullopt;
}

const MiniYamlNode* find_child(const std::vector<MiniYamlNode>& vec_nodes,
                               std::string_view str_key) {
  for (const MiniYamlNode& node : vec_nodes)
    if (node.str_key == str_key) return &node;
  return nullptr;
}

std::string to_lower_ascii(std::string_view str_in) {
  std::string str_out;
  str_out.reserve(str_in.size());
  for (const char ch : str_in)
    str_out.push_back(ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : ch);
  return str_out;
}

std::optional<int> parse_wdist_token(std::string_view str_token) {
  const std::size_t idx_c{str_token.find_first_of("cC")};
  if (idx_c == std::string_view::npos) return parse_int(str_token);
  const auto opt_cell{parse_int(str_token.substr(0, idx_c))};
  const auto opt_sub{parse_int(str_token.substr(idx_c + 1))};
  if (!opt_cell.has_value() || !opt_sub.has_value()) return std::nullopt;
  std::int32_t int4_sub{*opt_sub};
  if (*opt_cell < 0) int4_sub = -int4_sub;
  return 1024 * *opt_cell + int4_sub;
}

std::vector<std::string> split_comma(std::string_view str_value) {
  std::vector<std::string> vec_parts;
  std::size_t sz_pos{0};
  while (sz_pos <= str_value.size()) {
    const std::size_t idx_comma{str_value.find(',', sz_pos)};
    const std::size_t sz_end{idx_comma == std::string_view::npos ? str_value.size() : idx_comma};
    std::string_view str_part{str_value.substr(sz_pos, sz_end - sz_pos)};
    while (!str_part.empty() && (str_part.front() == ' ' || str_part.front() == '\t'))
      str_part.remove_prefix(1);
    while (!str_part.empty() && (str_part.back() == ' ' || str_part.back() == '\t'))
      str_part.remove_suffix(1);
    vec_parts.emplace_back(str_part);
    if (idx_comma == std::string_view::npos) break;
    sz_pos = idx_comma + 1;
  }
  return vec_parts;
}

std::vector<int> parse_wdist_list(std::string_view str_value) {
  std::vector<int> vec_out;
  for (const std::string& str_part : split_comma(str_value)) {
    if (const auto opt{parse_wdist_token(str_part)}) vec_out.push_back(*opt);
  }
  return vec_out;
}

std::vector<std::string> parse_string_list(std::string_view str_value) {
  std::vector<std::string> vec_out;
  for (const std::string& str_part : split_comma(str_value))
    if (!str_part.empty()) vec_out.push_back(str_part);
  return vec_out;
}

WarheadDef parse_warhead(const MiniYamlNode& node_warhead) {
  WarheadDef wh;
  const std::string_view str_type{node_warhead.str_value};
  wh.str_kind_name = std::format("{}Warhead", str_type);
  if (str_type == "SpreadDamage") {
    wh.kind = WarheadKind::kSpreadDamage;
  } else if (str_type == "CreateEffect") {
    wh.kind = WarheadKind::kCreateEffect;
  } else if (str_type == "LeaveSmudge") {
    wh.kind = WarheadKind::kLeaveSmudge;
  } else {
    wh.kind = WarheadKind::kLeaveSmudge;
    wh.str_kind_name = std::string{str_type} + "Warhead";
  }

  const std::vector<MiniYamlNode>& vec_children{node_warhead.vec_children};

  if (const MiniYamlNode* node{find_child(vec_children, "Delay")}) {
    if (const auto opt{parse_int(node->str_value)}) wh.int4_delay = *opt;
  }
  if (const MiniYamlNode* node{find_child(vec_children, "Inaccuracy")}) {
    if (const auto opt{parse_wdist_token(node->str_value)}) wh.int4_inaccuracy = *opt;
  }
  if (const MiniYamlNode* node{find_child(vec_children, "Damage")}) {
    if (const auto opt{parse_int(node->str_value)}) wh.int4_damage = *opt;
  }
  if (const MiniYamlNode* node{find_child(vec_children, "Falloff")})
    wh.vec_falloff = parse_wdist_list(node->str_value);
  else if (wh.kind == WarheadKind::kSpreadDamage)

    wh.vec_falloff = {100, 37, 14, 5, 0};
  if (const MiniYamlNode* node{find_child(vec_children, "Spread")}) {
    if (const auto opt{parse_wdist_token(node->str_value)}) wh.int4_spread = *opt;
  } else if (wh.kind == WarheadKind::kSpreadDamage)
    wh.int4_spread = 43;
  if (const MiniYamlNode* node{find_child(vec_children, "ValidTargets")})
    wh.vec_valid_targets = parse_string_list(node->str_value);
  if (const MiniYamlNode* node{find_child(vec_children, "InvalidTargets")})
    wh.vec_invalid_targets = parse_string_list(node->str_value);
  if (const MiniYamlNode* node{find_child(vec_children, "Explosions")})
    wh.vec_explosions = parse_string_list(node->str_value);
  if (const MiniYamlNode* node{find_child(vec_children, "ImpactSounds")})
    wh.vec_impact_sounds = parse_string_list(node->str_value);
  if (const MiniYamlNode* node{find_child(vec_children, "ImpactSoundChance")})
    if (const auto opt{parse_int(node->str_value)}) wh.int4_impact_sound_chance = *opt;
  if (const MiniYamlNode* node_versus{find_child(vec_children, "Versus")}) {
    for (const MiniYamlNode& node_entry : node_versus->vec_children) {
      const auto opt_value{parse_int(node_entry.str_value)};
      if (opt_value.has_value())
        wh.vec_versus.emplace_back(node_entry.str_key, *opt_value);
    }
  }

  return wh;
}

ProjectileDef parse_projectile(const MiniYamlNode& node_projectile) {
  ProjectileDef proj;
  const std::string_view str_type{node_projectile.str_value};
  proj.str_kind_name = std::format("{}Info", str_type);
  if (str_type == "InstantHit") {
    proj.kind = ProjectileKind::kInstantHit;
  } else if (str_type == "Bullet") {
    proj.kind = ProjectileKind::kBullet;
  } else {
    proj.kind = ProjectileKind::kNone;
    proj.str_kind_name = std::string{str_type} + "Info";
  }

  const std::vector<MiniYamlNode>& vec_children{node_projectile.vec_children};

  if (const MiniYamlNode* node{find_child(vec_children, "Speed")}) {
    if (const auto opt{parse_wdist_token(node->str_value)}) proj.int4_speed = *opt;
  }
  if (const MiniYamlNode* node{find_child(vec_children, "LaunchAngle")}) {
    for (const std::string& str_part : split_comma(node->str_value)) {
      if (const auto opt{parse_int(str_part)}) proj.vec_launch_angle.push_back(*opt);
    }
  }
  if (const MiniYamlNode* node{find_child(vec_children, "Inaccuracy")}) {
    if (const auto opt{parse_wdist_token(node->str_value)}) proj.int4_inaccuracy = *opt;
  }
  if (const MiniYamlNode* node{find_child(vec_children, "Blockable")}) {
    if (const auto opt{parse_bool(node->str_value)}) proj.b_blockable = *opt;
  }
  if (const MiniYamlNode* node{find_child(vec_children, "ContrailLength")}) {
    if (const auto opt{parse_int(node->str_value)}) proj.int4_contrail_length = *opt;
  }

  return proj;
}

}

std::optional<int> wdist_parse(std::string_view str_value) { return parse_wdist_token(str_value); }

std::expected<WeaponTable, std::string> weapon_table_load(
    const std::vector<std::string>& vec_file_texts,
    const std::vector<std::string>& vec_file_names) {
  if (vec_file_texts.size() != vec_file_names.size())
    return std::unexpected("weapon_table_load: texts/names size mismatch");

  std::vector<std::vector<MiniYamlNode>> vec_sources;
  vec_sources.reserve(vec_file_texts.size());
  for (std::size_t idx{0}; idx < vec_file_texts.size(); ++idx) {
    auto res{miniyaml_from_string(vec_file_texts[idx], vec_file_names[idx], true)};
    if (!res.has_value()) return std::unexpected(res.error());
    vec_sources.push_back(std::move(*res));
  }

  auto res_merged{miniyaml_merge(vec_sources)};
  if (!res_merged.has_value()) return std::unexpected(res_merged.error());

  WeaponTable table;
  for (const MiniYamlNode& node : *res_merged) {
    WeaponDef def;
    def.str_name_lower = to_lower_ascii(node.str_key);

    auto res_self{miniyaml_merge({node.vec_children})};
    if (!res_self.has_value()) return std::unexpected(res_self.error());

    const std::vector<MiniYamlNode>& vec_children{*res_self};

    if (const MiniYamlNode* n{find_child(vec_children, "Range")}) {
      if (const auto opt{parse_wdist_token(n->str_value)}) def.int4_range = *opt;
    }
    if (const MiniYamlNode* n{find_child(vec_children, "MinRange")}) {
      if (const auto opt{parse_wdist_token(n->str_value)}) def.int4_min_range = *opt;
    }
    if (const MiniYamlNode* n{find_child(vec_children, "ReloadDelay")}) {
      if (const auto opt{parse_int(n->str_value)}) def.int4_reload_delay = *opt;
    }
    if (const MiniYamlNode* n{find_child(vec_children, "Burst")}) {
      if (const auto opt{parse_int(n->str_value)}) def.int4_burst = *opt;
    }
    if (const MiniYamlNode* n{find_child(vec_children, "BurstDelays")})
      def.vec_burst_delays = parse_wdist_list(n->str_value);
    if (const MiniYamlNode* n{find_child(vec_children, "TargetActorCenter")}) {
      if (const auto opt{parse_bool(n->str_value)}) def.b_target_actor_center = *opt;
    }
    if (const MiniYamlNode* n{find_child(vec_children, "ValidTargets")})
      def.vec_valid_targets = parse_string_list(n->str_value);
    if (const MiniYamlNode* n{find_child(vec_children, "InvalidTargets")})
      def.vec_invalid_targets = parse_string_list(n->str_value);
    if (const MiniYamlNode* n{find_child(vec_children, "Report")})
      def.vec_report = parse_string_list(n->str_value);
    if (const MiniYamlNode* n{find_child(vec_children, "Projectile")})
      def.projectile = parse_projectile(*n);
    for (const MiniYamlNode& node_child : vec_children)
      if (node_child.str_key.starts_with("Warhead"))
        def.vec_warheads.push_back(parse_warhead(node_child));

    table.vec_weapons.push_back(std::move(def));
  }

  return table;
}

}

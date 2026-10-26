#include "game/local_player_profile.hpp"

#include "yaml/mini_yaml.hpp"

namespace ora::game {

namespace {

LocalPlayerProfileState& ProfileSlot() {
  static std::unique_ptr<LocalPlayerProfileState> up_state =
      std::make_unique<LocalPlayerProfileState>();
  return *up_state;
}

}  // namespace

LocalPlayerProfileState& LocalPlayerProfileInstance() {
  return ProfileSlot();
}

void LocalPlayerProfileResetForTest() {
  ProfileSlot() = LocalPlayerProfileState{};
}

void ProfileFireStateChanged(LocalPlayerProfileState& profile_state) {
  for (const auto& fn_handler : profile_state.vec_state_changed)
    if (fn_handler)
      fn_handler();
}

void ProfileSetState(LocalPlayerProfileState& profile_state,
                     LinkState kind_state) {
  profile_state.kind_state = kind_state;
  ProfileFireStateChanged(profile_state);
}

std::function<void()> ProfileAddStateChangedHandler(
    LocalPlayerProfileState& profile_state, std::function<void()> fn_handler) {
  profile_state.vec_state_changed.push_back(std::move(fn_handler));
  const std::size_t sz_slot = profile_state.vec_state_changed.size() - 1;
  return [sz_slot] {
    LocalPlayerProfileState& state_slot = LocalPlayerProfileInstance();
    if (sz_slot < state_slot.vec_state_changed.size())
      state_slot.vec_state_changed[sz_slot] = nullptr;
  };
}

void ProfileRefreshPlayerData(LocalPlayerProfileState& profile_state) {
  if (profile_state.deps.fn_refresh_player_data)
    profile_state.deps.fn_refresh_player_data();
}

void ProfileLinkForumAccount(LocalPlayerProfileState& profile_state,
                             const std::string& str_username,
                             const std::string& str_password,
                             std::function<void(LinkResult)> fn_callback) {
  if (profile_state.deps.fn_link_forum_account)
    profile_state.deps.fn_link_forum_account(str_username, str_password,
                                             std::move(fn_callback));
}

void ProfileDeleteKeypair(LocalPlayerProfileState& profile_state) {
  if (profile_state.deps.fn_delete_keypair)
    profile_state.deps.fn_delete_keypair();
}

PlayerDatabase PlayerDatabaseFromYaml(const yaml::MiniYaml& yaml_source) {
  PlayerDatabase db_player;
  for (const auto& node : yaml_source.Nodes) {
    if (node.Key == nullptr || node.Value.Value == nullptr)
      continue;
    const std::string_view sv_key = *node.Key;
    const std::string_view sv_value = *node.Value.Value;
    if (sv_key == "Forum")
      db_player.str_forum = std::string{sv_value};
    else if (sv_key == "Profile")
      db_player.str_profile = std::string{sv_value};
    else if (sv_key == "Link")
      db_player.str_link = std::string{sv_value};
    else if (sv_key == "IconSize")
      std::from_chars(sv_value.data(), sv_value.data() + sv_value.size(),
                      db_player.int4_icon_size);
  }
  return db_player;
}

}  // namespace ora::game

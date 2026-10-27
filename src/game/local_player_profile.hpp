#pragma once
import std;

namespace ora::yaml {
class MiniYaml;
}

namespace ora::game {

enum class LinkState : std::uint8_t {
  Unlinked,
  Linking,
  CheckingLink,
  ConnectionFailed,
  Linked,
};

enum class LinkResult : std::uint8_t {
  Success,
  AuthFailure,
  LoginAttempts,
  Banned,
  Error,
  ConnectionFailed,
};

struct PlayerBadge {
  std::string str_label;
  std::string str_icon;
  std::string str_icon2x;
  std::string str_icon3x;
};

struct PlayerProfile {
  std::string str_fingerprint;
  std::string str_public_key;
  bool b_key_revoked = false;
  std::int64_t int8_profile_id = 0;
  std::string str_profile_name;
  std::string str_profile_rank = "Registered Player";
  std::vector<PlayerBadge> vec_badges;
};

struct LocalPlayerProfileDeps {
  std::function<void()> fn_load_keypair;
  std::function<void()> fn_refresh_player_data;
  std::function<void(std::string, std::string,
                     std::function<void(LinkResult)>)>
      fn_link_forum_account;
  std::function<void()> fn_delete_keypair;
};

struct LocalPlayerProfileState {
  LinkState kind_state = LinkState::Unlinked;
  PlayerProfile profile_data{};
  LocalPlayerProfileDeps deps{};
  std::vector<std::function<void()>> vec_state_changed;
};

LocalPlayerProfileState& LocalPlayerProfileInstance();
void LocalPlayerProfileResetForTest();

void ProfileFireStateChanged(LocalPlayerProfileState& profile_state);
void ProfileSetState(LocalPlayerProfileState& profile_state,
                     LinkState kind_state);
std::function<void()> ProfileAddStateChangedHandler(
    LocalPlayerProfileState& profile_state, std::function<void()> fn_handler);
void ProfileRefreshPlayerData(LocalPlayerProfileState& profile_state);
void ProfileLinkForumAccount(LocalPlayerProfileState& profile_state,
                             const std::string& str_username,
                             const std::string& str_password,
                             std::function<void(LinkResult)> fn_callback);
void ProfileDeleteKeypair(LocalPlayerProfileState& profile_state);

struct PlayerDatabase {
  std::string str_forum = "https://forum.openra.net/";
  std::string str_profile = "https://forum.openra.net/openra/info/";
  std::string str_link = "https://forum.openra.net/openra/link";
  std::int32_t int4_icon_size = 24;
};

PlayerDatabase PlayerDatabaseFromYaml(const yaml::MiniYaml& yaml_source);

}

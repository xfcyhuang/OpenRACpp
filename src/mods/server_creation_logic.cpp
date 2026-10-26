// UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/ServerCreationLogic.cs
//          @1f594e320(实现;头注见 server_creation_logic.hpp)
//          Implementation; the header notes live in server_creation_logic.hpp.
#include "mods/server_creation_logic.hpp"

#include "fluent/fluent_provider.hpp"
#include "game/mod_data.hpp"
#include "gfx/sprite_font.hpp"
#include "map/map.hpp"
#include "map/map_cache.hpp"
#include "mods/confirmation_dialogs.hpp"
#include "mods/connection_logics.hpp"
#include "net/nat.hpp"
#include "server/server_settings.hpp"
#include "ui/ui.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/button_widget.hpp"
#include "ui/widgets/checkbox_widget.hpp"
#include "ui/widgets/label_widget.hpp"
#include "ui/widgets/password_field_widget.hpp"
#include "ui/widgets/text_field_widget.hpp"

#ifdef GetMessage
#undef GetMessage
#endif

namespace ora::mods {
namespace {

fluent::FluentProvider& Fluent() {
  return fluent::FluentProvider::Instance();
}

std::string Msg(std::string_view str_key) {
  return Fluent().GetMessage(str_key);
}

std::string Msg1(std::string_view str_key, std::string_view str_arg_name,
                 fluent::FluentValue val_value) {
  return Fluent().GetMessage(
      str_key,
      std::vector<std::pair<std::string, fluent::FluentValue>>{
          {std::string{str_arg_name}, std::move(val_value)}});
}

int ParsePortOrFallback(std::string_view sv_input, int int4_fallback) {
  std::size_t sz_begin = 0;
  std::size_t sz_end = sv_input.size();
  while (sz_begin < sz_end &&
         (sv_input[sz_begin] == ' ' || sv_input[sz_begin] == '\t'))
    ++sz_begin;
  while (sz_end > sz_begin &&
         (sv_input[sz_end - 1] == ' ' || sv_input[sz_end - 1] == '\t'))
    --sz_end;
  bool b_negative = false;
  if (sz_begin < sz_end &&
      (sv_input[sz_begin] == '+' || sv_input[sz_begin] == '-')) {
    b_negative = sv_input[sz_begin] == '-';
    ++sz_begin;
  }
  if (sz_begin >= sz_end || b_negative)
    return int4_fallback;
  long long ll_value = 0;
  for (std::size_t sz_i = sz_begin; sz_i < sz_end; ++sz_i) {
    const char c = sv_input[sz_i];
    if (c < '0' || c > '9')
      return int4_fallback;
    ll_value = ll_value * 10 + (c - '0');
    if (ll_value > 2147483647)
      return int4_fallback;
  }
  return static_cast<int>(ll_value);
}

std::string SanitizedServerNameLocal(std::string_view str_dirty,
                                     std::string_view str_player_name) {
  server::ServerSettings settings_temp;
  return settings_temp.SanitizedServerName(str_dirty, str_player_name);
}

}  // namespace

ServerCreationLogic::ServerCreationLogic(ora::ui::Widget& widget,
                                         game::ModData& mod_data,
                                         std::function<void()> fn_on_exit,
                                         std::function<void()> fn_open_lobby)
    : ptr_panel_(&widget),
      fn_on_exit_(std::move(fn_on_exit)),
      fn_open_lobby_(std::move(fn_open_lobby)) {
  static_cast<void>(&mod_data);
  auto& ui_singleton = ora::ui::Ui::Instance();
  const ui::ServerSettingsFace face_settings = ui_singleton.ServerSettingsGetDeps();

  map::MapCache* ptr_cache = ui_singleton.MapCacheDeps();
  if (ptr_cache != nullptr) {
    std::string str_initial = face_settings.Map;
    if (MersenneTwister* ptr_random = ui_singleton.CosmeticRandomDeps()) {
      const std::string str_last_modified =
          ptr_cache->PickLastModifiedMap(map::MapVisibility::Lobby);
      if (!str_last_modified.empty())
        str_initial = str_last_modified;
      ptr_map_ = &ptr_cache->At(
          ptr_cache->ChooseInitialMap(str_initial, *ptr_random));
    } else {
      ptr_map_ = &ptr_cache->At(str_initial);
    }
  }

  if (ptr_map_ != nullptr) {
    ora::ui::WidgetArgs args_preview;
    args_preview.Add("orderManager", static_cast<net::OrderManager*>(nullptr));
    args_preview.Add("getMap",
                     std::function<map::MapPreview*()>{[this] {
                       return ptr_map_;
                     }});
    args_preview.Add("showUnoccupiedSpawnpoints", false);
    args_preview.Add("mapUpdatesEnabled", true);
    args_preview.Add("onMapUpdate",
                     std::function<void(const std::string&)>{
                         [this, ptr_cache](const std::string& str_uid) {
                           if (ptr_cache != nullptr)
                             ptr_map_ = &ptr_cache->At(str_uid);
                         }});
    ui_singleton.LoadWidget("MAP_PREVIEW",
                            ptr_panel_->Get<ora::ui::Widget>(
                                "MAP_PREVIEW_ROOT"),
                            std::move(args_preview));
  }

  ptr_panel_->Get<ora::ui::ButtonWidget>("BACK_BUTTON")->fn_on_click =
      [this] {
        ora::ui::Ui::Instance().CloseWindow();
        if (fn_on_exit_)
          fn_on_exit_();
      };
  ptr_panel_->Get<ora::ui::ButtonWidget>("CREATE_BUTTON")->fn_on_click =
      [this] { CreateAndJoin(); };

  if (auto* ptr_map_button =
          ptr_panel_->GetOrNull<ora::ui::ButtonWidget>("MAP_BUTTON")) {
    ptr_map_button->fn_on_click = [this, ptr_cache] {
      ora::ui::WidgetArgs args_args;
      if (ptr_map_ != nullptr)
        args_args.Add("initialMap", ptr_map_->Uid());
      const std::vector<std::string> vec_empty;
      args_args.Add("remoteMapPool", &vec_empty);
      args_args.Add("initialTab",
                    static_cast<std::int32_t>(map::MapClassification::System));
      args_args.Add("onExit", std::function<void()>{[ptr_cache] {
                      if (ptr_cache != nullptr)
                        ptr_cache->UpdateMaps();
                    }});
      args_args.Add("onSelect",
                    std::function<void(const std::string&)>{
                        [this, ptr_cache](const std::string& str_uid) {
                          if (ptr_cache != nullptr)
                            ptr_map_ = &ptr_cache->At(str_uid);
                        }});
      args_args.Add(
          "filter",
          static_cast<std::int32_t>(map::MapVisibility::Lobby));
      args_args.Add("onStart", std::function<void()>{[] {}});
      ora::ui::Ui::Instance().OpenWindow("MAPCHOOSER_PANEL",
                                         std::move(args_args));
    };
  }

  auto* ptr_server_name =
      ptr_panel_->Get<ora::ui::TextFieldWidget>("SERVER_NAME");
  const std::string str_player_name = ui_singleton.PlayerNameDeps();
  str_server_name_ = SanitizedServerNameLocal(face_settings.Name,
                                              str_player_name);
  ptr_server_name->SetText(str_server_name_);
  ptr_server_name->fn_on_enter_key = [ptr_server_name](const ora::KeyInput&) {
    ptr_server_name->YieldKeyboardFocus();
    return true;
  };
  ptr_server_name->fn_on_lose_focus = [this, ptr_server_name,
                                       str_player_name]() mutable {
    str_server_name_ = SanitizedServerNameLocal(ptr_server_name->Text(),
                                                str_player_name);
    ptr_server_name->SetText(str_server_name_);
  };

  int4_listen_port_ = face_settings.ListenPort;
  ptr_panel_->Get<ora::ui::TextFieldWidget>("LISTEN_PORT")->SetText(
      std::to_string(int4_listen_port_));

  b_advertise_online_ = face_settings.AdvertiseOnline;

  auto* ptr_advertise_checkbox =
      ptr_panel_->Get<ora::ui::CheckboxWidget>("ADVERTISE_CHECKBOX");
  ptr_advertise_checkbox->fn_is_checked = [this] {
    return b_advertise_online_;
  };
  ptr_advertise_checkbox->fn_on_click = [this] {
    b_advertise_online_ = !b_advertise_online_;
    BuildNotices();
  };

  if (auto* ptr_password_field =
          ptr_panel_->GetOrNull<ora::ui::PasswordFieldWidget>("PASSWORD"))
    ptr_password_field->SetText(face_settings.Password);

  ptr_notices_label_a_ =
      ptr_panel_->GetOrNull<ora::ui::Widget>("NOTICES_HEADER_A");
  ptr_notices_label_b_ =
      ptr_panel_->GetOrNull<ora::ui::Widget>("NOTICES_HEADER_B");
  ptr_notices_label_c_ =
      ptr_panel_->GetOrNull<ora::ui::Widget>("NOTICES_HEADER_C");

  if (auto* ptr_notices_no_upnp =
          ptr_panel_->GetOrNull<ora::ui::Widget>("NOTICES_NO_UPNP")) {
    ptr_notices_no_upnp->fn_is_visible = [this] {
      return b_advertise_online_ &&
             (net::Nat::Instance().Status() == net::NatStatus::NotSupported ||
              net::Nat::Instance().Status() == net::NatStatus::Disabled);
    };

    if (auto* ptr_settings_a =
            ptr_notices_no_upnp->GetOrNull<ora::ui::Widget>("SETTINGS_A"))
      ptr_settings_a->fn_is_visible = [] {
        return net::Nat::Instance().Status() == net::NatStatus::Disabled;
      };

    if (auto* ptr_settings_b =
            ptr_notices_no_upnp->GetOrNull<ora::ui::Widget>("SETTINGS_B"))
      ptr_settings_b->fn_is_visible = [] {
        return net::Nat::Instance().Status() == net::NatStatus::Disabled;
      };
  }

  if (auto* ptr_notices_upnp =
          ptr_panel_->GetOrNull<ora::ui::Widget>("NOTICES_UPNP"))
    ptr_notices_upnp->fn_is_visible = [this] {
      return b_advertise_online_ &&
             net::Nat::Instance().Status() == net::NatStatus::Enabled;
    };

  if (auto* ptr_notices_lan =
          ptr_panel_->GetOrNull<ora::ui::Widget>("NOTICES_LAN"))
    ptr_notices_lan->fn_is_visible = [this] { return !b_advertise_online_; };

  BuildNotices();
}

ServerCreationLogic::~ServerCreationLogic() = default;

void ServerCreationLogic::CreateClickedForTest() { CreateAndJoin(); }

void ServerCreationLogic::AdvertiseClickedForTest() {
  if (auto* ptr_advertise_checkbox =
          ptr_panel_->GetOrNull<ora::ui::CheckboxWidget>(
              "ADVERTISE_CHECKBOX")) {
    ptr_advertise_checkbox->fn_on_click();
    return;
  }
  b_advertise_online_ = !b_advertise_online_;
  BuildNotices();
}

void ServerCreationLogic::BackClickedForTest() {
  if (auto* ptr_back =
          ptr_panel_->GetOrNull<ora::ui::ButtonWidget>("BACK_BUTTON"))
    ptr_back->fn_on_click();
}

void ServerCreationLogic::BuildNotices() {
  if (ptr_notices_label_a_ == nullptr || ptr_notices_label_b_ == nullptr ||
      ptr_notices_label_c_ == nullptr)
    return;

  if (b_advertise_online_) {
    str_notices_a_ = Msg("label-internet-server-nat-A") + " ";
    if (auto* ptr_label_a =
            dynamic_cast<ora::ui::LabelWidget*>(ptr_notices_label_a_)) {
      ptr_label_a->fn_get_text = [this] { return str_notices_a_; };
      const int int4_width =
          ora::ui::RequireWidgetFont(ptr_label_a->str_font)
              .Measure(str_notices_a_)
              .X;
      ptr_label_a->bounds_widget.int4_width = int4_width;
    }

    str_notices_b_ =
        net::Nat::Instance().Status() == net::NatStatus::Enabled
            ? Msg("label-internet-server-nat-B-enabled")
            : net::Nat::Instance().Status() == net::NatStatus::NotSupported
                  ? Msg("label-internet-server-nat-B-not-supported")
                  : Msg("label-internet-server-nat-B-disabled");
    if (auto* ptr_label_b =
            dynamic_cast<ora::ui::LabelWidget*>(ptr_notices_label_b_)) {
      ptr_label_b->fn_get_text = [this] { return str_notices_b_; };

      core::Color color_notice{};
      if (ora::ui::ChromeMetrics* ptr_metrics =
              ora::ui::Ui::Instance().ChromeMetricsDeps()) {
        const net::NatStatus status_nat = net::Nat::Instance().Status();
        const std::string_view sv_key =
            status_nat == net::NatStatus::Enabled
                ? std::string_view{"NoticeSuccessColor"}
                : status_nat == net::NatStatus::NotSupported
                      ? std::string_view{"NoticeErrorColor"}
                      : std::string_view{"NoticeInfoColor"};
        if (!ptr_metrics->TryGetColor(sv_key, color_notice))
          color_notice = core::Color{};
      }
      ptr_label_b->fn_get_color = [color_notice] { return color_notice; };

      const int int4_width =
          ora::ui::RequireWidgetFont(ptr_label_b->str_font)
              .Measure(str_notices_b_)
              .X;
      ptr_label_b->bounds_widget.int4_x =
          ptr_notices_label_a_->bounds_widget.int4_x +
          ptr_notices_label_a_->bounds_widget.int4_width;
      ptr_label_b->bounds_widget.int4_width = int4_width;
      ptr_label_b->b_visible = true;
    }
    b_notices_b_visible_ = true;

    if (auto* ptr_label_c =
            dynamic_cast<ora::ui::LabelWidget*>(ptr_notices_label_c_)) {
      const std::string str_text_c = Msg("label-internet-server-nat-C");
      ptr_label_c->fn_get_text = [str_text_c] { return str_text_c; };
      ptr_label_c->bounds_widget.int4_x =
          ptr_notices_label_b_->bounds_widget.int4_x +
          ptr_notices_label_b_->bounds_widget.int4_width;
      ptr_label_c->b_visible = true;
    }
  } else {
    str_notices_a_ = Msg("label-local-server");
    if (auto* ptr_label_a =
            dynamic_cast<ora::ui::LabelWidget*>(ptr_notices_label_a_))
      ptr_label_a->fn_get_text = [this] { return str_notices_a_; };
    ptr_notices_label_b_->b_visible = false;
    ptr_notices_label_c_->b_visible = false;
    b_notices_b_visible_ = false;
  }
}

void ServerCreationLogic::CreateAndJoin() {
  auto& ui_singleton = ora::ui::Ui::Instance();
  map::MapCache* ptr_cache = ui_singleton.MapCacheDeps();
  if (ptr_map_ != nullptr && ptr_cache != nullptr &&
      ptr_cache->At(ptr_map_->Uid()).Status() != map::MapStatus::Available)
    return;

  auto* ptr_name_field = ptr_panel_->Get<ora::ui::TextFieldWidget>(
      "SERVER_NAME");
  str_server_name_ = SanitizedServerNameLocal(
      ptr_name_field->Text(), ui_singleton.PlayerNameDeps());
  ptr_name_field->SetText(str_server_name_);
  int4_listen_port_ = ParsePortOrFallback(
      ptr_panel_->Get<ora::ui::TextFieldWidget>("LISTEN_PORT")->Text(), 1234);

  const auto* ptr_password_field =
      ptr_panel_->GetOrNull<ora::ui::PasswordFieldWidget>("PASSWORD");
  const std::string str_password =
      ptr_password_field != nullptr ? ptr_password_field->Text()
                                    : std::string{};

  ui::ServerSettingsFace face_settings = ui_singleton.ServerSettingsGetDeps();
  face_settings.Name = str_server_name_;
  face_settings.ListenPort = int4_listen_port_;
  face_settings.AdvertiseOnline = b_advertise_online_;
  if (ptr_map_ != nullptr)
    face_settings.Map = ptr_map_->Uid();
  face_settings.Password = str_password;
  ui_singleton.ServerSettingsSetDeps(face_settings);
  ui_singleton.SettingsSaveDeps();

  int int4_error_code = 0;
  std::string str_error_message;
  net::ConnectionTarget target_endpoint;
  if (ui_singleton.CreateServerDeps(face_settings, int4_error_code,
                                    str_error_message, target_endpoint)) {
    ora::ui::Ui::Instance().CloseWindow();
    Connect(target_endpoint, str_password, fn_open_lobby_, fn_on_exit_);
    return;
  }

  std::string str_message = Msg1("dialog-server-creation-failed.prompt",
                                 "port",
                                 fluent::FluentValue{
                                     static_cast<std::int64_t>(
                                         face_settings.ListenPort)});
  if (int4_error_code == 10048) {
    str_message += "\n" + Msg("dialog-server-creation-failed.prompt-port-used");
  } else {
    str_message +=
        "\n" +
        Fluent().GetMessage(
            "dialog-server-creation-failed.prompt-error",
            std::vector<std::pair<std::string, fluent::FluentValue>>{
                {std::string{"message"},
                 fluent::FluentValue{str_error_message}},
                {std::string{"code"},
                 fluent::FluentValue{
                     static_cast<std::int64_t>(int4_error_code)}}});
  }

  ButtonPrompt("dialog-server-creation-failed.title", str_message, nullptr,
               {}, [] {}, "dialog-server-creation-failed.cancel");
}

void RegisterServerCreationLogic() {
  ora::ui::ChromeLogicRegistry::Instance().Register(
      "ServerCreationLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        std::function<void()> fn_on_exit;
        if (const auto* ptr_value =
                std::get_if<std::function<void()>>(&args_args.At("onExit")))
          fn_on_exit = *ptr_value;

        std::function<void()> fn_open_lobby;
        if (const auto* ptr_value = std::get_if<std::function<void()>>(
                &args_args.At("openLobby")))
          fn_open_lobby = *ptr_value;

        return std::make_unique<ServerCreationLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<game::ModData*>(args_args.At("modData")),
            std::move(fn_on_exit), std::move(fn_open_lobby));
      });
}

}  // namespace ora::mods

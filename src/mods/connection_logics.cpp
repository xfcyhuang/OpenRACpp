#include "mods/connection_logics.hpp"

#include "gfx/sprite_font.hpp"

#include "fluent/fluent_provider.hpp"
#include "game/mod_data.hpp"
#include "net/network_connection.hpp"
#include "net/order_manager.hpp"
#include "ui/ui.hpp"
#include "ui/widget.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/label_widget.hpp"
#include "ui/widgets/password_field_widget.hpp"
#include "ui/widgets/rgba_sprite_widget.hpp"

namespace ora::mods {
namespace {

fluent::FluentProvider& Fluent() {
  return fluent::FluentProvider::Instance();
}

std::string Msg(std::string_view str_key) {
  return Fluent().GetMessage(str_key);
}

std::string Msg1(std::string_view str_key, std::string_view str_arg_name,
                 std::string str_value) {
  return Fluent().GetMessage(
      str_key,
      std::vector<std::pair<std::string, fluent::FluentValue>>{
          {std::string{str_arg_name},
           fluent::FluentValue{std::move(str_value)}}});
}

std::optional<ServerExternalModData>& OptServerExternalMod() {
  static std::optional<ServerExternalModData> opt_mod{std::nullopt};
  return opt_mod;
}

std::function<void()>& SwitchExternalModSink() {
  static std::function<void()> fn_sink;
  return fn_sink;
}

bool IsWhitespace(char c) {
  return c == ' ' || c == '\t';
}

int ParsePortOrFallback(std::string_view sv_input, int int4_fallback) {
  std::size_t sz_begin = 0;
  std::size_t sz_end = sv_input.size();
  while (sz_begin < sz_end && IsWhitespace(sv_input[sz_begin]))
    ++sz_begin;
  while (sz_end > sz_begin && IsWhitespace(sv_input[sz_end - 1]))
    --sz_end;
  bool b_negative = false;
  if (sz_begin < sz_end && (sv_input[sz_begin] == '+' || sv_input[sz_begin] == '-')) {
    b_negative = sv_input[sz_begin] == '-';
    ++sz_begin;
  }
  if (sz_begin >= sz_end || b_negative)
    return int4_fallback;
  int int4_value = 0;
  for (std::size_t i = sz_begin; i < sz_end; ++i) {
    const char c = sv_input[i];
    if (c < '0' || c > '9')
      return int4_fallback;
    int4_value = int4_value * 10 + (c - '0');
  }
  return int4_value;
}

}  // namespace

ServerExternalModData* CurrentServerExternalMod() {
  auto& opt_mod = OptServerExternalMod();
  if (!opt_mod.has_value())
    return nullptr;
  return &*opt_mod;
}

void SetServerExternalModForTest(std::optional<ServerExternalModData> mod) {
  OptServerExternalMod() = std::move(mod);
}

void ClearServerExternalModForTest() {
  OptServerExternalMod().reset();
}

void SetSwitchExternalModSinkForTest(std::function<void()> fn_sink) {
  SwitchExternalModSink() = std::move(fn_sink);
}

void Connect(net::ConnectionTarget target_endpoint,
             const std::string& str_password,
             std::function<void()> fn_on_connect,
             std::function<void()> fn_on_abort) {
  ora::ui::Ui::Instance().JoinServerDeps(target_endpoint, str_password);

  auto endpoint_sink =
      std::make_shared<net::ConnectionTarget>(target_endpoint);
  auto fn_on_connect_sink =
      std::make_shared<std::function<void()>>(std::move(fn_on_connect));
  auto fn_on_abort_sink =
      std::make_shared<std::function<void()>>(std::move(fn_on_abort));
  const auto fn_on_retry =
      [endpoint_sink, fn_on_connect_sink, fn_on_abort_sink](
          const std::string& str_next_password) {
        Connect(*endpoint_sink, str_next_password,
                std::function<void()>(*fn_on_connect_sink),
                std::function<void()>(*fn_on_abort_sink));
      };

  ora::ui::WidgetArgs args_args;
  args_args.Add("endpoint", endpoint_sink.get());
  args_args.Add("onConnect", std::function<void()>{*fn_on_connect_sink});
  args_args.Add("onAbort", std::function<void()>{*fn_on_abort_sink});
  args_args.Add(
      "onRetry",
      std::function<void(const std::string&)>{std::move(fn_on_retry)});
  ora::ui::Ui::Instance().OpenWindow("CONNECTING_PANEL", std::move(args_args));
}

ConnectionLogic::ConnectionLogic(ora::ui::Widget& widget,
                                 game::ModData& mod_data,
                                 net::ConnectionTarget& target_endpoint,
                                 std::function<void()> fn_on_connect,
                                 std::function<void()> fn_on_abort,
                                 std::function<void(const std::string&)>
                                     fn_on_retry)
    : fn_on_connect_(std::move(fn_on_connect)),
      fn_on_abort_(std::move(fn_on_abort)),
      fn_on_retry_(std::move(fn_on_retry)),
      ptr_panel_(&widget) {
  static_cast<void>(&mod_data);
  static_cast<void>(&target_endpoint);

  str_connecting_desc_ = Msg1("label-connecting-to-endpoint", "endpoint",
                              target_endpoint.ToString());

  fn_unsubscribe_ = ora::ui::Ui::Instance().SubscribeConnectionStateChanged(
      [this](net::OrderManager* ptr_om, const std::string& str_password,
             net::NetworkConnection* ptr_connection) {
        ConnectionStateChanged(*ptr_om, str_password, *ptr_connection);
      });

  widget.Get<ora::ui::ButtonWidget>("ABORT_BUTTON")->fn_on_click = [this] {
    CloseWindow();
    if (fn_on_abort_)
      fn_on_abort_();
  };

  widget.Get<ora::ui::LabelWidget>("CONNECTING_DESC")->fn_get_text =
      [this] { return str_connecting_desc_; };
}

ConnectionLogic::~ConnectionLogic() {
  if (fn_unsubscribe_) {
    fn_unsubscribe_();
    fn_unsubscribe_ = {};
  }
}

void ConnectionLogic::ConnectionStateChanged(
    net::OrderManager& order_manager, const std::string& str_password,
    net::NetworkConnection& connection) {
  if (connection.State() == net::ConnectionState::Connected) {
    CloseWindow();
    if (fn_on_connect_)
      fn_on_connect_();
    return;
  }

  if (connection.State() != net::ConnectionState::NotConnected)
    return;

  CloseWindow();

  const std::string str_panel = CurrentServerExternalMod() != nullptr
                                    ? "CONNECTION_SWITCHMOD_PANEL"
                                    : "CONNECTIONFAILED_PANEL";
  ora::ui::WidgetArgs args_args;
  args_args.Add("orderManager", &order_manager);
  args_args.Add("connection", &connection);
  args_args.Add("password", std::string{str_password});
  if (fn_on_abort_)
    args_args.Add("onAbort", fn_on_abort_);
  if (fn_on_retry_)
    args_args.Add("onRetry", fn_on_retry_);
  ora::ui::Ui::Instance().OpenWindow(str_panel, std::move(args_args));
}

void ConnectionLogic::CloseWindow() {
  if (fn_unsubscribe_) {
    fn_unsubscribe_();
    fn_unsubscribe_ = {};
  }
  ora::ui::Ui::Instance().CloseWindow();
}

ConnectionFailedLogic::ConnectionFailedLogic(
    ora::ui::Widget& widget, game::ModData& mod_data,
    net::OrderManager& order_manager, net::NetworkConnection& connection,
    const std::string& str_password, std::function<void()> fn_on_abort,
    std::function<void()> fn_on_quit,
    std::function<void(const std::string&)> fn_on_retry)
    : ptr_order_manager_(&order_manager),
      ptr_panel_(&widget),
      str_password_(str_password) {
  static_cast<void>(&mod_data);

  ptr_abort_button_ = widget.GetOrNull<ora::ui::ButtonWidget>("ABORT_BUTTON");
  ptr_quit_button_ = widget.GetOrNull<ora::ui::ButtonWidget>("QUIT_BUTTON");
  ptr_retry_button_ = widget.GetOrNull<ora::ui::ButtonWidget>("RETRY_BUTTON");

  if (ptr_abort_button_ != nullptr) {
    ptr_abort_button_->fn_is_visible = [has_abort = fn_on_abort != nullptr] {
      return has_abort;
    };
    ptr_abort_button_->fn_is_disabled =
        [this] { return b_leaving_; };
    ptr_abort_button_->fn_on_click = [fn_on_abort = std::move(fn_on_abort)] {
      ora::ui::Ui::Instance().CloseWindow();
      if (fn_on_abort)
        fn_on_abort();
    };
  }

  if (ptr_quit_button_ != nullptr) {
    ptr_quit_button_->fn_is_visible = [has_quit = fn_on_quit != nullptr] {
      return has_quit;
    };
    ptr_quit_button_->fn_is_disabled = [this] { return b_leaving_; };
    ptr_quit_button_->fn_on_click = [this, fn_on_quit = std::move(fn_on_quit)] {
      if (fn_on_quit)
        fn_on_quit();
      b_leaving_ = true;
    };
  }

  if (ptr_retry_button_ != nullptr) {
    ptr_retry_button_->fn_is_visible = [has_retry = fn_on_retry != nullptr] {
      return has_retry;
    };
    ptr_retry_button_->fn_is_disabled = [this] { return b_leaving_; };
    ptr_retry_button_->fn_on_click = [this,
                                      fn_on_retry = std::move(fn_on_retry)] {
      const auto* ptr_password_field =
          ptr_panel_->GetOrNull<ora::ui::PasswordFieldWidget>("PASSWORD");
      const std::string str_pass =
          ptr_password_field != nullptr && ptr_password_field->fn_is_visible()
              ? ptr_password_field->Text()
              : str_password_;
      ora::ui::Ui::Instance().CloseWindow();
      str_retry_password_for_test_ = str_pass;
      if (fn_on_retry)
        fn_on_retry(str_pass);
    };
  }

  str_connection_error_ =
      order_manager.ServerError().empty()
          ? (!connection.ErrorMessage().empty()
                 ? connection.ErrorMessage()
                 : Msg("label-unknown-error"))
          : Msg(order_manager.ServerError());
  widget.GetOrNull<ora::ui::LabelWidget>("CONNECTION_ERROR")->fn_get_text =
      [this] { return str_connection_error_; };

  str_panel_title_ = order_manager.AuthenticationFailed()
                         ? Msg("label-password-required")
                         : Msg("label-connection-failed");
  if (auto* ptr_title = widget.GetOrNull<ora::ui::LabelWidget>("TITLE"))
    ptr_title->fn_get_text = [this] { return str_panel_title_; };

  if (auto* ptr_password_field =
          widget.GetOrNull<ora::ui::PasswordFieldWidget>("PASSWORD")) {
    ptr_password_field->TakeKeyboardFocus();
    ptr_password_field->fn_is_visible = [this] {
      return ptr_order_manager_->AuthenticationFailed();
    };
    if (auto* ptr_label =
            widget.GetOrNull<ora::ui::LabelWidget>("PASSWORD_LABEL"))
      ptr_label->fn_is_visible = ptr_password_field->fn_is_visible;
    ptr_password_field->fn_on_enter_key = [this](const KeyInput&) {
      if (ptr_retry_button_ != nullptr)
        ptr_retry_button_->fn_on_click();
      return true;
    };
    ptr_password_field->fn_on_esc_key = [this](const KeyInput&) {
      if (ptr_abort_button_ != nullptr)
        ptr_abort_button_->fn_on_click();
      return true;
    };
  }

  if (auto* ptr_ticker = widget.GetOrNull<ora::ui::LogicTickerWidget>(
          "CONNECTION_FAILED_TICKER")) {
    ptr_ticker_ = ptr_ticker;
    ptr_ticker_->fn_on_tick = [this] {
      auto* ptr_password_field =
          ptr_panel_->GetOrNull<ora::ui::PasswordFieldWidget>("PASSWORD");
      auto* ptr_error =
          ptr_panel_->GetOrNull<ora::ui::LabelWidget>("CONNECTION_ERROR");
      auto* ptr_background = ptr_panel_->GetOrNull("CONNECTION_BACKGROUND");
      if (ptr_password_field != nullptr && ptr_error != nullptr &&
          ptr_password_field->fn_is_visible() &&
          !b_password_offset_adjusted_) {
        const int int4_offset = ptr_password_field->bounds_widget.int4_y -
                                ptr_error->bounds_widget.int4_y;
        ptr_abort_button_->bounds_widget.int4_y += int4_offset;
        ptr_retry_button_->bounds_widget.int4_y += int4_offset;
        ptr_panel_->bounds_widget.int4_height += int4_offset;
        ptr_panel_->bounds_widget.int4_y -= int4_offset / 2;
        if (ptr_background != nullptr)
          ptr_background->bounds_widget.int4_height += int4_offset;
        b_password_offset_adjusted_ = true;
      }
    };
  }
}

ConnectionSwitchModLogic::ConnectionSwitchModLogic(
    ora::ui::Widget& widget, game::ModData& mod_data,
    net::OrderManager& order_manager, net::NetworkConnection& connection,
    std::function<void()> fn_on_abort,
    std::function<void(const std::string&)> fn_on_retry) {
  static_cast<void>(&mod_data);

  const ServerExternalModData* ptr_mod = CurrentServerExternalMod();
  const std::string str_mod_title = ptr_mod != nullptr ? ptr_mod->str_id : "";
  const std::string str_mod_version =
      ptr_mod != nullptr ? ptr_mod->str_version : "";

  ptr_abort_button_ =
      widget.GetOrNull<ora::ui::ButtonWidget>("ABORT_BUTTON");
  if (ptr_abort_button_ != nullptr) {
    ptr_abort_button_->fn_is_visible = [has_abort = fn_on_abort != nullptr] {
      return has_abort;
    };
    ptr_abort_button_->fn_on_click = [fn_on_abort = std::move(fn_on_abort)] {
      ora::ui::Ui::Instance().CloseWindow();
      if (fn_on_abort)
        fn_on_abort();
    };
  }

  ptr_switch_button_ =
      widget.GetOrNull<ora::ui::ButtonWidget>("SWITCH_BUTTON");
  if (ptr_switch_button_ != nullptr) {
    ptr_switch_button_->fn_on_click =
        [&connection, &order_manager,
         fn_on_retry = std::move(fn_on_retry)] {
          const auto* ptr_endpoint = connection.Target().FirstEndpoint();
          std::string str_launch;
          if (ptr_endpoint != nullptr)
            str_launch = std::format("Launch.URI=tcp://{}:{}",
                                     ptr_endpoint->str_host,
                                     ptr_endpoint->int4_port);
          if (SwitchExternalModSink()) {
            SwitchExternalModSink()();
            return;
          }
          order_manager.SetServerError("notification-mod-switch-failed");
          ora::ui::Ui::Instance().CloseWindow();
          ora::ui::WidgetArgs args_args;
          args_args.Add("orderManager", &order_manager);
          if (fn_on_retry)
            args_args.Add("onRetry", fn_on_retry);
          ora::ui::Ui::Instance().OpenWindow("CONNECTIONFAILED_PANEL",
                                    std::move(args_args));
        };
  }

  int int4_width = 0;
  if (auto* ptr_title =
          widget.GetOrNull<ora::ui::LabelWidget>("MOD_TITLE")) {
    auto& font = ora::ui::RequireWidgetFont(ptr_title->str_font);
    const std::string str_label = ora::ui::TruncateText(
        str_mod_title, ptr_title->bounds_widget.int4_width, font);
    int int4_label_width = font.Measure(str_label).X;
    int4_width = std::max(int4_width,
                          ptr_title->bounds_widget.int4_x + int4_label_width);
    ptr_title->bounds_widget.int4_width = int4_label_width;
    ptr_title->fn_get_text = [str_label] { return str_label; };
  }

  if (auto* ptr_version =
          widget.GetOrNull<ora::ui::LabelWidget>("MOD_VERSION")) {
    auto& font = ora::ui::RequireWidgetFont(ptr_version->str_font);
    const std::string str_label = ora::ui::TruncateText(
        str_mod_version, ptr_version->bounds_widget.int4_width, font);
    int int4_label_width = font.Measure(str_label).X;
    int4_width = std::max(
        int4_width, ptr_version->bounds_widget.int4_x + int4_label_width);
    ptr_version->bounds_widget.int4_width = int4_label_width;
    ptr_version->fn_get_text = [str_label] { return str_label; };
  }

  if (auto* ptr_logo =
          widget.GetOrNull<ora::ui::RGBASpriteWidget>("MOD_ICON")) {
    if (ptr_mod == nullptr) {
      if (auto* ptr_title =
              widget.GetOrNull<ora::ui::LabelWidget>("MOD_TITLE"))
        ptr_title->bounds_widget.int4_x = ptr_logo->bounds_widget.int4_x;
      if (auto* ptr_version =
              widget.GetOrNull<ora::ui::LabelWidget>("MOD_VERSION"))
        ptr_version->bounds_widget.int4_x = ptr_logo->bounds_widget.int4_x;
      int4_width -= ptr_logo->bounds_widget.int4_width;
    } else {
      int4_width += ptr_logo->bounds_widget.int4_width;
    }
  }

  if (auto* ptr_container = widget.GetOrNull("MOD_CONTAINER")) {
    ptr_container->bounds_widget.int4_x +=
        (ptr_container->bounds_widget.int4_width - int4_width) / 2;
    ptr_container->bounds_widget.int4_width = int4_width;
  }
}

DirectConnectLogic::DirectConnectLogic(
    ora::ui::Widget& widget, std::function<void()> fn_on_exit,
    std::function<void()> fn_open_lobby,
    net::ConnectionTarget* ptr_direct_connect_endpoint) {
  ptr_ip_field_ = widget.Get<ora::ui::TextFieldWidget>("IP");
  ptr_port_field_ = widget.Get<ora::ui::TextFieldWidget>("PORT");

  const std::string str_last_server = ora::ui::Ui::Instance().PlayerLastServerGetDeps();
  const std::size_t sz_last = str_last_server.rfind(':');
  if (sz_last == std::string::npos) {
    ptr_ip_field_->SetText("localhost");
    ptr_port_field_->SetText("1234");
  } else {
    ptr_ip_field_->SetText(str_last_server.substr(0, sz_last));
    ptr_port_field_->SetText(str_last_server.substr(sz_last + 1));
  }

  ptr_join_button_ = widget.GetOrNull<ora::ui::ButtonWidget>("JOIN_BUTTON");
  if (ptr_join_button_ != nullptr) {
    ptr_join_button_->fn_is_disabled = [this] {
      return ptr_ip_field_->Text().empty();
    };
    ptr_join_button_->fn_on_click =
        [this, fn_open_lobby = std::move(fn_open_lobby)] {
          const int int4_port =
              ParsePortOrFallback(ptr_port_field_->Text(), 1234);
          const std::string str_server =
              std::format("{}:{}", ptr_ip_field_->Text(), int4_port);
          ora::ui::Ui::Instance().PlayerLastServerSetDeps(str_server);
          str_joined_last_server_for_test_ = str_server;
          ora::ui::Ui::Instance().SettingsSaveDeps();

          net::ConnectionTarget target_endpoint{ptr_ip_field_->Text(),
                                                int4_port};
          auto fn_on_connected = [] {
            ora::ui::Ui::Instance().CloseWindow();
          };
          Connect(target_endpoint, "",
                  [fn_open_lobby = std::move(fn_open_lobby),
                   fn_on_connected = std::move(fn_on_connected)] {
                    fn_on_connected();
                    if (fn_open_lobby)
                      fn_open_lobby();
                  },
                  [] {});
        };
  }

  ptr_back_button_ = widget.GetOrNull<ora::ui::ButtonWidget>("BACK_BUTTON");
  if (ptr_back_button_ != nullptr) {
    ptr_back_button_->fn_on_click =
        [fn_on_exit = std::move(fn_on_exit)] {
          ora::ui::Ui::Instance().CloseWindow();
          if (fn_on_exit)
            fn_on_exit();
        };
  }

  if (ptr_direct_connect_endpoint != nullptr) {
    widget.b_visible = false;
    ptr_alive_ = std::make_shared<bool>(true);
    const auto ptr_endpoint = std::make_shared<net::ConnectionTarget>(
        *ptr_direct_connect_endpoint);
    const std::weak_ptr<bool> weak_alive = ptr_alive_;
    ora::ui::Ui::Instance().RunAfterTickDefer(
        [weak_alive, ptr_widget = &widget, ptr_endpoint,
         fn_open_lobby = std::move(fn_open_lobby)] {
          if (!weak_alive.expired())
            Connect(*ptr_endpoint, "",
                    [fn_open_lobby] {
                      ora::ui::Ui::Instance().CloseWindow();
                      if (fn_open_lobby)
                        fn_open_lobby();
                    },
                    [] {});
          ptr_widget->b_visible = true;
        });
  }
}

void RegisterConnectionLogics() {
  auto& registry_logics = ora::ui::ChromeLogicRegistry::Instance();

  registry_logics.Register(
      "ConnectionLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        const auto* ptr_endpoint = std::get_if<net::ConnectionTarget*>(
            &args_args.At("endpoint"));
        if (ptr_endpoint == nullptr)
          throw std::runtime_error(
              "The given key was not present in the dictionary.");

        std::function<void()> fn_on_connect;
        if (const auto* ptr_value =
                std::get_if<std::function<void()>>(&args_args.At("onConnect")))
          fn_on_connect = *ptr_value;

        std::function<void()> fn_on_abort;
        if (const auto* ptr_value =
                std::get_if<std::function<void()>>(&args_args.At("onAbort")))
          fn_on_abort = *ptr_value;

        std::function<void(const std::string&)> fn_on_retry;
        if (const auto* ptr_value = std::get_if<
                std::function<void(const std::string&)>>(
                &args_args.At("onRetry")))
          fn_on_retry = *ptr_value;

        return std::make_unique<ConnectionLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<game::ModData*>(args_args.At("modData")), **ptr_endpoint,
            std::move(fn_on_connect), std::move(fn_on_abort),
            std::move(fn_on_retry));
      });

  registry_logics.Register(
      "ConnectionFailedLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        net::OrderManager* ptr_order_manager = nullptr;
        if (const auto* ptr_value = std::get_if<net::OrderManager*>(
                &args_args.At("orderManager")))
          ptr_order_manager = *ptr_value;

        net::NetworkConnection* ptr_connection = nullptr;
        if (const auto* ptr_value = std::get_if<net::NetworkConnection*>(
                &args_args.At("connection")))
          ptr_connection = *ptr_value;

        std::string str_password;
        if (const auto* ptr_value = std::get_if<const std::string*>(
                &args_args.At("password"))) {
          if (*ptr_value != nullptr)
            str_password = **ptr_value;
        } else if (const auto* ptr_str = std::get_if<std::string>(
                       &args_args.At("password"))) {
          str_password = *ptr_str;
        }

        std::function<void()> fn_on_abort;
        if (const auto* ptr_value =
                std::get_if<std::function<void()>>(&args_args.At("onAbort")))
          fn_on_abort = *ptr_value;

        std::function<void()> fn_on_quit;
        if (const auto* ptr_value =
                std::get_if<std::function<void()>>(&args_args.At("onQuit")))
          fn_on_quit = *ptr_value;

        std::function<void(const std::string&)> fn_on_retry;
        if (const auto* ptr_value = std::get_if<
                std::function<void(const std::string&)>>(
                &args_args.At("onRetry")))
          fn_on_retry = *ptr_value;

        return std::make_unique<ConnectionFailedLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<game::ModData*>(args_args.At("modData")),
            *ptr_order_manager, *ptr_connection, str_password,
            std::move(fn_on_abort), std::move(fn_on_quit),
            std::move(fn_on_retry));
      });

  registry_logics.Register(
      "ConnectionSwitchModLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        net::OrderManager* ptr_order_manager = nullptr;
        if (const auto* ptr_value = std::get_if<net::OrderManager*>(
                &args_args.At("orderManager")))
          ptr_order_manager = *ptr_value;

        net::NetworkConnection* ptr_connection = nullptr;
        if (const auto* ptr_value = std::get_if<net::NetworkConnection*>(
                &args_args.At("connection")))
          ptr_connection = *ptr_value;

        std::function<void()> fn_on_abort;
        if (const auto* ptr_value =
                std::get_if<std::function<void()>>(&args_args.At("onAbort")))
          fn_on_abort = *ptr_value;

        std::function<void(const std::string&)> fn_on_retry;
        if (const auto* ptr_value = std::get_if<
                std::function<void(const std::string&)>>(
                &args_args.At("onRetry")))
          fn_on_retry = *ptr_value;

        return std::make_unique<ConnectionSwitchModLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<game::ModData*>(args_args.At("modData")),
            *ptr_order_manager, *ptr_connection, std::move(fn_on_abort),
            std::move(fn_on_retry));
      });

  registry_logics.Register(
      "DirectConnectLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        std::function<void()> fn_on_exit;
        if (const auto* ptr_value =
                std::get_if<std::function<void()>>(&args_args.At("onExit")))
          fn_on_exit = *ptr_value;

        std::function<void()> fn_open_lobby;
        if (const auto* ptr_value =
                std::get_if<std::function<void()>>(&args_args.At("openLobby")))
          fn_open_lobby = *ptr_value;

        net::ConnectionTarget* ptr_endpoint = nullptr;
        if (const auto* ptr_value = std::get_if<net::ConnectionTarget*>(
                &args_args.At("directConnectEndPoint")))
          ptr_endpoint = *ptr_value;

        return std::make_unique<DirectConnectLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            std::move(fn_on_exit), std::move(fn_open_lobby), ptr_endpoint);
      });
}

}  // namespace ora::mods

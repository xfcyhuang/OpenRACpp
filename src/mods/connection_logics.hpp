#pragma once
import std;

#include "net/connection_target.hpp"
#include "ui/widget.hpp"
#include "ui/widgets/button_widget.hpp"
#include "ui/widgets/logic_ticker_widget.hpp"
#include "ui/widgets/text_field_widget.hpp"

namespace ora::game {
class ModData;
}
namespace ora::net {
class NetworkConnection;
class OrderManager;
}

namespace ora::mods {

struct ServerExternalModData {
  std::string str_id;
  std::string str_version;
};

class ConnectionLogic final : public ora::ui::ChromeLogic {
 public:
  ConnectionLogic(ora::ui::Widget& widget, game::ModData& mod_data,
                  net::ConnectionTarget& target_endpoint,
                  std::function<void()> fn_on_connect,
                  std::function<void()> fn_on_abort,
                  std::function<void(const std::string&)> fn_on_retry);
  ~ConnectionLogic() override;

  void ConnectionStateChanged(net::OrderManager& order_manager,
                              const std::string& str_password,
                              net::NetworkConnection& connection);

  void CloseWindow();

  void RaiseConnectionStateChangedForTest(net::OrderManager& order_manager,
                                          const std::string& str_password,
                                          net::NetworkConnection& connection) {
    ConnectionStateChanged(order_manager, str_password, connection);
  }
  const std::string& ConnectingDescForTest() const {
    return str_connecting_desc_;
  }

 private:
  std::function<void()> fn_on_connect_;
  std::function<void()> fn_on_abort_;
  std::function<void(const std::string&)> fn_on_retry_;
  ora::ui::Widget* ptr_panel_ = nullptr;
  std::string str_connecting_desc_;
  std::function<void()> fn_unsubscribe_;
};

void Connect(net::ConnectionTarget target_endpoint,
             const std::string& str_password,
             std::function<void()> fn_on_connect,
             std::function<void()> fn_on_abort);

class ConnectionFailedLogic final : public ora::ui::ChromeLogic {
 public:
  ConnectionFailedLogic(ora::ui::Widget& widget, game::ModData& mod_data,
                        net::OrderManager& order_manager,
                        net::NetworkConnection& connection,
                        const std::string& str_password,
                        std::function<void()> fn_on_abort,
                        std::function<void()> fn_on_quit,
                        std::function<void(const std::string&)> fn_on_retry);

  void RetryClickedForTest() {
    if (ptr_retry_button_ != nullptr && ptr_retry_button_->fn_on_click)
      ptr_retry_button_->fn_on_click();
  }
  void AbortClickedForTest() {
    if (ptr_abort_button_ != nullptr && ptr_abort_button_->fn_on_click)
      ptr_abort_button_->fn_on_click();
  }
  void QuitClickedForTest() {
    if (ptr_quit_button_ != nullptr && ptr_quit_button_->fn_on_click)
      ptr_quit_button_->fn_on_click();
  }
  void TickerTickForTest() {
    if (ptr_ticker_ != nullptr && ptr_ticker_->fn_on_tick)
      ptr_ticker_->fn_on_tick();
  }
  const std::string& ErrorTextForTest() const {
    return str_connection_error_;
  }
  const std::string& TitleTextForTest() const { return str_panel_title_; }
  const std::string& RetryPasswordForTest() const {
    return str_retry_password_for_test_;
  }

 private:
  net::OrderManager* ptr_order_manager_ = nullptr;
  ora::ui::Widget* ptr_panel_ = nullptr;
  ora::ui::ButtonWidget* ptr_abort_button_ = nullptr;
  ora::ui::ButtonWidget* ptr_quit_button_ = nullptr;
  ora::ui::ButtonWidget* ptr_retry_button_ = nullptr;
  ora::ui::LogicTickerWidget* ptr_ticker_ = nullptr;
  bool b_leaving_ = false;
  bool b_password_offset_adjusted_ = false;
  std::string str_password_;
  std::string str_connection_error_;
  std::string str_panel_title_;
  std::string str_retry_password_for_test_;
};

ServerExternalModData* CurrentServerExternalMod();
void SetServerExternalModForTest(std::optional<ServerExternalModData> mod);
void ClearServerExternalModForTest();
void SetSwitchExternalModSinkForTest(std::function<void()> fn_sink);

class ConnectionSwitchModLogic final : public ora::ui::ChromeLogic {
 public:
  ConnectionSwitchModLogic(ora::ui::Widget& widget, game::ModData& mod_data,
                           net::OrderManager& order_manager,
                           net::NetworkConnection& connection,
                           std::function<void()> fn_on_abort,
                           std::function<void(const std::string&)> fn_on_retry);

  void SwitchClickedForTest() {
    if (ptr_switch_button_ != nullptr)
      ptr_switch_button_->fn_on_click();
  }
  void AbortClickedForTest() {
    if (ptr_abort_button_ != nullptr && ptr_abort_button_->fn_on_click)
      ptr_abort_button_->fn_on_click();
  }

 private:
  ora::ui::ButtonWidget* ptr_switch_button_ = nullptr;
  ora::ui::ButtonWidget* ptr_abort_button_ = nullptr;
};

class DirectConnectLogic final : public ora::ui::ChromeLogic {
 public:
  DirectConnectLogic(ora::ui::Widget& widget,
                     std::function<void()> fn_on_exit,
                     std::function<void()> fn_open_lobby,
                     net::ConnectionTarget* ptr_direct_connect_endpoint);
  ~DirectConnectLogic() override {
    if (ptr_alive_ != nullptr)
      *ptr_alive_ = false;
  }

  void JoinClickedForTest() {
    if (ptr_join_button_ != nullptr)
      ptr_join_button_->fn_on_click();
  }
  void BackClickedForTest() {
    if (ptr_back_button_ != nullptr)
      ptr_back_button_->fn_on_click();
  }
  const std::string& JoinedLastServerForTest() const {
    return str_joined_last_server_for_test_;
  }

 private:
  ora::ui::TextFieldWidget* ptr_ip_field_ = nullptr;
  ora::ui::TextFieldWidget* ptr_port_field_ = nullptr;
  ora::ui::ButtonWidget* ptr_join_button_ = nullptr;
  ora::ui::ButtonWidget* ptr_back_button_ = nullptr;
  std::shared_ptr<bool> ptr_alive_;
  std::string str_joined_last_server_for_test_;
};

void RegisterConnectionLogics();

}

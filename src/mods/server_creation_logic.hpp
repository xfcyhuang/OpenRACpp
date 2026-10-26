// UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/ServerCreationLogic.cs
//          @1f594e320 L20-256 全文(NAT 公告三标签方程 + CreateAndJoin 的
//          设置存盘与 Game.CreateServer/SocketException 10048 分流)。
//          The whole of ServerCreationLogic.cs L20-256 (the NAT-notice
//          three-label layout equations + CreateAndJoin's settings save and
//          the Game.CreateServer/SocketException-10048 split).
#pragma once
import std;

#include "ui/widget.hpp"

namespace ora::game {
class ModData;
}
namespace ora::map {
class MapPreview;
}

namespace ora::mods {

class ServerCreationLogic final : public ora::ui::ChromeLogic {
 public:
  ServerCreationLogic(ora::ui::Widget& widget, game::ModData& mod_data,
                      std::function<void()> fn_on_exit,
                      std::function<void()> fn_open_lobby);
  ~ServerCreationLogic() override;

  void CreateClickedForTest();
  void AdvertiseClickedForTest();
  void BackClickedForTest();

  const std::string& NoticesLabelAForTest() const {
    return str_notices_a_;
  }
  const std::string& NoticesLabelBForTest() const {
    return str_notices_b_;
  }
  bool NoticesLabelBVisibleForTest() const { return b_notices_b_visible_; }
  map::MapPreview* SelectedMapForTest() const { return ptr_map_; }
  const std::string& ServerNameForTest() const { return str_server_name_; }
  int ListenPortForTest() const { return int4_listen_port_; }
  bool AdvertiseOnlineForTest() const { return b_advertise_online_; }

 private:
  void BuildNotices();
  void CreateAndJoin();

  ora::ui::Widget* ptr_panel_ = nullptr;
  std::function<void()> fn_on_exit_;
  std::function<void()> fn_open_lobby_;
  ora::ui::Widget* ptr_notices_label_a_ = nullptr;
  ora::ui::Widget* ptr_notices_label_b_ = nullptr;
  ora::ui::Widget* ptr_notices_label_c_ = nullptr;
  map::MapPreview* ptr_map_ = nullptr;
  std::string str_notices_a_;
  std::string str_notices_b_;
  bool b_notices_b_visible_ = false;
  std::string str_server_name_;
  int int4_listen_port_ = 1234;
  bool b_advertise_online_ = false;
};

void RegisterServerCreationLogic();

}  // namespace ora::mods

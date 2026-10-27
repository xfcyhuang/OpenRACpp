#include "server/lobby_option.hpp"

namespace ora::server {

LobbyOptionData MakeLobbyBooleanOption(std::string_view str_id,
                                       std::string_view str_name,
                                       std::string_view str_description,
                                       bool b_visible, int int4_display_order,
                                       bool b_default_value, bool b_locked) {
  LobbyOptionData opt{};
  opt.Id = std::string{str_id};

  opt.Name = std::string{str_name};

  std::string str_desc{str_description};
  for (auto pos = str_desc.find("\\n"); pos != std::string::npos;
       pos = str_desc.find("\\n", pos + 1))
    str_desc.replace(pos, 2, "\n");
  opt.Description = std::move(str_desc);
  opt.IsVisible = b_visible;
  opt.DisplayOrder = int4_display_order;
  opt.vec_values = {{"True", "Enabled"}, {"False", "Disabled"}};
  opt.DefaultValue = b_default_value ? "True" : "False";
  opt.IsLocked = b_locked;
  opt.IsBooleanOption = true;
  return opt;
}

}

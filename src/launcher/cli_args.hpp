#pragma once

import std;

namespace ora::launcher {

struct CliArgs {
  std::string str_mod{"ra"};
  std::string str_map;
  std::int32_t int4_seed{0};
  std::int32_t int4_tick_ms{40};
  bool b_fullscreen{};
  std::string str_content_dir;
  std::string str_log_file;
  bool b_headless{};
  std::int32_t int4_headless_ticks{600};
  bool b_demo{};
  std::string str_record;   // T16：对局录制为 .orarep（退出前 Dispose 落盘）
  std::string str_replay;   // T16：重演 .orarep（headless；与 --record 互斥）
};

struct CliParseResult {
  bool b_ok{true};
  bool b_help{};
  bool b_version{};
  CliArgs args;
  std::vector<std::string> vec_errors;
};

[[nodiscard]] CliParseResult cli_args_parse(std::span<const std::string_view> vec_args);

[[nodiscard]] std::string cli_help_text();

}  // namespace ora::launcher

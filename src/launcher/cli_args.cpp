#include "launcher/cli_args.hpp"

namespace ora::launcher {

namespace {

std::optional<std::int32_t> parse_int(std::string_view str_value) {
  std::int32_t int4_out{};
  const char* ptr_first{str_value.data()};
  const char* ptr_last{str_value.data() + str_value.size()};
  const auto res{std::from_chars(ptr_first, ptr_last, int4_out)};
  if (res.ec != std::errc{} || res.ptr != ptr_last) return std::nullopt;
  return int4_out;
}

struct FlagSpec {
  std::string_view str_flag;
  bool b_takes_value{};
};

constexpr std::array<FlagSpec, 16> kFlags{{
    FlagSpec{"--mod", true},
    FlagSpec{"--map", true},
    FlagSpec{"--seed", true},
    FlagSpec{"--tick-ms", true},
    FlagSpec{"--content-dir", true},
    FlagSpec{"--log-file", true},
    FlagSpec{"--ticks", true},
    FlagSpec{"--record", true},
    FlagSpec{"--replay", true},
    FlagSpec{"--fullscreen", false},
    FlagSpec{"--headless", false},
    FlagSpec{"--demo", false},
    FlagSpec{"--help", false},
    FlagSpec{"--lobby", false},
    FlagSpec{"--maps-dir", true},
    FlagSpec{"--chrome-dir", true},
}};

}

CliParseResult cli_args_parse(std::span<const std::string_view> vec_args) {
  CliParseResult res;
  CliArgs& args{res.args};

  std::size_t idx{0};
  while (idx < vec_args.size()) {
    const std::string_view str_arg{vec_args[idx]};

    if (str_arg == "--version") {
      res.b_version = true;
      ++idx;
      continue;
    }

    const FlagSpec* ptr_flag{nullptr};
    for (const FlagSpec& flag : kFlags)
      if (flag.str_flag == str_arg) ptr_flag = &flag;
    if (ptr_flag == nullptr) {
      res.vec_errors.push_back(std::format("unrecognized argument '{}'", str_arg));
      ++idx;
      continue;
    }

    if (str_arg == "--help") {
      res.b_help = true;
      ++idx;
      continue;
    }
    if (!ptr_flag->b_takes_value) {
      if (str_arg == "--fullscreen") args.b_fullscreen = true;
      if (str_arg == "--headless") args.b_headless = true;
      if (str_arg == "--demo") args.b_demo = true;
      if (str_arg == "--lobby") args.b_lobby = true;
      ++idx;
      continue;
    }

    if (idx + 1 >= vec_args.size()) {
      res.vec_errors.push_back(std::format("missing value for '{}'", str_arg));
      ++idx;
      continue;
    }
    const std::string_view str_value{vec_args[idx + 1]};
    idx += 2;

    if (str_arg == "--mod") {
      args.str_mod = std::string{str_value};
      if (args.str_mod != "ra")
        res.vec_errors.push_back(std::format("unsupported mod '{}' (M1: ra only)", str_value));
    } else if (str_arg == "--map") {
      args.str_map = std::string{str_value};
    } else if (str_arg == "--content-dir") {
      args.str_content_dir = std::string{str_value};
    } else if (str_arg == "--log-file") {
      args.str_log_file = std::string{str_value};
    } else if (str_arg == "--record") {
      args.str_record = std::string{str_value};
    } else if (str_arg == "--replay") {
      args.str_replay = std::string{str_value};
    } else if (str_arg == "--maps-dir") {
      args.str_maps_dir = std::string{str_value};
    } else if (str_arg == "--chrome-dir") {
      args.str_chrome_dir = std::string{str_value};
    } else if (str_arg == "--seed") {
      const auto opt{parse_int(str_value)};
      if (!opt.has_value()) {
        res.vec_errors.push_back(std::format("invalid seed '{}'", str_value));
      } else {
        args.int4_seed = *opt;
      }
    } else if (str_arg == "--tick-ms") {
      const auto opt{parse_int(str_value)};
      if (!opt.has_value() || *opt <= 0) {
        res.vec_errors.push_back(std::format("invalid tick-ms '{}'", str_value));
      } else {
        args.int4_tick_ms = *opt;
      }
    } else if (str_arg == "--ticks") {
      const auto opt{parse_int(str_value)};
      if (!opt.has_value() || *opt < 0) {
        res.vec_errors.push_back(std::format("invalid ticks '{}'", str_value));
      } else {
        args.int4_headless_ticks = *opt;
      }
    }
  }

  if (!args.str_record.empty() && !args.str_replay.empty())
    res.vec_errors.push_back("--record 与 --replay 互斥");

  res.b_ok = res.vec_errors.empty();
  return res;
}

std::string cli_help_text() {
  return std::string{
      "ora - OpenRACpp M1 sandbox game launcher\n"
      "usage: ora [--mod ra] [--map <path.oramap>] [--seed N] [--tick-ms 40]\n"
      "           [--fullscreen] [--content-dir <path>] [--log-file <path>]\n"
      "           [--headless --ticks N] [--record <path.orarep>] [--replay <path.orarep>]\n"
      "           [--help] [--version]\n"
      "defaults: mod=ra map=built-in sandbox seed=0 tick-ms=40 windowed\n"};
}

}

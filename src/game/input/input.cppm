export module ora.game.input;

import std;
import ora.game;
import ora.game.miniyaml;
import ora.game.world;
import ora.game.pathfinding;

export {
namespace ora::game::input {

enum class MouseButton : std::uint8_t {
  kNone = 0,
  kLeft = 1,
  kMiddle = 2,
  kRight = 3,
};

enum class MouseInputEvent : std::uint8_t {
  kNone = 0,
  kDown,
  kUp,
  kMove,
};

// 位值对齐上游 IInputHandler.cs:38-45（Shift=1/Alt=2/Ctrl=4/Meta=8）。
// 位值只用于内部合成；黄金/序列化一律走 canonical 字符串（hotkey_to_string）。
enum class ModifierKeys : std::uint8_t {
  kNone = 0,
  kShift = 1,
  kAlt = 2,
  kCtrl = 4,
  kMeta = 8,
};

[[nodiscard]] constexpr ModifierKeys operator&(ModifierKeys a, ModifierKeys b) {
  return static_cast<ModifierKeys>(static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
}

[[nodiscard]] constexpr ModifierKeys operator|(ModifierKeys a, ModifierKeys b) {
  return static_cast<ModifierKeys>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

constexpr ModifierKeys& operator|=(ModifierKeys& a, ModifierKeys b) { a = a | b; return a; }

[[nodiscard]] constexpr bool has_modifier(ModifierKeys mods, ModifierKeys mod) {
  return (mods & mod) != ModifierKeys::kNone;
}

// SDL 2.0.1 keycode 全表照抄（上游 OpenRA.Game/Input/Keycode.cs，238 项，
// 含扩展键 N|(1<<30) 与 MOUSE4/MOUSE5）；数值与 SDL_Keycode 天然一致。
// 枚举值到 canonical 键名（上游 C# 枚举名）的映射见 hotkey.cpp 的 kKeycodeNames。
enum class Keycode : std::uint32_t {
  kUnknown = 0,
  kReturn = '\r',
  kEscape = 27,
  kBackspace = '\b',
  kTab = '\t',
  kSpace = ' ',
  kExclaim = '!',
  kQuotedbl = '"',
  kHash = '#',
  kPercent = '%',
  kDollar = '$',
  kAmpersand = '&',
  kQuote = '\'',
  kLeftparen = '(',
  kRightparen = ')',
  kAsterisk = '*',
  kPlus = '+',
  kComma = ',',
  kMinus = '-',
  kPeriod = '.',
  kSlash = '/',
  kNumber0 = '0',
  kNumber1 = '1',
  kNumber2 = '2',
  kNumber3 = '3',
  kNumber4 = '4',
  kNumber5 = '5',
  kNumber6 = '6',
  kNumber7 = '7',
  kNumber8 = '8',
  kNumber9 = '9',
  kColon = ':',
  kSemicolon = ';',
  kLess = '<',
  kEquals = '=',
  kGreater = '>',
  kQuestion = '?',
  kAt = '@',
  kLeftbracket = '[',
  kBackslash = '\\',
  kRightbracket = ']',
  kCaret = '^',
  kUnderscore = '_',
  kBackquote = '`',
  kA = 'a',
  kB = 'b',
  kC = 'c',
  kD = 'd',
  kE = 'e',
  kF = 'f',
  kG = 'g',
  kH = 'h',
  kI = 'i',
  kJ = 'j',
  kK = 'k',
  kL = 'l',
  kM = 'm',
  kN = 'n',
  kO = 'o',
  kP = 'p',
  kQ = 'q',
  kR = 'r',
  kS = 's',
  kT = 't',
  kU = 'u',
  kV = 'v',
  kW = 'w',
  kX = 'x',
  kY = 'y',
  kZ = 'z',
  kCapslock = 57 | (1 << 30),
  kF1 = 58 | (1 << 30),
  kF2 = 59 | (1 << 30),
  kF3 = 60 | (1 << 30),
  kF4 = 61 | (1 << 30),
  kF5 = 62 | (1 << 30),
  kF6 = 63 | (1 << 30),
  kF7 = 64 | (1 << 30),
  kF8 = 65 | (1 << 30),
  kF9 = 66 | (1 << 30),
  kF10 = 67 | (1 << 30),
  kF11 = 68 | (1 << 30),
  kF12 = 69 | (1 << 30),
  kPrintscreen = 70 | (1 << 30),
  kScrolllock = 71 | (1 << 30),
  kPause = 72 | (1 << 30),
  kInsert = 73 | (1 << 30),
  kHome = 74 | (1 << 30),
  kPageup = 75 | (1 << 30),
  kDelete = 127,
  kEnd = 77 | (1 << 30),
  kPagedown = 78 | (1 << 30),
  kRight = 79 | (1 << 30),
  kLeft = 80 | (1 << 30),
  kDown = 81 | (1 << 30),
  kUp = 82 | (1 << 30),
  kNumlockclear = 83 | (1 << 30),
  kKpDivide = 84 | (1 << 30),
  kKpMultiply = 85 | (1 << 30),
  kKpMinus = 86 | (1 << 30),
  kKpPlus = 87 | (1 << 30),
  kKpEnter = 88 | (1 << 30),
  kKp1 = 89 | (1 << 30),
  kKp2 = 90 | (1 << 30),
  kKp3 = 91 | (1 << 30),
  kKp4 = 92 | (1 << 30),
  kKp5 = 93 | (1 << 30),
  kKp6 = 94 | (1 << 30),
  kKp7 = 95 | (1 << 30),
  kKp8 = 96 | (1 << 30),
  kKp9 = 97 | (1 << 30),
  kKp0 = 98 | (1 << 30),
  kKpPeriod = 99 | (1 << 30),
  kApplication = 101 | (1 << 30),
  kPower = 102 | (1 << 30),
  kKpEquals = 103 | (1 << 30),
  kF13 = 104 | (1 << 30),
  kF14 = 105 | (1 << 30),
  kF15 = 106 | (1 << 30),
  kF16 = 107 | (1 << 30),
  kF17 = 108 | (1 << 30),
  kF18 = 109 | (1 << 30),
  kF19 = 110 | (1 << 30),
  kF20 = 111 | (1 << 30),
  kF21 = 112 | (1 << 30),
  kF22 = 113 | (1 << 30),
  kF23 = 114 | (1 << 30),
  kF24 = 115 | (1 << 30),
  kExecute = 116 | (1 << 30),
  kHelp = 117 | (1 << 30),
  kMenu = 118 | (1 << 30),
  kSelect = 119 | (1 << 30),
  kStop = 120 | (1 << 30),
  kAgain = 121 | (1 << 30),
  kUndo = 122 | (1 << 30),
  kCut = 123 | (1 << 30),
  kCopy = 124 | (1 << 30),
  kPaste = 125 | (1 << 30),
  kFind = 126 | (1 << 30),
  kMute = 127 | (1 << 30),
  kVolumeup = 128 | (1 << 30),
  kVolumedown = 129 | (1 << 30),
  kKpComma = 133 | (1 << 30),
  kKpEqualsas400 = 134 | (1 << 30),
  kAlterase = 153 | (1 << 30),
  kSysreq = 154 | (1 << 30),
  kCancel = 155 | (1 << 30),
  kClear = 156 | (1 << 30),
  kPrior = 157 | (1 << 30),
  kReturn2 = 158 | (1 << 30),
  kSeparator = 159 | (1 << 30),
  kOut = 160 | (1 << 30),
  kOper = 161 | (1 << 30),
  kClearagain = 162 | (1 << 30),
  kCrsel = 163 | (1 << 30),
  kExsel = 164 | (1 << 30),
  kKp00 = 176 | (1 << 30),
  kKp000 = 177 | (1 << 30),
  kThousandsseparator = 178 | (1 << 30),
  kDecimalseparator = 179 | (1 << 30),
  kCurrencyunit = 180 | (1 << 30),
  kCurrencysubunit = 181 | (1 << 30),
  kKpLeftparen = 182 | (1 << 30),
  kKpRightparen = 183 | (1 << 30),
  kKpLeftbrace = 184 | (1 << 30),
  kKpRightbrace = 185 | (1 << 30),
  kKpTab = 186 | (1 << 30),
  kKpBackspace = 187 | (1 << 30),
  kKpA = 188 | (1 << 30),
  kKpB = 189 | (1 << 30),
  kKpC = 190 | (1 << 30),
  kKpD = 191 | (1 << 30),
  kKpE = 192 | (1 << 30),
  kKpF = 193 | (1 << 30),
  kKpXor = 194 | (1 << 30),
  kKpPower = 195 | (1 << 30),
  kKpPercent = 196 | (1 << 30),
  kKpLess = 197 | (1 << 30),
  kKpGreater = 198 | (1 << 30),
  kKpAmpersand = 199 | (1 << 30),
  kKpDblampersand = 200 | (1 << 30),
  kKpVerticalbar = 201 | (1 << 30),
  kKpDblverticalbar = 202 | (1 << 30),
  kKpColon = 203 | (1 << 30),
  kKpHash = 204 | (1 << 30),
  kKpSpace = 205 | (1 << 30),
  kKpAt = 206 | (1 << 30),
  kKpExclam = 207 | (1 << 30),
  kKpMemstore = 208 | (1 << 30),
  kKpMemrecall = 209 | (1 << 30),
  kKpMemclear = 210 | (1 << 30),
  kKpMemadd = 211 | (1 << 30),
  kKpMemsubtract = 212 | (1 << 30),
  kKpMemmultiply = 213 | (1 << 30),
  kKpMemdivide = 214 | (1 << 30),
  kKpPlusminus = 215 | (1 << 30),
  kKpClear = 216 | (1 << 30),
  kKpClearentry = 217 | (1 << 30),
  kKpBinary = 218 | (1 << 30),
  kKpOctal = 219 | (1 << 30),
  kKpDecimal = 220 | (1 << 30),
  kKpHexadecimal = 221 | (1 << 30),
  kLctrl = 224 | (1 << 30),
  kLshift = 225 | (1 << 30),
  kLalt = 226 | (1 << 30),
  kLgui = 227 | (1 << 30),
  kRctrl = 228 | (1 << 30),
  kRshift = 229 | (1 << 30),
  kRalt = 230 | (1 << 30),
  kRgui = 231 | (1 << 30),
  kMode = 257 | (1 << 30),
  kAudionext = 258 | (1 << 30),
  kAudioprev = 259 | (1 << 30),
  kAudiostop = 260 | (1 << 30),
  kAudioplay = 261 | (1 << 30),
  kAudiomute = 262 | (1 << 30),
  kMediaselect = 263 | (1 << 30),
  kWww = 264 | (1 << 30),
  kMail = 265 | (1 << 30),
  kCalculator = 266 | (1 << 30),
  kComputer = 267 | (1 << 30),
  kAcSearch = 268 | (1 << 30),
  kAcHome = 269 | (1 << 30),
  kAcBack = 270 | (1 << 30),
  kAcForward = 271 | (1 << 30),
  kAcStop = 272 | (1 << 30),
  kAcRefresh = 273 | (1 << 30),
  kAcBookmarks = 274 | (1 << 30),
  kBrightnessdown = 275 | (1 << 30),
  kBrightnessup = 276 | (1 << 30),
  kDisplayswitch = 277 | (1 << 30),
  kKbdillumtoggle = 278 | (1 << 30),
  kKbdillumdown = 279 | (1 << 30),
  kKbdillumup = 280 | (1 << 30),
  kEject = 281 | (1 << 30),
  kSleep = 282 | (1 << 30),
  kMouse4 = 283 | (1 << 30),
  kMouse5 = 284 | (1 << 30),
};

enum class KeyInputEvent : std::uint8_t {
  kNone = 0,
  kDown,
  kUp,
};

struct KeyInput {
  KeyInputEvent event{};
  Keycode key{Keycode::kUnknown};
  ModifierKeys mods{};
  std::int32_t int4_tap_count{1};
  char32_t c4_unicode{};
  bool b_is_repeat{};
};

struct MouseInput {
  MouseInputEvent event{};
  MouseButton button{};
  std::int32_t int4_x{};
  std::int32_t int4_y{};
  std::int32_t int4_delta_x{};
  std::int32_t int4_delta_y{};
  ModifierKeys mods{};
  std::int32_t int4_scroll_y{};
  std::int32_t int4_tap_count{1};
};

}

namespace ora::game::input {

// 上游 OpenRA.Platforms.Default/MultiTapDetection.cs 的三槽移位寄存器
// （TapHistory，:19-83）。墙钟语义：Δt<250ms（相邻对链判）且位移欧氏长度 <4
// （C++ 整型实现 = dx*dx+dy*dy < 16）。时钟注入式，无头可测。
struct TapSlot {
  std::int64_t int8_time_ms{};
  std::int32_t int4_x{};
  std::int32_t int4_y{};
};

struct TapHistoryState {
  TapSlot slot_first{};
  TapSlot slot_second{};
  TapSlot slot_third{};
};

// 上游桶初始化 = (now-1s, Zero)：首击 Third-Second 判"不近"→ count=1。
void tap_history_init(TapHistoryState& st, std::int64_t int8_now_ms);

// 上游 GetTapCount（:60-72，Down 事件调）：先移位再判。
[[nodiscard]] std::int32_t tap_history_detect(TapHistoryState& st, std::int64_t int8_now_ms,
                                              std::int32_t int4_x, std::int32_t int4_y);

// 上游 LastTapCount（:74-82，Up 事件调）：同判定不移位（回放上次 Down 的计数）。
[[nodiscard]] std::int32_t tap_history_info(const TapHistoryState& st);

// 键盘按 (Keycode, Modifiers) 分桶、鼠标按按钮字节分桶（上游 Cache 双桶）。
struct TapHistoryKeyboard {
  std::map<std::pair<std::uint32_t, std::uint8_t>, TapHistoryState> map_buckets;
};

struct TapHistoryMouse {
  std::map<std::uint8_t, TapHistoryState> map_buckets;
};

[[nodiscard]] std::int32_t tap_detect_mouse(TapHistoryMouse& taps, std::uint8_t u1_button,
                                            std::int64_t int8_now_ms, std::int32_t int4_x,
                                            std::int32_t int4_y);

[[nodiscard]] std::int32_t tap_info_mouse(const TapHistoryMouse& taps, std::uint8_t u1_button);

[[nodiscard]] std::int32_t tap_detect_keyboard(TapHistoryKeyboard& taps, Keycode key,
                                               ModifierKeys mods, std::int64_t int8_now_ms);

[[nodiscard]] std::int32_t tap_info_keyboard(const TapHistoryKeyboard& taps, Keycode key,
                                             ModifierKeys mods);

}

namespace ora::game::input {

// 上游 OpenRA.Game/Input/Hotkey.cs 的纯值对 (Keycode, Modifiers)。
struct HotkeyModel {
  Keycode key{Keycode::kUnknown};
  ModifierKeys mods{ModifierKeys::kNone};

  [[nodiscard]] friend bool operator==(const HotkeyModel&, const HotkeyModel&) = default;
};

inline constexpr HotkeyModel kHotkeyInvalid{};

[[nodiscard]] constexpr bool hotkey_is_valid(const HotkeyModel& hk) {
  return hk.key != Keycode::kUnknown;
}

// 上游 Hotkey.operator==（Hotkey.cs:59-66）：左操作数 key 为 UNKNOWN 一票否决，
// Invalid == Invalid 亦为 false。冲突检测/改键写回/激活判定落点全部经此函数。
[[nodiscard]] constexpr bool hotkey_equal(const HotkeyModel& lhs, const HotkeyModel& rhs) {
  if (lhs.key == Keycode::kUnknown) return false;
  return lhs.key == rhs.key && lhs.mods == rhs.mods;
}

// 上游 Hotkey.TryParse（Hotkey.cs:27-45）：首个空格切两段，key 段枚举名
// 忽略大小写，mods 段逗号分隔 flags 忽略大小写；三 token（如 "A B C"）拒绝。
[[nodiscard]] std::expected<HotkeyModel, std::string> hotkey_try_parse(std::string_view str_text);

// 上游 Hotkey.ToString()："KEY Mod1, Mod2"（Modifiers:F canonical 序）。
[[nodiscard]] std::string hotkey_to_string(const HotkeyModel& hk);

[[nodiscard]] std::optional<Keycode> keycode_from_name(std::string_view str_name);
[[nodiscard]] std::string_view keycode_name(Keycode key);

[[nodiscard]] std::optional<ModifierKeys> modifiers_from_string(std::string_view str_text);
[[nodiscard]] std::string modifiers_to_string(ModifierKeys mods);

// 上游 HotkeyReference.IsActivatedBy（HotkeyReference.cs:40-44）：Key 与
// Modifiers **双双相等**（全匹配，不做按位包含）；不走 UNKNOWN 否决的 hotkey_equal。
[[nodiscard]] constexpr bool hotkey_is_activated_by(const HotkeyModel& hk, const KeyInput& ki) {
  return hk.key == ki.key && hk.mods == ki.mods;
}

// 上游 Hotkey.FromKeyInput。
[[nodiscard]] constexpr HotkeyModel hotkey_from_key_input(const KeyInput& ki) {
  return HotkeyModel{ki.key, ki.mods};
}

}

namespace ora::game::input {

// 上游 HotkeyDefinition（HotkeyDefinition.cs）的注册表条目。
struct HotkeyDefinitionModel {
  std::string str_name;
  HotkeyModel hk_default{};  // Hotkey.Invalid = (UNKNOWN, None)
  std::string str_description;
  std::vector<std::string> vec_types;
  std::vector<std::string> vec_contexts;  // "hotkey-context." 前缀（上游 :51-53）
  bool b_readonly{};
  bool b_has_duplicates{};
};

// 上游 HotkeyManager（HotkeyManager.cs:18-116）。
// definitions/keys 平行数组保装载序（GetFirstDuplicate 的遍历序语义）。
struct HotkeyRegistryModel {
  std::vector<HotkeyDefinitionModel> vec_definitions;
  std::vector<HotkeyModel> vec_keys;
  // settings.yaml `Keys:` 段（shared SettingsModule）的重绑节点（name → "KEY Mod1, Mod2"），
  // 保序（上游 hotkeySettings.Yaml.Nodes 的 RemoveAll+追加语义）。持久化写盘归 T21。
  std::vector<std::pair<std::string, std::string>> vec_settings;
};

// 装载：文件节点序 = mod.yaml Hotkeys: 列表序；MiniYaml Merge 语义复用 T04。
// 定义值解析失败 = 报错拒绝装载（上游热键 yaml 路径 FieldLoader.GetValue 直接 throw）。
[[nodiscard]] std::expected<HotkeyRegistryModel, std::string> hotkey_registry_load(
    const std::vector<std::vector<MiniYamlNode>>& vec_files);

// settings `Keys:` 段覆盖（HotkeyManager ctor :36-39）：未知名跳过、Readonly 跳过、
// 坏值 = 报错拒绝装载（D059 修正：上游此解析在 InvalidValueAction 替换窗口外，
// 默认委托 throw YamlException → ModData ctor 炸 → 启动失败，t21 probe 6 实证）。
[[nodiscard]] std::expected<void, std::string> hotkey_registry_apply_settings(
    HotkeyRegistryModel& reg, std::span<const MiniYamlNode> vec_keys_nodes);

[[nodiscard]] std::optional<std::size_t> hotkey_registry_find(const HotkeyRegistryModel& reg,
                                                              std::string_view str_name);

// 上游 GetHotkeyReference（:50-61）：有定义 → 当前值活查询；否则按字面
// Hotkey.TryParse(name)；再失败 → Hotkey.Invalid。
[[nodiscard]] HotkeyModel hotkey_resolve(const HotkeyRegistryModel& reg, std::string_view str_name);

// 上游 Set（:63-89）：定义缺失/Readonly → false 不动；否则更新当前值 + settings
// 节点 RemoveAll + `value != Default`（hotkey_equal 的 UNKNOWN 永不等语义）时追加 +
// 全表冲突重算（上游为条件重算，见 COVERAGE.md 偏离登记）。
bool hotkey_set(HotkeyRegistryModel& reg, std::string_view str_name, const HotkeyModel& hk);

// 上游 GetFirstDuplicate（:91-106）：装载序遍历 keys，hotkey_equal 且 Contexts 相交。
[[nodiscard]] std::optional<std::size_t> hotkey_get_first_duplicate(const HotkeyRegistryModel& reg,
                                                                    std::size_t idx_definition);

void hotkey_registry_recompute_duplicates(HotkeyRegistryModel& reg);

}

namespace ora::game::input {

constexpr std::size_t kControlGroupCount{10};

struct SelectionModel {
  std::vector<std::uint32_t> vec_actor_ids;
  std::array<std::vector<std::uint32_t>, kControlGroupCount> arr_groups;
};

void selection_combine(SelectionModel& selection, std::span<const std::uint32_t> vec_new,
                       bool b_combine, bool b_click);

void selection_clear(SelectionModel& selection);

void control_group_create(SelectionModel& selection, const world::WorldModel& w,
                          std::uint32_t u4_viewer_player, std::size_t idx_group);

void control_group_add_to(SelectionModel& selection, const world::WorldModel& w,
                          std::uint32_t u4_viewer_player, std::size_t idx_group);

// CombineSelectionWithControlGroup（ControlGroups.cs:74-77）：组内（in world）
// 成员并入当前选区（Combine true/false）。
void control_group_combine_with(SelectionModel& selection, const world::WorldModel& w,
                                std::size_t idx_group);

// RemoveFromControlGroup（ControlGroups.cs:84-89 + HotkeyLogic :34-44）：选中各 actor
// 从其所属（首个）组移除。
void control_group_remove_from(SelectionModel& selection);

void control_group_select(SelectionModel& selection, std::size_t idx_group);

}

namespace ora::game::input {

struct PxRect {
  std::int32_t int4_x{};
  std::int32_t int4_y{};
  std::int32_t int4_w{};
  std::int32_t int4_h{};

  [[nodiscard]] friend bool operator==(const PxRect&, const PxRect&) = default;

  [[nodiscard]] constexpr bool contains(std::int32_t int4_px_x, std::int32_t int4_px_y) const {
    return int4_px_x >= int4_x && int4_px_x < int4_x + int4_w && int4_px_y >= int4_y &&
           int4_px_y < int4_y + int4_h;
  }

  [[nodiscard]] constexpr bool intersects(const PxRect& other) const {
    return int4_x < other.int4_x + other.int4_w && other.int4_x < int4_x + int4_w &&
           int4_y < other.int4_y + other.int4_h && other.int4_y < int4_y + int4_h;
  }
};

struct ActorBoundsPair {
  std::uint32_t u4_actor_id{};
  PxRect rect_bounds;
};

[[nodiscard]] std::int32_t view_to_world_px(std::int32_t int4_view_px);

[[nodiscard]] std::int32_t world_px_to_cell(std::int32_t int4_world_px);

[[nodiscard]] std::int32_t actor_center_px(std::int32_t int4_cell);

[[nodiscard]] std::optional<PxRect> actor_mouse_bounds(const world::WorldModel& w,
                                                       const world::ActorModel& actor);

[[nodiscard]] std::vector<ActorBoundsPair> actors_at_mouse(const world::WorldModel& w,
                                                           std::int32_t int4_x,
                                                           std::int32_t int4_y);

[[nodiscard]] std::vector<ActorBoundsPair> actors_in_box(const world::WorldModel& w,
                                                         const PxRect& rect);

}

namespace ora::game::input {

constexpr std::int32_t kSelectionDeadzone{24};

// 上游 SelectionPriorityModifiers（TraitsInterfaces.cs:479-484）。
enum class SelectionPriorityModifiers : std::uint8_t {
  kNone = 0,
  kCtrl = 1,
  kAlt = 2,
};

[[nodiscard]] constexpr SelectionPriorityModifiers operator&(
    SelectionPriorityModifiers a, SelectionPriorityModifiers b) {
  return static_cast<SelectionPriorityModifiers>(static_cast<std::uint8_t>(a) &
                                                 static_cast<std::uint8_t>(b));
}

[[nodiscard]] constexpr SelectionPriorityModifiers operator|(
    SelectionPriorityModifiers a, SelectionPriorityModifiers b) {
  return static_cast<SelectionPriorityModifiers>(static_cast<std::uint8_t>(a) |
                                                 static_cast<std::uint8_t>(b));
}

enum class SelectionRelationship {
  kSelf,
  kNeutral,
  kEnemy,
};

[[nodiscard]] SelectionRelationship selection_relationship(const world::WorldModel& w,
                                                           std::uint32_t u4_viewer_player,
                                                           std::uint32_t u4_owner_player);

[[nodiscard]] std::int32_t selection_priority(const world::WorldModel& w,
                                              const world::ActorModel& actor,
                                              std::uint32_t u4_viewer_player,
                                              ModifierKeys mods);

// 上游 Selectable.Class（Selectable.cs:43-53）：yaml Class，空则 actor 类型名（小写）。
[[nodiscard]] std::string selectable_class(const world::WorldModel& w,
                                           const world::ActorModel& actor);

// 上游 ISelectableInfo.PriorityModifiers（Selectable trait yaml 字段）。
[[nodiscard]] SelectionPriorityModifiers selectable_priority_modifiers(
    const world::WorldModel& w, const world::ActorModel& actor);

[[nodiscard]] std::int32_t pixel_distance(std::int32_t int4_dx, std::int32_t int4_dy);

[[nodiscard]] std::optional<std::uint32_t> select_highest_priority_at_point(
    const world::WorldModel& w, std::uint32_t u4_viewer_player, std::int32_t int4_x,
    std::int32_t int4_y, ModifierKeys mods);

[[nodiscard]] std::vector<std::uint32_t> select_actors_in_box(
    const world::WorldModel& w, std::uint32_t u4_viewer_player, std::int32_t int4_ax,
    std::int32_t int4_ay, std::int32_t int4_bx, std::int32_t int4_by, ModifierKeys mods);

// 上游 SelectActorsOnScreen（SelectionUtils.cs:23-28）：视口（world px）框内
// Selectable actor 按 owner 集 + selectionClass 集过滤；classes 为空 = 全选。
[[nodiscard]] std::vector<std::uint32_t> select_actors_on_screen(
    const world::WorldModel& w, std::span<const std::uint32_t> vec_eligible_players,
    std::span<const std::string> vec_selection_classes, const PxRect& rect_viewport);

// 上游 SelectActorsInWorld（SelectionUtils.cs:30-33）：全图 in-world Selectable actor
// 按 owner 集 + selectionClass 集过滤。
[[nodiscard]] std::vector<std::uint32_t> select_actors_in_world(
    const world::WorldModel& w, std::span<const std::uint32_t> vec_eligible_players,
    std::span<const std::string> vec_selection_classes);

// 上游 SubsetWithHighestSelectionPriority（SelectableExts.cs:92-99）：
// GroupBy(priority) 取最高组（保输入序）。
[[nodiscard]] std::vector<std::uint32_t> subset_with_highest_selection_priority(
    const world::WorldModel& w, std::uint32_t u4_viewer_player,
    std::span<const std::uint32_t> vec_ids, ModifierKeys mods);

// 上游 GetPlayersToIncludeInSelection（SelectionUtils.cs:76-84）：viewer，
// 观战（NonCombatant+Spectating）→ 全体玩家。
[[nodiscard]] std::vector<std::uint32_t> players_to_include_in_selection(
    const world::WorldModel& w, std::uint32_t u4_viewer_player);

// 双击选同类（WIC :148-168）：tap>=2 时鼠标下最高优先 actor 的 Class →
// 视口内同 Class 且 eligible 玩家的 actor 集合（调用方 Combine(isCombine=true,isClick=false)）；
// 无命中/owner 不 eligible → 空集（不改选区）。
[[nodiscard]] std::vector<std::uint32_t> select_by_type_double_click(
    const world::WorldModel& w, std::uint32_t u4_viewer_player, std::int32_t int4_x,
    std::int32_t int4_y, ModifierKeys mods, const PxRect& rect_viewport);

struct DragState {
  bool b_dragging{};
  std::int32_t int4_start_x{};
  std::int32_t int4_start_y{};
  std::int32_t int4_mouse_x{};
  std::int32_t int4_mouse_y{};
};

[[nodiscard]] std::int32_t drag_length(const DragState& drag);

[[nodiscard]] bool drag_is_valid_box(const DragState& drag);

void selection_handle_mouse(const world::WorldModel& w, std::uint32_t u4_viewer_player,
                            SelectionModel& selection, DragState& drag, const MouseInput& mi);

// 带 world px 视口框的完整 WIC 左键面（双击选同类走视口内 SelectActorsOnScreen）。
// 旧签名等价于"视口 = 全图包围盒"。
void selection_handle_mouse_in_view(const world::WorldModel& w, std::uint32_t u4_viewer_player,
                                    SelectionModel& selection, DragState& drag,
                                    const MouseInput& mi, const PxRect& rect_viewport);

}

namespace ora::game::input {

// 上游 TargetModifiers（TraitsInterfaces.cs:129-138）。
enum class TargetModifiers : std::uint8_t {
  kNone = 0,
  kForceAttack = 1,
  kForceQueue = 2,
  kForceMove = 4,
};

[[nodiscard]] constexpr TargetModifiers operator&(TargetModifiers a, TargetModifiers b) {
  return static_cast<TargetModifiers>(static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
}

[[nodiscard]] constexpr TargetModifiers operator|(TargetModifiers a, TargetModifiers b) {
  return static_cast<TargetModifiers>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

[[nodiscard]] constexpr bool has_target_modifier(TargetModifiers mods, TargetModifiers mod) {
  return (mods & mod) != TargetModifiers::kNone;
}

// 上游 UOG.OrderForUnit :169-174：Ctrl→ForceAttack、Shift→ForceQueue、Alt→ForceMove
// （独立位可叠加）。
[[nodiscard]] TargetModifiers target_modifiers_from_keys(ModifierKeys mods);

[[nodiscard]] std::int32_t cpos_bits(std::int32_t int4_x, std::int32_t int4_y,
                                     std::int32_t int4_layer);

[[nodiscard]] network::TargetModel make_cell_target(std::int32_t int4_x, std::int32_t int4_y);

[[nodiscard]] network::TargetModel make_actor_target(std::uint32_t u4_actor,
                                                     std::int32_t int4_generation);

[[nodiscard]] network::OrderModel make_move_order(std::uint32_t u4_subject,
                                                  std::int32_t int4_cell_x,
                                                  std::int32_t int4_cell_y, bool b_queued);

[[nodiscard]] network::OrderModel make_attack_order(std::uint32_t u4_subject,
                                                    std::uint32_t u4_target_actor,
                                                    std::int32_t int4_generation, bool b_queued);

// 通用攻击单构造：str_order_name 可为 "Attack"/"ForceAttack"；目标 actor 或 cell。
[[nodiscard]] network::OrderModel make_attack_order_ex(std::uint32_t u4_subject,
                                                       const network::TargetModel& target,
                                                       std::string_view str_order_name,
                                                       bool b_queued);

// AttackMoveOrderGenerator.OrderInner（AttackMove.cs:118-131）：subjects = 选中
// AttackMove 单；Ctrl→"AssaultMove" 否则 "AttackMove"；Shift→queued 且不退出模式；
// 非 Shift 用后自动 CancelInputMode（无头面无生成器状态可退，省略）。
// 单条订单：Subject 空、Target.FromCell、GroupedActors=subjects。
[[nodiscard]] std::vector<network::OrderModel> attack_move_generator_order(
    const world::WorldModel& w, std::uint32_t u4_viewer_player, const SelectionModel& selection,
    std::int32_t int4_cell_x, std::int32_t int4_cell_y, const MouseInput& mi);

[[nodiscard]] network::OrderModel make_create_group_order(std::uint32_t u4_player_actor,
                                                          std::span<const std::uint32_t> vec_grouped);

[[nodiscard]] network::OrderModel make_start_production_order(std::uint32_t u4_queue_actor,
                                                               std::string_view str_item,
                                                               std::uint32_t u4_count,
                                                               bool b_queued);

[[nodiscard]] network::OrderModel make_pause_production_order(std::uint32_t u4_queue_actor,
                                                              std::string_view str_item,
                                                              bool b_paused);

[[nodiscard]] network::OrderModel make_cancel_production_order(std::uint32_t u4_queue_actor,
                                                               std::string_view str_item,
                                                               std::uint32_t u4_count);

[[nodiscard]] std::vector<network::OrderModel> unit_order_generator_order(
    const world::WorldModel& w, std::uint32_t u4_viewer_player, const SelectionModel& selection,
    std::int32_t int4_cell_x, std::int32_t int4_cell_y, std::int32_t int4_world_px_x,
    std::int32_t int4_world_px_y, const MouseInput& mi);

[[nodiscard]] std::vector<network::OrderModel> order_generator_handle_mouse(
    const world::WorldModel& w, std::uint32_t u4_viewer_player, const SelectionModel& selection,
    const DragState& drag, const MouseInput& mi);

}

namespace ora::game::input {

constexpr std::int32_t kPanelWidth{238};
constexpr std::int32_t kPanelTabSize{28};
constexpr std::int32_t kPanelTabStride{30};
constexpr std::int32_t kPanelIconWidth{62};
constexpr std::int32_t kPanelIconHeight{46};
constexpr std::int32_t kPanelIconStrideX{64};
constexpr std::int32_t kPanelIconStrideY{48};
constexpr std::int32_t kPanelIconColumns{3};
constexpr std::int32_t kPanelStartProductionShiftCount{5};

inline constexpr std::array<std::string_view, 6> kPanelTabTypes{
    "Building", "Defense", "Infantry", "Vehicle", "Aircraft", "Naval"};

struct ProductionItemView {
  std::string str_item;
  std::int32_t int4_total_time{1};
  std::int32_t int4_remaining_time{1};
  std::int32_t int4_total_cost{};
  std::int32_t int4_remaining_cost{};
  bool b_started{};
  bool b_paused{};
  bool b_done{};
  bool b_building{};
};

struct ProductionQueueView {
  std::string str_type;
  bool b_enabled{};
  bool b_disallow_paused{};
  std::vector<std::string> vec_items;
  std::vector<ProductionItemView> vec_queue;
};

struct ProductionPanelModel {
  std::string str_current_type;
};

struct PanelGeometry {
  std::int32_t int4_view_w{};
  std::int32_t int4_view_h{};
};

struct PanelDispatchResult {
  bool b_handled{};
  std::vector<network::OrderModel> vec_orders;
};

[[nodiscard]] PxRect panel_rect(PanelGeometry geo);

[[nodiscard]] PxRect panel_tab_rect(PanelGeometry geo, std::size_t idx_tab);

[[nodiscard]] PxRect panel_icon_rect(PanelGeometry geo, std::size_t idx_icon);

[[nodiscard]] bool panel_contains(PanelGeometry geo, std::int32_t int4_x, std::int32_t int4_y);

[[nodiscard]] std::int32_t panel_item_progress_percent(const ProductionItemView& item);

[[nodiscard]] PanelDispatchResult panel_handle_mouse(ProductionPanelModel& panel,
                                                     PanelGeometry geo,
                                                     std::span<const ProductionQueueView> vec_queues,
                                                     std::uint32_t u4_queue_actor,
                                                     const MouseInput& mi);

}

namespace ora::game::input {

// 键盘滚动方向位（上游 ScrollDirection，ViewportControllerWidget 键面）。
enum class ScrollDirection : std::uint8_t {
  kNone = 0,
  kUp = 1,
  kDown = 2,
  kLeft = 4,
  kRight = 8,
};

[[nodiscard]] constexpr ScrollDirection operator|(ScrollDirection a, ScrollDirection b) {
  return static_cast<ScrollDirection>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

[[nodiscard]] constexpr ScrollDirection operator&(ScrollDirection a, ScrollDirection b) {
  return static_cast<ScrollDirection>(static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
}

[[nodiscard]] constexpr ScrollDirection operator~(ScrollDirection a) {
  return static_cast<ScrollDirection>(~static_cast<std::uint8_t>(a) & 0x0f);
}

[[nodiscard]] constexpr bool has_scroll_direction(ScrollDirection dirs, ScrollDirection dir) {
  return (dirs & dir) != ScrollDirection::kNone;
}

// 无头视口状态（上游 Viewport + ViewportControllerWidget 键消费面）。
// 坐标 = world px；zoom 默认 [1,2]（Viewport.cs:92-93 上游缺省，mod 覆盖未建模）。
struct ViewportState {
  std::int32_t int4_center_x{};
  std::int32_t int4_center_y{};
  double f64_zoom{1.0};
  std::array<std::optional<std::pair<std::int32_t, std::int32_t>>, 4> arr_bookmarks{};
  ScrollDirection scroll_directions{ScrollDirection::kNone};
  bool b_disable_user_interface{};
  bool b_disable_all_user_interface{};
};

// Viewport.AdjustZoom（Viewport.cs:102-106）：zoom *= exp(dz)，夹 [1,2]。
void viewport_adjust_zoom(ViewportState& viewport, double f64_dz);

// Viewport.ToggleZoom（Viewport.cs:118-126，ResetZoom 热键消费）：zoom > min → min；
// zoom == min → max。
void viewport_toggle_zoom(ViewportState& viewport);

void viewport_set_center(ViewportState& viewport, std::int32_t int4_x, std::int32_t int4_y);

// ViewportControllerWidget.HandleKeyPress（:400-490）键面：
// MapScrollUp/Down/Left/Right —— Down 置方向位（isHotkey 或绑定无修饰键时置位）、
// Up 清位（上游 :404-412 quirk：绑定 Modifiers==None 时任何修饰键按下也算命中）；
// ZoomIn/ZoomOut = AdjustZoom(±0.25)；MapJumpTo*Edge = 视口中心钉到地图边
// （int4_map_w_px/int4_map_h_px = 地图 world px 尺寸）；MapBookmarkSave/Restore01..04；
// DisableUserInterface/DisableAllUserInterface 位。全走注册表活查询（全匹配激活）。
[[nodiscard]] bool viewport_handle_key(ViewportState& viewport, const HotkeyRegistryModel& reg,
                                       const KeyInput& ki, std::int32_t int4_map_w_px,
                                       std::int32_t int4_map_h_px);

}

namespace ora::game::input {

// ControlGroupsWidget.HandleKeyPress（ControlGroupsWidget.cs:128-171）键面：
// 逐组按 Select/Create/AddTo/CombineWith/JumpTo 顺序做注册表活查询（全匹配激活）。
// Select 命中且 KeyInput.MultiTapCount>=2 时记录视口跳转 hook（opt_center_group，
// 表现层据此 Center 组质心；JumpTo 同样只跳转不改选区）。
struct ControlGroupsKeyResult {
  bool b_handled{};
  std::optional<std::size_t> opt_center_group{};
};

[[nodiscard]] ControlGroupsKeyResult control_groups_handle_key(
    const HotkeyRegistryModel& reg, SelectionModel& selection, const world::WorldModel& w,
    std::uint32_t u4_viewer_player, const KeyInput& ki);

// SelectAllUnitsHotkeyLogic.OnHotkeyActivated：屏幕内（eligible 玩家、全 Class）
// 取最高优先组 → 未增则全图；替换 Combine。Down-only（SingleHotkeyBaseLogic）。
[[nodiscard]] bool hotkey_action_select_all_units(const HotkeyRegistryModel& reg,
                                                  const world::WorldModel& w,
                                                  std::uint32_t u4_viewer_player,
                                                  SelectionModel& selection, const KeyInput& ki,
                                                  const PxRect& rect_viewport);

// SelectUnitsByTypeHotkeyLogic.OnHotkeyActivated：以选中 actor 的 Class 集选屏幕
// 同类 → 未增则全图；并选 Combine。空选区/无 eligible 选中 → false。
[[nodiscard]] bool hotkey_action_select_units_by_type(const HotkeyRegistryModel& reg,
                                                      const world::WorldModel& w,
                                                      std::uint32_t u4_viewer_player,
                                                      SelectionModel& selection,
                                                      const KeyInput& ki,
                                                      const PxRect& rect_viewport);

// 移除选中（HotkeyManager 改键后的热键名消费示例面）：RemoveFromControlGroup。
[[nodiscard]] bool hotkey_action_remove_from_control_group(const HotkeyRegistryModel& reg,
                                                           SelectionModel& selection,
                                                           const KeyInput& ki);

}
}

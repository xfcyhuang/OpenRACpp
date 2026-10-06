// UPSTREAM: OpenRA.Game/Widgets/Widget.cs @b6fc03f(Widget 树逐语义:Ui 门面
// /ChromeLogic/WidgetBounds/Widget/ContainerWidget/InputWidget/WidgetArgs/
// Mediator)。C# static Ui → 进程级单例(Widget 的 Outer 族经其取焦点态);
// 子 widget 所有权归父 unique_ptr,RemoveChild/HideChild 转移出树;Mediator
// 的 TypeDictionary 反射接口索引 → type_index 桶 + NotificationHandlerSet
// 混入;PostInit 的 ObjectCreator.CreateObject → 名字分派注册表(未知名
// "Cannot locate type: {name}" 逐字);FieldLoader.LoadFieldOrProperty →
// LoadFieldOrProperty 虚方法链式回退;Initialize 的 Renderer.Resolution 与
// ChromeMetrics 经 WidgetArgs("windowSize"/"chromeMetrics")注入。
// Verbatim-semantics rewrite of the widget tree. C#'s static Ui becomes the
// process singleton (the Widget Outer family reaches focus state through
// it); child widgets are owned by the parent's unique_ptrs with
// RemoveChild/HideChild transferring ownership out of the tree; the
// Mediator's TypeDictionary reflection index becomes type_index buckets +
// the NotificationHandlerSet mixin; PostInit's ObjectCreator.CreateObject
// becomes a name-dispatch registry (unknown names throw
// "Cannot locate type: {name}" verbatim); FieldLoader.LoadFieldOrProperty
// becomes the chained-fallback LoadFieldOrProperty virtual; Initialize's
// Renderer.Resolution and ChromeMetrics arrive through WidgetArgs
// ("windowSize"/"chromeMetrics").
#pragma once
import std;

#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "meta/variable_expression.hpp"
#include "platform/sdl2_input.hpp"

namespace ora::yaml {
class MiniYaml;
}

namespace ora::sim {
class World;
}
namespace ora::net {
class OrderManager;
}
namespace ora::gfx {
class WorldRenderer;
}
namespace ora::game {
class ModData;
}

namespace ora::ui {

class Widget;
class Ui;
class ChromeMetrics;
class Mediator;

/// logicArgs 替换表(MiniYaml::ToDictionary 产物;键/值指向池串与节点,
/// 加载器存活期有效)
/// The logicArgs substitution table (MiniYaml::ToDictionary's product;
/// keys/values point into pooled strings and loader-owned nodes, valid for
/// the loader's lifetime).
using YamlDictionary = std::vector<std::pair<std::string_view, const yaml::MiniYaml*>>;

/// WidgetArgs 载荷(引擎与 Phase 6 逻辑实际穿越的类型闭包)
/// The WidgetArgs payload (the closure of types actually threaded by the
/// engine and Phase 6 logic).
using WidgetArgValue =
    std::variant<std::monostate, int2, std::int32_t, std::string, Widget*, sim::World*,
                 net::OrderManager*, gfx::WorldRenderer*, game::ModData*, ChromeMetrics*,
                 Mediator*,
                 const std::map<std::string, std::int32_t>*,  // substitutions
                 const YamlDictionary*>;                      // logicArgs

/// WidgetArgs(L673-679;插入序条目表)
/// WidgetArgs (L673-679; an insertion-ordered entry list).
class WidgetArgs {
 public:
  WidgetArgs() = default;
  WidgetArgs(std::initializer_list<std::pair<std::string, WidgetArgValue>> init)
      : vec_entries_(init) {}

  bool ContainsKey(std::string_view str_key) const { return Find(str_key) != nullptr; }

  /// 上游 args[key]:缺失抛 KeyNotFoundException 等价消息
  /// Upstream args[key]: a miss throws the KeyNotFoundException equivalent.
  const WidgetArgValue& At(std::string_view str_key) const {
    const WidgetArgValue* ptr_value = Find(str_key);
    if (ptr_value == nullptr)
      throw std::runtime_error("The given key was not present in the dictionary.");
    return *ptr_value;
  }

  WidgetArgValue* TryGet(std::string_view str_key) { return Find(str_key); }
  const WidgetArgValue* TryGet(std::string_view str_key) const { return Find(str_key); }

  /// 重复键抛(上游 Add 语义) | throws on duplicates (upstream Add).
  void Add(std::string str_key, WidgetArgValue val_value) {
    if (ContainsKey(str_key))
      throw std::runtime_error("An item with the same key has already been added.");
    vec_entries_.emplace_back(std::move(str_key), std::move(val_value));
  }

  /// 索引写:存在则覆盖(PostInit 的 set/Remove 往返面)
  /// Indexer write: overwrite when present (the PostInit set/Remove round
  /// trip).
  void Set(std::string str_key, WidgetArgValue val_value) {
    for (auto& [str_entry_key, val_entry] : vec_entries_)
      if (str_entry_key == str_key) {
        val_entry = std::move(val_value);
        return;
      }
    vec_entries_.emplace_back(std::move(str_key), std::move(val_value));
  }

  void Remove(std::string_view str_key) {
    std::erase_if(vec_entries_,
                  [&](const auto& pair_entry) { return pair_entry.first == str_key; });
  }

  /// 上游 new WidgetArgs(args) { {key, value} }:已存在键不覆盖
  /// Upstream's new WidgetArgs(args) { {key, value} }: an existing key is
  /// not overwritten.
  WidgetArgs With(std::string_view str_key, WidgetArgValue val_value) const {
    WidgetArgs args_next = *this;
    if (!args_next.ContainsKey(str_key))
      args_next.vec_entries_.emplace_back(std::string(str_key), std::move(val_value));
    return args_next;
  }

 private:
  const WidgetArgValue* Find(std::string_view str_key) const {
    for (const auto& [str_entry_key, val_entry] : vec_entries_)
      if (str_entry_key == str_key)
        return &val_entry;
    return nullptr;
  }
  WidgetArgValue* Find(std::string_view str_key) {
    return const_cast<WidgetArgValue*>(
        static_cast<const WidgetArgs*>(this)->Find(str_key));
  }

  std::vector<std::pair<std::string, WidgetArgValue>> vec_entries_;
};

// ———— Mediator(L681-707)————
// ———— Mediator (L681-707) ————

/// INotificationHandler<T>(L704-707);通知类型 T 以 type_index 自键
/// (上游接口 Type 键等价)
/// INotificationHandler<T> (L704-707); the notification type T keys itself
/// by type_index (the interface-Type key equivalent).
template <class T>
class INotificationHandler {
 public:
  static const std::type_index kNotificationId;
  virtual ~INotificationHandler() = default;
  virtual void Handle(const T& notification_notification) = 0;
};

template <class T>
const std::type_index INotificationHandler<T>::kNotificationId = typeid(T);

/// Mediator(L681-702)
/// Mediator (L681-702).
class Mediator {
 public:
  template <class T>
  void Subscribe(INotificationHandler<T>& handler_instance) {
    entries_[INotificationHandler<T>::kNotificationId].push_back(
        Entry{&handler_instance, [&handler_instance](const void* ptr_notification) {
                handler_instance.Handle(*static_cast<const T*>(ptr_notification));
              }});
  }

  template <class T>
  void Unsubscribe(INotificationHandler<T>& handler_instance) {
    auto it_bucket = entries_.find(INotificationHandler<T>::kNotificationId);
    if (it_bucket == entries_.end())
      return;
    auto& vec_bucket = it_bucket->second;
    for (auto it_entry = vec_bucket.begin(); it_entry != vec_bucket.end(); ++it_entry)
      if (it_entry->ptr_instance == &handler_instance) {
        vec_bucket.erase(it_entry);  // 首匹配摘除(上游 Remove) | first-match removal
        return;
      }
  }

  template <class T>
  void Send(const T& notification_notification) {
    // 插入序快照上分发(执行期订阅/退订可重入)
    // dispatch over an insertion-order snapshot (reentrant subscribe/
    // unsubscribe).
    auto it_bucket = entries_.find(INotificationHandler<T>::kNotificationId);
    if (it_bucket == entries_.end())
      return;

    std::vector<Entry> vec_handlers = it_bucket->second;
    for (const Entry& entry_handler : vec_handlers)
      entry_handler.fn_notify(&notification_notification);
  }

 private:
  struct Entry {
    void* ptr_instance;
    std::function<void(const void*)> fn_notify;
  };
  std::map<std::type_index, std::vector<Entry>> entries_;
};

/// ChromeLogic(L196-203);SubscribeAll/UnsubscribeAll = TypeDictionary.Add
/// 反射接口枚举的编译期等价
/// ChromeLogic (L196-203); SubscribeAll/UnsubscribeAll are the
/// compile-time equivalent of TypeDictionary.Add's interface enumeration.
class ChromeLogic {
 public:
  virtual ~ChromeLogic() = default;
  virtual void Tick() {}
  virtual void BecameHidden() {}
  virtual void BecameVisible() {}
  virtual void SubscribeAll(Mediator& mediator_m) { (void)mediator_m; }
  virtual void UnsubscribeAll(Mediator& mediator_m) { (void)mediator_m; }
};

/// 通知接口集混入:Logic 声明 `class MyLogic : public
/// NotificationHandlerSet<A, B>`(经本混入继承 ChromeLogic,SubscribeAll
/// 虚分派可达)
/// The notification-interface mixin: declare `class MyLogic : public
/// NotificationHandlerSet<A, B>` (ChromeLogic arrives through this mixin so
/// the SubscribeAll dispatch reaches it).
template <class... Ts>
class NotificationHandlerSet : public ChromeLogic, public INotificationHandler<Ts>... {
 public:
  void SubscribeAll(Mediator& mediator_m) override {
    (mediator_m.Subscribe(static_cast<INotificationHandler<Ts>&>(*this)), ...);
  }
  void UnsubscribeAll(Mediator& mediator_m) override {
    (mediator_m.Unsubscribe(static_cast<INotificationHandler<Ts>&>(*this)), ...);
  }
};

/// ChromeLogic 名字分派注册表(ObjectCreator.CreateObject 等价)
/// The ChromeLogic name-dispatch registry (the ObjectCreator.CreateObject
/// equivalent).
class ChromeLogicRegistry {
 public:
  using FactoryFn = std::function<std::unique_ptr<ChromeLogic>(WidgetArgs&)>;

  static ChromeLogicRegistry& Instance();

  void Register(std::string str_name, FactoryFn fn_factory);

  /// 未知名抛 "Cannot locate type: {name}"(ObjectCreator.cs L92 逐字)
  /// an unknown name throws "Cannot locate type: {name}" (ObjectCreator.cs
  /// L92 verbatim).
  std::unique_ptr<ChromeLogic> Create(const std::string& str_name,
                                      WidgetArgs& args) const;

 private:
  std::vector<std::pair<std::string, FactoryFn>> vec_entries_;
};

/// Logic 自注册宏(名字 = 类名) | the Logic self-registration macro (the
/// name = the class name).
#define ORA_REGISTER_CHROME_LOGIC(Cls)                          \
  const bool kChromeLogicRegistered_##Cls = [] {                \
    ::ora::ui::ChromeLogicRegistry::Instance().Register(        \
        #Cls, [](::ora::ui::WidgetArgs& args_args)              \
                  -> std::unique_ptr<::ora::ui::ChromeLogic> {  \
          return std::make_unique<Cls>(args_args);              \
        });                                                     \
    return true;                                                \
  }()

/// WidgetBounds(L205-217)
/// WidgetBounds (L205-217).
struct WidgetBounds {
  std::int32_t int4_x = 0;
  std::int32_t int4_y = 0;
  std::int32_t int4_width = 0;
  std::int32_t int4_height = 0;

  constexpr std::int32_t Left() const { return int4_x; }
  constexpr std::int32_t Right() const { return int4_x + int4_width; }
  constexpr std::int32_t Top() const { return int4_y; }
  constexpr std::int32_t Bottom() const { return int4_y + int4_height; }

  constexpr Rectangle ToRectangle() const {
    return Rectangle{int4_x, int4_y, int4_width, int4_height};
  }
};

/// Widget(L219-631;抽象基类)
/// Widget (L219-631; the abstract base).
class Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "Widget";

  Widget();
  virtual ~Widget() = default;

  Widget& operator=(const Widget&) = delete;

  // YAML 声明面(公开字段,上游同名)| the YAML-declared faces (public
  // fields, upstream names)
  std::string str_id;  // 空 = C# null | empty = C# null
  std::optional<expr::IntegerExpression> expr_x;
  std::optional<expr::IntegerExpression> expr_y;
  std::optional<expr::IntegerExpression> expr_width;
  std::optional<expr::IntegerExpression> expr_height;
  std::vector<std::string> vec_logic;
  bool b_visible = true;
  bool b_ignore_mouse_over = false;
  bool b_ignore_child_mouse_over = false;

  // 计算面 | the calculated faces
  WidgetBounds bounds_widget;
  Widget* ptr_parent = nullptr;  // 非拥有引用 | a non-owning reference
  std::function<bool()> fn_is_visible = [this] { return b_visible; };

  const std::vector<std::unique_ptr<Widget>>& OwnedChildren() const {
    return vec_children_;
  }
  std::vector<Widget*> Children() const {
    std::vector<Widget*> vec_view;
    vec_view.reserve(vec_children_.size());
    for (const auto& ptr_child : vec_children_)
      vec_view.push_back(ptr_child.get());
    return vec_view;
  }

  /// 类型名(GetType().Name 等价;异常文本用)
  /// the type name (the GetType().Name equivalent; for exception texts).
  virtual std::string_view TypeName() const { return kWidgetTypeName; }

  /// 默认不可克隆,消息逐字(L269) | not cloneable by default, message
  /// verbatim (L269).
  virtual std::unique_ptr<Widget> Clone() {
    throw std::runtime_error("Widget type `" + std::string(TypeName()) +
                             "` is not cloneable.");
  }

  virtual int2 RenderOrigin() const;
  virtual int2 ChildOrigin() const { return RenderOrigin(); }
  virtual Rectangle RenderBounds() const;

  /// L292-320:YAML 方程求值定界;substitutions 与保留键撞名时按 C#
  /// Dictionary.Add 抛
  /// L292-320: evaluates the YAML equations; a user substitution hitting a
  /// reserved key throws per C# Dictionary.Add.
  virtual void Initialize(WidgetArgs& args);

  /// L322-336:Logic 名物化 + Mediator 订阅
  /// L322-336: materializes the Logic names + subscribes to the Mediator.
  void PostInit(WidgetArgs& args);

  /// 链式回退:子类先查自有字段;未知名返回 false(加载器抛上游
  /// UnknownFieldAction 文本)
  /// chained fallback: subclasses consult their own fields first; an
  /// unknown key returns false (the loader throws upstream's
  /// UnknownFieldAction text).
  virtual bool LoadFieldOrProperty(std::string_view str_key, std::string_view str_value);

  virtual Rectangle EventBounds() const { return RenderBounds(); }
  virtual bool EventBoundsContains(int2 pt_location) const;

  bool HasMouseFocus() const;
  bool HasKeyboardFocus() const;

  virtual bool TakeMouseFocus(const MouseInput& mi_input);
  /// 返回 false 表示拒绝让出(上游注释) | returning false refuses the
  /// yield (upstream comment).
  virtual bool YieldMouseFocus(const MouseInput& mi_input);
  virtual bool TakeKeyboardFocus();
  virtual bool YieldKeyboardFocus();

  virtual const std::string* GetCursor(int2 pt_pos) const;  // 空 = null | null = none
  const std::string* GetCursorOuter(int2 pt_pos) const;

  virtual void MouseEntered() {}
  virtual void MouseExited() {}

  virtual bool HandleMouseInput(const MouseInput& mi_input) {
    (void)mi_input;
    return false;
  }
  bool HandleMouseInputOuter(const MouseInput& mi_input);

  virtual bool HandleKeyPress(const KeyInput& e_input) {
    (void)e_input;
    return false;
  }
  virtual bool HandleKeyPressOuter(const KeyInput& e_input);

  virtual bool HandleTextInput(std::string_view str_text) {
    (void)str_text;
    return false;
  }
  virtual bool HandleTextInputOuter(std::string_view str_text);

  virtual void PrepareRenderables() {}
  virtual void PrepareRenderablesOuter();

  virtual void Draw() {}
  virtual void DrawOuter();

  virtual void Tick() {}
  virtual void TickOuter();

  virtual void AddChild(std::unique_ptr<Widget> ptr_child);
  /// 所有权转移出树;不在子列表也照常 Removed()(上游静默移除 + 无条件通知)
  /// ownership moves out of the tree; Removed() fires even on a miss
  /// (upstream's silent removal + unconditional notification).
  virtual std::unique_ptr<Widget> RemoveChild(Widget& child_widget);
  virtual std::unique_ptr<Widget> HideChild(Widget& child_widget);
  virtual void RemoveChildren();

  virtual void Hidden();
  virtual void Removed();

  Widget* GetOrNull(std::string_view str_id_to_find) const;

  template <class T>
  T* GetOrNull(std::string_view str_id_to_find) const {
    return static_cast<T*>(GetOrNull(str_id_to_find));
  }

  template <class T>
  T* Get(std::string_view str_id_to_find) const {
    T* ptr_widget = GetOrNull<T>(str_id_to_find);
    if (ptr_widget == nullptr)
      throw std::runtime_error("Widget " + str_id + " has no child " +
                               std::string(str_id_to_find) + " of type " +
                               std::string(T::kWidgetTypeName));
    return ptr_widget;
  }

  Widget* Get(std::string_view str_id_to_find) const { return Get<Widget>(str_id_to_find); }

  const std::vector<std::unique_ptr<ChromeLogic>>& LogicObjects() const {
    return vec_logic_objects_;
  }

 protected:
  /// L244-265:Clone 基座;克隆体 IsVisible 委托读**源体** Visible(上游
  /// 委托拷贝语义,直至另行重绑)
  /// L244-265: the Clone base; the clone's IsVisible delegate reads the
  /// SOURCE's Visible (upstream delegate-copy semantics, until rebound).
  Widget(const Widget& widget_other);

 private:
  void ForceYieldMouseFocus();
  void ForceYieldKeyboardFocus();

  std::vector<std::unique_ptr<Widget>> vec_children_;
  std::vector<std::unique_ptr<ChromeLogic>> vec_logic_objects_;
  Mediator* ptr_mediator_ = nullptr;  // Removed 退订面 | the Removed unsubscribe face
  std::string str_default_cursor;     // 空 = C# null | empty = C# null
};

/// ContainerWidget(L633-652)
/// ContainerWidget (L633-652).
class ContainerWidget : public Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "ContainerWidget";

  ContainerWidget() { b_ignore_mouse_over = true; }
  explicit ContainerWidget(const ContainerWidget& widget_other);
  ~ContainerWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new ContainerWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  const std::string* GetCursor(int2) const override { return nullptr; }
  bool HandleMouseInput(const MouseInput& mi_input) override;
  bool LoadFieldOrProperty(std::string_view str_key,
                           std::string_view str_value) override;

  bool b_click_through = true;
};

/// InputWidget(L654-671)
/// InputWidget (L654-671).
class InputWidget : public Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "InputWidget";

  InputWidget() { fn_is_disabled = [this] { return b_disabled; }; }
  explicit InputWidget(const InputWidget& widget_other);
  ~InputWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new InputWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  bool LoadFieldOrProperty(std::string_view str_key,
                           std::string_view str_value) override;

  bool b_disabled = false;
  std::function<bool()> fn_is_disabled;
};

}  // namespace ora::ui

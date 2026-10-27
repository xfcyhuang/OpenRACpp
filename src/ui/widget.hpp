#pragma once
import std;

#include "core/color.hpp"
#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "meta/variable_expression.hpp"
#include "platform/sdl2_input.hpp"

namespace ora::yaml {
class MiniYaml;
}

namespace ora::sim {
class World;
class Player;
}
namespace ora::net {
class OrderManager;
class ConnectionTarget;
class NetworkConnection;
struct SessionClient;
}
namespace ora::server {
struct GameServerData;
struct GameInformationPlayer;
struct GameClientData;
}
namespace ora::gfx {
class WorldRenderer;
}
namespace ora::map {
class MapPreview;
struct MapGenerationArgs;
}
namespace ora::game {
class ModData;
}
namespace ora::game {
struct PlayerProfile;
}
namespace ora::mods {
class ISettingsLogic;
}

namespace ora::ui {
class MapPreviewWidget;
class TooltipContainerWidget;

class SpawnOccupant {
 public:
  core::Color color_player{};
  std::string str_player_name;
  int int4_team = 0;
  std::string str_faction;
  int int4_spawn_point = 0;

  SpawnOccupant(const net::SessionClient& client);
  SpawnOccupant(const server::GameInformationPlayer& player);
  SpawnOccupant(const server::GameClientData& player, bool b_suppress_faction);
};

class Widget;
class Ui;
class ChromeMetrics;
class Mediator;

using YamlDictionary = std::vector<std::pair<std::string_view, const yaml::MiniYaml*>>;

using GetMapFn = std::function<map::MapPreview*()>;
using WidgetArgValue =
    std::variant<std::monostate, int2, std::int32_t, bool, std::string, Widget*, sim::World*,
                 net::OrderManager*, gfx::WorldRenderer*, game::ModData*, ChromeMetrics*,
                 Mediator*, net::SessionClient*, core::Color, sim::Player*,
                 mods::ISettingsLogic*,
                 std::function<std::string()>,
                 std::function<void(core::Color)>,
                 std::function<void(Widget*)>,
                 std::function<void()>,
                 std::function<void(bool)>,
                 const std::map<std::string, std::int32_t>*,
                 const YamlDictionary*,
                 const std::vector<std::string>*,
                 std::function<void(const std::string&)>,
                 const std::string*,
                 net::ConnectionTarget*,
                 net::NetworkConnection*,
                 std::function<void(server::GameServerData&)>,
                 std::function<bool()>,
                 map::MapPreview*,
                 GetMapFn,
                 std::function<std::pair<map::MapPreview*, int>()>,
                 std::function<void(MapPreviewWidget*, map::MapPreview*,
                                    const MouseInput&)>,
                 std::function<std::map<int, SpawnOccupant>()>,
                 std::function<std::vector<int>()>,
                 MapPreviewWidget*,
                 TooltipContainerWidget*,
                const map::MapGenerationArgs*,
                std::function<void(const map::MapGenerationArgs&)>,
                game::PlayerProfile*,
                std::function<int(int)>>;

class WidgetArgs {
 public:
  WidgetArgs() = default;
  WidgetArgs(std::initializer_list<std::pair<std::string, WidgetArgValue>> init)
      : vec_entries_(init) {}

  bool ContainsKey(std::string_view str_key) const { return Find(str_key) != nullptr; }

  const WidgetArgValue& At(std::string_view str_key) const {
    const WidgetArgValue* ptr_value = Find(str_key);
    if (ptr_value == nullptr)
      throw std::runtime_error("The given key was not present in the dictionary.");
    return *ptr_value;
  }

  WidgetArgValue* TryGet(std::string_view str_key) { return Find(str_key); }
  const WidgetArgValue* TryGet(std::string_view str_key) const { return Find(str_key); }

  void Add(std::string str_key, WidgetArgValue val_value) {
    if (ContainsKey(str_key))
      throw std::runtime_error("An item with the same key has already been added.");
    vec_entries_.emplace_back(std::move(str_key), std::move(val_value));
  }

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

template <class T>
class INotificationHandler {
 public:
  static const std::type_index kNotificationId;
  virtual ~INotificationHandler() = default;
  virtual void Handle(const T& notification_notification) = 0;
};

template <class T>
const std::type_index INotificationHandler<T>::kNotificationId = typeid(T);

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
        vec_bucket.erase(it_entry);
        return;
      }
  }

  template <class T>
  void Send(const T& notification_notification) {

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

class ChromeLogic {
 public:
  virtual ~ChromeLogic() = default;
  virtual void Tick() {}
  virtual void BecameHidden() {}
  virtual void BecameVisible() {}
  virtual void SubscribeAll(Mediator& mediator_m) { (void)mediator_m; }
  virtual void UnsubscribeAll(Mediator& mediator_m) { (void)mediator_m; }
};

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

class ChromeLogicRegistry {
 public:
  using FactoryFn = std::function<std::unique_ptr<ChromeLogic>(WidgetArgs&)>;

  static ChromeLogicRegistry& Instance();

  void Register(std::string str_name, FactoryFn fn_factory);

  std::unique_ptr<ChromeLogic> Create(const std::string& str_name,
                                      WidgetArgs& args) const;

 private:
  std::vector<std::pair<std::string, FactoryFn>> vec_entries_;
};

#define ORA_REGISTER_CHROME_LOGIC(Cls)                          \
  const bool kChromeLogicRegistered_##Cls = [] {                \
    ::ora::ui::ChromeLogicRegistry::Instance().Register(        \
        #Cls, [](::ora::ui::WidgetArgs& args_args)              \
                  -> std::unique_ptr<::ora::ui::ChromeLogic> {  \
          return std::make_unique<Cls>(args_args);              \
        });                                                     \
    return true;                                                \
  }()

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

class Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "Widget";

  Widget();
  virtual ~Widget() = default;

  Widget& operator=(const Widget&) = delete;

  std::string str_id;
  std::optional<expr::IntegerExpression> expr_x;
  std::optional<expr::IntegerExpression> expr_y;
  std::optional<expr::IntegerExpression> expr_width;
  std::optional<expr::IntegerExpression> expr_height;
  std::vector<std::string> vec_logic;
  bool b_visible = true;
  bool b_ignore_mouse_over = false;
  bool b_ignore_child_mouse_over = false;

  WidgetBounds bounds_widget;
  Widget* ptr_parent = nullptr;
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

  virtual std::string_view TypeName() const { return kWidgetTypeName; }

  virtual std::unique_ptr<Widget> Clone() {
    throw std::runtime_error("Widget type `" + std::string(TypeName()) +
                             "` is not cloneable.");
  }

  virtual int2 RenderOrigin() const;
  virtual int2 ChildOrigin() const { return RenderOrigin(); }
  virtual Rectangle RenderBounds() const;

  virtual void Initialize(WidgetArgs& args);

  void PostInit(WidgetArgs& args);

  virtual bool LoadFieldOrProperty(std::string_view str_key, std::string_view str_value);

  virtual Rectangle EventBounds() const { return RenderBounds(); }
  virtual bool EventBoundsContains(int2 pt_location) const;

  bool HasMouseFocus() const;
  bool HasKeyboardFocus() const;

  virtual bool TakeMouseFocus(const MouseInput& mi_input);

  virtual bool YieldMouseFocus(const MouseInput& mi_input);
  virtual bool TakeKeyboardFocus();
  virtual bool YieldKeyboardFocus();

  virtual const std::string* GetCursor(int2 pt_pos) const;
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

  Widget(const Widget& widget_other);

  std::vector<std::unique_ptr<Widget>>& ChildrenOwned() { return vec_children_; }

 private:
  void ForceYieldMouseFocus();
  void ForceYieldKeyboardFocus();

  std::vector<std::unique_ptr<Widget>> vec_children_;
  std::vector<std::unique_ptr<ChromeLogic>> vec_logic_objects_;
  Mediator* ptr_mediator_ = nullptr;
  std::string str_default_cursor;
};

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

}

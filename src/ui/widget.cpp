import std;

#include "ui/chrome_metrics.hpp"
#include "ui/ui.hpp"
#include "ui/widget.hpp"

namespace ora::ui {

ChromeLogicRegistry& ChromeLogicRegistry::Instance() {
  static ChromeLogicRegistry instance_registry;
  return instance_registry;
}

void ChromeLogicRegistry::Register(std::string str_name, FactoryFn fn_factory) {
  vec_entries_.emplace_back(std::move(str_name), std::move(fn_factory));
}

std::unique_ptr<ChromeLogic> ChromeLogicRegistry::Create(const std::string& str_name,
                                                         WidgetArgs& args) const {
  for (const auto& [str_entry_name, fn_entry_factory] : vec_entries_)
    if (str_entry_name == str_name)
      return fn_entry_factory(args);

  throw std::runtime_error("Cannot locate type: " + str_name);
}

Widget::Widget() = default;

Widget::Widget(const Widget& widget_other)
    : str_id(widget_other.str_id),
      expr_x(widget_other.expr_x),
      expr_y(widget_other.expr_y),
      expr_width(widget_other.expr_width),
      expr_height(widget_other.expr_height),
      vec_logic(widget_other.vec_logic),
      b_visible(widget_other.b_visible),
      b_ignore_mouse_over(widget_other.b_ignore_mouse_over),
      b_ignore_child_mouse_over(widget_other.b_ignore_child_mouse_over),
      bounds_widget(widget_other.bounds_widget),
      ptr_parent(widget_other.ptr_parent),
      fn_is_visible(widget_other.fn_is_visible),
      str_default_cursor(widget_other.str_default_cursor) {
  for (const auto& ptr_child : widget_other.vec_children_)
    AddChild(ptr_child->Clone());
}

int2 Widget::RenderOrigin() const {
  const int2 int2_offset = ptr_parent == nullptr ? int2{} : ptr_parent->ChildOrigin();
  return int2{bounds_widget.int4_x, bounds_widget.int4_y} + int2_offset;
}

Rectangle Widget::RenderBounds() const {
  const int2 int2_ro = RenderOrigin();
  return Rectangle{int2_ro.X, int2_ro.Y, bounds_widget.int4_width,
                   bounds_widget.int4_height};
}

void Widget::Initialize(WidgetArgs& args) {
  if (const WidgetArgValue* ptr_value = args.TryGet("chromeMetrics")) {
    if (const auto* ptr_metrics = std::get_if<ChromeMetrics*>(ptr_value))
      str_default_cursor = (*ptr_metrics)->Get("DefaultCursor");
  }

  int2 int2_window{0, 0};
  if (const WidgetArgValue* ptr_value = args.TryGet("windowSize"))
    if (const auto* ptr_size = std::get_if<int2>(ptr_value))
      int2_window = *ptr_size;

  WidgetBounds bounds_parent;
  if (ptr_parent == nullptr)
    bounds_parent = WidgetBounds{0, 0, int2_window.X, int2_window.Y};
  else
    bounds_parent = ptr_parent->bounds_widget;

  std::map<std::string, std::int32_t> map_substitutions;
  if (const WidgetArgValue* ptr_value = args.TryGet("substitutions"))
    if (const auto* ptr_subs =
            std::get_if<const std::map<std::string, std::int32_t>*>(ptr_value))
      map_substitutions = **ptr_subs;

  const auto fn_add = [&](const std::string& str_key, std::int32_t int4_value) {

    if (!map_substitutions.emplace(str_key, int4_value).second)
      throw std::runtime_error("An item with the same key has already been added.");
  };

  fn_add("WINDOW_WIDTH", int2_window.X);
  fn_add("WINDOW_HEIGHT", int2_window.Y);
  fn_add("PARENT_WIDTH", bounds_parent.int4_width);
  fn_add("PARENT_HEIGHT", bounds_parent.int4_height);

  std::vector<std::pair<std::string, std::int32_t>> vec_symbols(
      map_substitutions.begin(), map_substitutions.end());
  const auto fn_evaluate =
      [&vec_symbols](const std::optional<expr::IntegerExpression>& expr_optional) {
        return expr_optional.has_value() ? expr_optional->Evaluate(vec_symbols) : 0;
      };

  const std::int32_t int4_width = fn_evaluate(expr_width);
  const std::int32_t int4_height = fn_evaluate(expr_height);

  fn_add("WIDTH", int4_width);
  fn_add("HEIGHT", int4_height);
  vec_symbols.assign(map_substitutions.begin(), map_substitutions.end());

  const std::int32_t int4_x = fn_evaluate(expr_x);
  const std::int32_t int4_y = fn_evaluate(expr_y);
  bounds_widget = WidgetBounds{int4_x, int4_y, int4_width, int4_height};
}

void Widget::PostInit(WidgetArgs& args) {
  if (vec_logic.empty())
    return;

  args.Set("widget", this);

  for (const std::string& str_logic : vec_logic)
    vec_logic_objects_.push_back(
        ChromeLogicRegistry::Instance().Create(str_logic, args));

  Mediator* ptr_mediator = nullptr;
  if (const WidgetArgValue* ptr_value = args.TryGet("mediator"))
    if (const auto* ptr_med = std::get_if<Mediator*>(ptr_value))
      ptr_mediator = *ptr_med;
  if (ptr_mediator == nullptr && Ui::HasInstance())
    ptr_mediator = &Ui::Instance().mediator();
  ptr_mediator_ = ptr_mediator;

  for (const auto& ptr_logic : vec_logic_objects_)
    ptr_logic->SubscribeAll(*ptr_mediator);

  args.Remove("widget");
}

bool Widget::LoadFieldOrProperty(std::string_view str_key, std::string_view str_value) {
  const auto fn_parse_bool = [](std::string_view str_text) {

    if (str_text == "true")
      return true;
    if (str_text == "false")
      return false;
    throw std::runtime_error("String was not recognized as a valid Boolean.");
  };

  if (str_key == "Id") {
    str_id = std::string(str_value);
    return true;
  }
  if (str_key == "X") {
    expr_x.emplace(std::string(str_value));
    return true;
  }
  if (str_key == "Y") {
    expr_y.emplace(std::string(str_value));
    return true;
  }
  if (str_key == "Width") {
    expr_width.emplace(std::string(str_value));
    return true;
  }
  if (str_key == "Height") {
    expr_height.emplace(std::string(str_value));
    return true;
  }
  if (str_key == "Logic") {

    vec_logic.clear();
    std::size_t int4_pos = 0;
    while (int4_pos <= str_value.size()) {
      const std::size_t int4_comma = str_value.find(',', int4_pos);
      const std::string_view str_part =
          str_value.substr(int4_pos, int4_comma == std::string_view::npos
                                         ? std::string_view::npos
                                         : int4_comma - int4_pos);
      const std::size_t int4_begin = str_part.find_first_not_of(" \t");
      const std::size_t int4_end = str_part.find_last_not_of(" \t");
      if (int4_begin != std::string_view::npos)
        vec_logic.emplace_back(str_part.substr(int4_begin, int4_end - int4_begin + 1));
      if (int4_comma == std::string_view::npos)
        break;
      int4_pos = int4_comma + 1;
    }
    return true;
  }
  if (str_key == "Visible") {
    b_visible = fn_parse_bool(str_value);
    return true;
  }
  if (str_key == "IgnoreMouseOver") {
    b_ignore_mouse_over = fn_parse_bool(str_value);
    return true;
  }
  if (str_key == "IgnoreChildMouseOver") {
    b_ignore_child_mouse_over = fn_parse_bool(str_value);
    return true;
  }

  return false;
}

bool Widget::EventBoundsContains(int2 pt_location) const {
  if (EventBounds().Contains(pt_location))
    return true;

  for (const auto& ptr_child : vec_children_)
    if (ptr_child->fn_is_visible() && ptr_child->EventBoundsContains(pt_location))
      return true;

  return false;
}

bool Widget::HasMouseFocus() const {
  return Ui::HasInstance() && Ui::Instance().MouseFocusWidget() == this;
}

bool Widget::HasKeyboardFocus() const {
  return Ui::HasInstance() && Ui::Instance().KeyboardFocusWidget() == this;
}

bool Widget::TakeMouseFocus(const MouseInput& mi_input) {
  if (HasMouseFocus())
    return true;

  Widget* ptr_focus = Ui::Instance().MouseFocusWidget();
  if (ptr_focus != nullptr && !ptr_focus->YieldMouseFocus(mi_input))
    return false;

  Ui::Instance().SetMouseFocusWidget(this);
  return true;
}

bool Widget::YieldMouseFocus(const MouseInput&) {
  if (Ui::Instance().MouseFocusWidget() == this)
    Ui::Instance().SetMouseFocusWidget(nullptr);

  return true;
}

void Widget::ForceYieldMouseFocus() {

  if (Ui::Instance().MouseFocusWidget() == this && !YieldMouseFocus(MouseInput{}))
    Ui::Instance().SetMouseFocusWidget(nullptr);
}

bool Widget::TakeKeyboardFocus() {
  if (HasKeyboardFocus())
    return true;

  Widget* ptr_focus = Ui::Instance().KeyboardFocusWidget();
  if (ptr_focus != nullptr && !ptr_focus->YieldKeyboardFocus())
    return false;

  Ui::Instance().SetKeyboardFocusWidget(this);
  return true;
}

bool Widget::YieldKeyboardFocus() {
  if (Ui::Instance().KeyboardFocusWidget() == this)
    Ui::Instance().SetKeyboardFocusWidget(nullptr);

  return true;
}

void Widget::ForceYieldKeyboardFocus() {
  if (Ui::Instance().KeyboardFocusWidget() == this && !YieldKeyboardFocus())
    Ui::Instance().SetKeyboardFocusWidget(nullptr);
}

const std::string* Widget::GetCursor(int2) const {
  return str_default_cursor.empty() ? nullptr : &str_default_cursor;
}

const std::string* Widget::GetCursorOuter(int2 pt_pos) const {
  if (!(fn_is_visible() && EventBoundsContains(pt_pos)))
    return nullptr;

  for (std::size_t int4_index = vec_children_.size(); int4_index-- > 0;) {
    const std::string* ptr_cursor = vec_children_[int4_index]->GetCursorOuter(pt_pos);
    if (ptr_cursor != nullptr)
      return ptr_cursor;
  }

  return EventBounds().Contains(pt_pos) ? GetCursor(pt_pos) : nullptr;
}

bool Widget::HandleMouseInputOuter(const MouseInput& mi_input) {
  if (!(HasMouseFocus() || (fn_is_visible() && EventBoundsContains(mi_input.Location))))
    return false;

  Widget* ptr_old_mouse_over = Ui::Instance().MouseOverWidget();

  for (std::size_t int4_index = vec_children_.size(); int4_index-- > 0;)
    if (vec_children_[int4_index]->HandleMouseInputOuter(mi_input))
      return true;

  if (b_ignore_child_mouse_over)
    Ui::Instance().SetMouseOverWidget(ptr_old_mouse_over);

  if (mi_input.Event == MouseInputEvent::Move &&
      Ui::Instance().MouseOverWidget() == nullptr && !b_ignore_mouse_over)
    Ui::Instance().SetMouseOverWidget(this);

  return HandleMouseInput(mi_input);
}

bool Widget::HandleKeyPressOuter(const KeyInput& e_input) {
  if (!fn_is_visible())
    return false;

  for (std::size_t int4_index = vec_children_.size(); int4_index-- > 0;)
    if (vec_children_[int4_index]->HandleKeyPressOuter(e_input))
      return true;

  return HandleKeyPress(e_input);
}

bool Widget::HandleTextInputOuter(std::string_view str_text) {
  if (!fn_is_visible())
    return false;

  for (std::size_t int4_index = vec_children_.size(); int4_index-- > 0;)
    if (vec_children_[int4_index]->HandleTextInputOuter(str_text))
      return true;

  return HandleTextInput(str_text);
}

void Widget::PrepareRenderablesOuter() {
  if (fn_is_visible()) {
    PrepareRenderables();
    for (const auto& ptr_child : vec_children_)
      ptr_child->PrepareRenderablesOuter();
  }
}

void Widget::DrawOuter() {
  if (fn_is_visible()) {
    Draw();
    for (const auto& ptr_child : vec_children_)
      ptr_child->DrawOuter();
  }
}

void Widget::TickOuter() {
  if (fn_is_visible()) {
    Tick();
    for (const auto& ptr_child : vec_children_)
      ptr_child->TickOuter();

    for (const auto& ptr_logic : vec_logic_objects_)
      ptr_logic->Tick();
  }
}

void Widget::AddChild(std::unique_ptr<Widget> ptr_child) {
  ptr_child->ptr_parent = this;
  vec_children_.push_back(std::move(ptr_child));
}

std::unique_ptr<Widget> Widget::RemoveChild(Widget& child_widget) {
  std::unique_ptr<Widget> ptr_detached;
  for (auto it_child = vec_children_.begin(); it_child != vec_children_.end(); ++it_child)
    if (it_child->get() == &child_widget) {
      ptr_detached = std::move(*it_child);
      vec_children_.erase(it_child);
      break;
    }

  child_widget.Removed();
  return ptr_detached;
}

std::unique_ptr<Widget> Widget::HideChild(Widget& child_widget) {
  std::unique_ptr<Widget> ptr_detached;
  for (auto it_child = vec_children_.begin(); it_child != vec_children_.end(); ++it_child)
    if (it_child->get() == &child_widget) {
      ptr_detached = std::move(*it_child);
      vec_children_.erase(it_child);
      break;
    }

  child_widget.Hidden();
  return ptr_detached;
}

void Widget::RemoveChildren() {
  for (const auto& ptr_child : vec_children_)
    if (ptr_child != nullptr)
      ptr_child->Removed();

  vec_children_.clear();
}

void Widget::Hidden() {

  ForceYieldKeyboardFocus();
  ForceYieldMouseFocus();

  for (std::size_t int4_index = vec_children_.size(); int4_index-- > 0;)
    vec_children_[int4_index]->Hidden();
}

void Widget::Removed() {
  ForceYieldKeyboardFocus();
  ForceYieldMouseFocus();

  for (std::size_t int4_index = vec_children_.size(); int4_index-- > 0;)
    vec_children_[int4_index]->Removed();

  if (ptr_mediator_ != nullptr) {
    for (const auto& ptr_logic : vec_logic_objects_)
      ptr_logic->UnsubscribeAll(*ptr_mediator_);
  }
  ptr_mediator_ = nullptr;

}

Widget* Widget::GetOrNull(std::string_view str_id_to_find) const {
  if (str_id == str_id_to_find)
    return const_cast<Widget*>(this);

  for (const auto& ptr_child : vec_children_) {
    Widget* ptr_widget = ptr_child->GetOrNull(str_id_to_find);
    if (ptr_widget != nullptr)
      return ptr_widget;
  }

  return nullptr;
}

ContainerWidget::ContainerWidget(const ContainerWidget& widget_other)
    : Widget(widget_other), b_click_through(widget_other.b_click_through) {
  b_ignore_mouse_over = true;
}

bool ContainerWidget::HandleMouseInput(const MouseInput& mi_input) {
  return !b_click_through && EventBounds().Contains(mi_input.Location);
}

bool ContainerWidget::LoadFieldOrProperty(std::string_view str_key,
                                          std::string_view str_value) {
  if (str_key == "ClickThrough") {
    if (str_value == "true")
      b_click_through = true;
    else if (str_value == "false")
      b_click_through = false;
    else
      throw std::runtime_error("String was not recognized as a valid Boolean.");
    return true;
  }
  return Widget::LoadFieldOrProperty(str_key, str_value);
}

InputWidget::InputWidget(const InputWidget& widget_other)
    : Widget(widget_other), b_disabled(widget_other.b_disabled) {

  fn_is_disabled = [ptr_source = &widget_other]() -> bool {
    return ptr_source->b_disabled;
  };
}

bool InputWidget::LoadFieldOrProperty(std::string_view str_key,
                                      std::string_view str_value) {
  if (str_key == "Disabled") {
    if (str_value == "true")
      b_disabled = true;
    else if (str_value == "false")
      b_disabled = false;
    else
      throw std::runtime_error("String was not recognized as a valid Boolean.");
    return true;
  }
  return Widget::LoadFieldOrProperty(str_key, str_value);
}

}

#pragma once
import std;

#include "fluent/fluent_bundle.hpp"
#include "ui/widget.hpp"

namespace ora::mods {

using FluentArgs = std::vector<std::pair<std::string, fluent::FluentValue>>;

void ButtonPrompt(const std::string& str_title, const std::string& str_text,
                  std::function<void()> fn_on_confirm = nullptr,
                  const std::string& str_confirm_text = {},
                  std::function<void()> fn_on_cancel = nullptr,
                  const std::string& str_cancel_text = {},
                  std::function<void()> fn_on_other = nullptr,
                  const std::string& str_other_text = {},
                  const FluentArgs& vec_text_arguments = {},
                  const FluentArgs& vec_title_arguments = {});

void TextInputPrompt(const std::string& str_title,
                     const std::string& str_prompt,
                     const std::string& str_initial_text,
                     std::function<void(std::string)> fn_on_accept,
                     std::function<void()> fn_on_cancel = nullptr,
                     const std::string& str_accept_text = {},
                     const std::string& str_cancel_text = {},
                     std::function<bool(const std::string&)> fn_input_validator =
                         nullptr);

}

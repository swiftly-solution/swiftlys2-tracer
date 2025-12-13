#pragma once

#include <print>
#include <format>

template <class... Args>
constexpr void LOG(std::format_string<Args...> fmt, Args &&...args)
{
  std::println("[SW2 TRACER] {}", std::format(fmt, std::forward<Args>(args)...));
};
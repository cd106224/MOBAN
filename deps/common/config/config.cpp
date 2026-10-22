#include "config.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <type_traits>

#include "config_parser.h"

namespace common::config {

namespace {

bool is_index(std::string_view segment) {
  if (segment.empty()) return false;
  return std::all_of(segment.begin(), segment.end(),
                     [](unsigned char c) { return std::isdigit(c) != 0; });
}

}  // namespace

bool Config::load_file(std::string_view path) {
  std::ifstream file{std::string(path), std::ios::binary};
  if (!file) {
    throw ConfigError("cannot open config file '" + std::string(path) + "'");
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();
  if (file.bad()) {
    throw ConfigError("cannot read config file '" + std::string(path) + "'");
  }

  const std::string content = buffer.str();
  const ConfigFormat resolved = (format_ == ConfigFormat::Auto)
                                    ? config_format_from_extension(path)
                                    : format_;

  return load_string(content, resolved);
}

bool Config::load_string(std::string_view content, ConfigFormat format) {
  const ConfigFormat resolved =
      (format == ConfigFormat::Auto) ? format_ : format;

  root_ = parse_config(content, resolved);

  format_ = resolved;
  loaded_ = true;
  return true;
}

const ConfigValue* Config::find(std::string_view path) const {
  if (!loaded_) throw ConfigError("config not loaded");
  if (path.empty()) return &root_;

  const ConfigValue* current = &root_;
  size_t position = 0;

  while (true) {
    const size_t dot = path.find('.', position);
    const std::string_view segment = path.substr(
        position, dot == std::string_view::npos ? std::string_view::npos
                                                : dot - position);

    if (segment.empty()) return nullptr;

    if (current->type() == ConfigValue::Type::Array && is_index(segment)) {
      // 纯数字段在数组上按索引用；位数超过 19 必然超出 size_t，直接判为越界，
      // 免得 stoull 抛 std::out_of_range 泄漏出 ConfigError 之外。
      if (segment.size() > 19) return nullptr;
      const ConfigArray& items = current->as_array();
      const size_t index = std::stoull(std::string(segment));
      if (index >= items.size()) return nullptr;
      current = &items[index];
    } else if (current->type() == ConfigValue::Type::Object) {
      const ConfigObject& fields = current->as_object();
      auto it = std::find_if(
          fields.begin(), fields.end(),
          [&segment](const auto& field) { return field.first == segment; });
      if (it == fields.end()) return nullptr;
      current = &it->second;
    } else {
      return nullptr;
    }

    if (dot == std::string_view::npos) return current;
    position = dot + 1;
  }
}

const ConfigValue& Config::node(std::string_view path) const {
  if (const ConfigValue* found = find(path)) return *found;
  throw ConfigError("config key '" + std::string(path) + "' not found");
}

template <typename T>
T Config::get(std::string_view path) const {
  const ConfigValue& value = node(path);

  if constexpr (std::is_same_v<T, bool>) {
    return value.as_bool();
  } else if constexpr (std::is_integral_v<T> && !std::is_signed_v<T>) {
    const int64_t raw = value.as_int();
    if (raw < 0) {
      throw ConfigError((value.path().empty() ? "<root>" : value.path()) +
                        ": value is negative, cannot read as unsigned");
    }
    return static_cast<T>(raw);
  } else if constexpr (std::is_integral_v<T>) {
    return static_cast<T>(value.as_int());
  } else if constexpr (std::is_floating_point_v<T>) {
    return static_cast<T>(value.as_double());
  } else if constexpr (std::is_same_v<T, std::string>) {
    return value.as_string();
  } else {
    static_assert(sizeof(T) == 0, "Config::get<T>: unsupported type T");
  }
}

template <typename T>
T Config::get_or(std::string_view path, T fallback) const {
  if (!has(path)) return fallback;
  return get<T>(path);
}

template bool Config::get<bool>(std::string_view) const;
template int32_t Config::get<int32_t>(std::string_view) const;
template uint32_t Config::get<uint32_t>(std::string_view) const;
template int64_t Config::get<int64_t>(std::string_view) const;
template uint64_t Config::get<uint64_t>(std::string_view) const;
template float Config::get<float>(std::string_view) const;
template double Config::get<double>(std::string_view) const;
template std::string Config::get<std::string>(std::string_view) const;

template bool Config::get_or<bool>(std::string_view, bool) const;
template int32_t Config::get_or<int32_t>(std::string_view, int32_t) const;
template uint32_t Config::get_or<uint32_t>(std::string_view, uint32_t) const;
template int64_t Config::get_or<int64_t>(std::string_view, int64_t) const;
template uint64_t Config::get_or<uint64_t>(std::string_view, uint64_t) const;
template float Config::get_or<float>(std::string_view, float) const;
template double Config::get_or<double>(std::string_view, double) const;
template std::string Config::get_or<std::string>(std::string_view,
                                                 std::string) const;

}  // namespace common::config

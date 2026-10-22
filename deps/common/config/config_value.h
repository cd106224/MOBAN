#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace common::config {

class ConfigError : public std::runtime_error {
 public:
  explicit ConfigError(const std::string& what) : std::runtime_error(what) {}
};

class ConfigValue;

using ConfigArray = std::vector<ConfigValue>;
using ConfigObject = std::vector<std::pair<std::string, ConfigValue>>;

// 格式无关的配置值树。各解析后端（Json/Xml）负责把自己的文档转成这棵树，
// 上层读取逻辑只认 ConfigValue，不感知具体格式。
class ConfigValue {
 public:
  enum class Type { Null, Bool, Int, Double, String, Array, Object };

  // Array/Object 用 shared_ptr 间接持有：ConfigValue 是递归类型，直接放进
  // variant 会要求类型在定义自身时已经完整。
  using Storage = std::variant<std::monostate, bool, int64_t, double,
                               std::string, std::shared_ptr<const ConfigArray>,
                               std::shared_ptr<const ConfigObject>>;

  ConfigValue() = default;

  static ConfigValue make_null(std::string path = {});
  static ConfigValue make_bool(bool value, std::string path = {});
  static ConfigValue make_int(int64_t value, std::string path = {});
  static ConfigValue make_double(double value, std::string path = {});
  static ConfigValue make_string(std::string value, std::string path = {});
  static ConfigValue make_array(ConfigArray items, std::string path = {});
  // 对象内不允许重复 key，重复时抛 ConfigError（错误信息带该字段的点号路径）。
  // 校验放在值树这一层而不是各个解析后端里，保证所有格式的判定一致。
  static ConfigValue make_object(ConfigObject fields, std::string path = {});

  // 拼出子节点的点号路径：父路径为空时就是 key 本身，否则 "parent.key"。
  // 各解析后端用它给每个节点标注路径，路径同时也是错误信息里的定位信息。
  static std::string child_path(std::string_view parent, std::string_view key);

  Type type() const noexcept { return static_cast<Type>(value_.index()); }
  bool is_null() const noexcept { return type() == Type::Null; }
  bool is_container() const noexcept {
    return type() == Type::Array || type() == Type::Object;
  }

  // 取值。类型不符抛 ConfigError，错误信息带上该节点的点号路径。
  bool as_bool() const;
  int64_t as_int() const;
  double as_double() const;  // Int 会隐式放宽为 Double
  const std::string& as_string() const;

  const ConfigArray& as_array() const;
  const ConfigObject& as_object() const;

  // 取子节点，不存在抛 ConfigError（先 has/或用 Config::find 探测）。
  const ConfigValue& operator[](std::string_view key) const;
  const ConfigValue& operator[](size_t index) const;

  bool has(std::string_view key) const;
  size_t size() const;  // 仅 Array/Object
  std::vector<std::string> keys() const;

  // 该节点在整棵树中的点号路径，用于错误信息。
  const std::string& path() const noexcept { return path_; }

 private:
  ConfigValue(Storage value, std::string path)
      : value_(std::move(value)), path_(std::move(path)) {}

  [[noreturn]] void throw_type_error(std::string_view expected) const;
  [[noreturn]] void throw_missing_key(std::string_view key) const;

  Storage value_ = std::monostate{};
  std::string path_;
};

}  // namespace common::config

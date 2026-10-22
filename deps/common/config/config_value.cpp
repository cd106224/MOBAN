#include "config_value.h"

#include <sstream>

namespace common::config {

namespace {

const char* type_name(ConfigValue::Type type) {
  switch (type) {
    case ConfigValue::Type::Null:
      return "null";
    case ConfigValue::Type::Bool:
      return "bool";
    case ConfigValue::Type::Int:
      return "int";
    case ConfigValue::Type::Double:
      return "double";
    case ConfigValue::Type::String:
      return "string";
    case ConfigValue::Type::Array:
      return "array";
    case ConfigValue::Type::Object:
      return "object";
  }
  return "unknown";
}

}  // namespace

ConfigValue ConfigValue::make_null(std::string path) {
  return ConfigValue(Storage{std::monostate{}}, std::move(path));
}

ConfigValue ConfigValue::make_bool(bool value, std::string path) {
  return ConfigValue(Storage{value}, std::move(path));
}

ConfigValue ConfigValue::make_int(int64_t value, std::string path) {
  return ConfigValue(Storage{value}, std::move(path));
}

ConfigValue ConfigValue::make_double(double value, std::string path) {
  return ConfigValue(Storage{value}, std::move(path));
}

ConfigValue ConfigValue::make_string(std::string value, std::string path) {
  return ConfigValue(Storage{std::move(value)}, std::move(path));
}

ConfigValue ConfigValue::make_array(ConfigArray items, std::string path) {
  return ConfigValue(
      Storage{std::make_shared<const ConfigArray>(std::move(items))},
      std::move(path));
}

ConfigValue ConfigValue::make_object(ConfigObject fields, std::string path) {
  // 配置是人手写的，同一层出现同名 key 基本都是复制粘贴事故。两个值
  // 通常还不相同，静默按"取第一个/最后一个"处理会让运行时行为取决于
  // 哪一行在前，因此这里直接报错。线性扫描即可，配置对象的 key 数量很小。
  for (size_t i = 0; i < fields.size(); ++i) {
    for (size_t j = i + 1; j < fields.size(); ++j) {
      if (fields[i].first != fields[j].first) continue;
      const std::string& where = fields[j].second.path();
      throw ConfigError(
          (where.empty() ? child_path(path, fields[j].first) : where) +
          ": duplicate key");
    }
  }

  return ConfigValue(
      Storage{std::make_shared<const ConfigObject>(std::move(fields))},
      std::move(path));
}

std::string ConfigValue::child_path(std::string_view parent,
                                    std::string_view key) {
  if (parent.empty()) return std::string(key);
  std::string result(parent);
  result.push_back('.');
  result.append(key);
  return result;
}

bool ConfigValue::as_bool() const {
  if (const auto* value = std::get_if<bool>(&value_)) return *value;
  throw_type_error("bool");
}

int64_t ConfigValue::as_int() const {
  if (const auto* value = std::get_if<int64_t>(&value_)) return *value;
  throw_type_error("int");
}

double ConfigValue::as_double() const {
  // 整数放宽成 double 是无损且常见的场景，放行；反向（double 截断成 int）会
  // 悄悄丢精度，故不放行。
  if (const auto* value = std::get_if<double>(&value_)) return *value;
  if (const auto* value = std::get_if<int64_t>(&value_)) {
    return static_cast<double>(*value);
  }
  throw_type_error("double");
}

const std::string& ConfigValue::as_string() const {
  if (const auto* value = std::get_if<std::string>(&value_)) return *value;
  throw_type_error("string");
}

const ConfigArray& ConfigValue::as_array() const {
  if (const auto* value =
          std::get_if<std::shared_ptr<const ConfigArray>>(&value_)) {
    return **value;
  }
  throw_type_error("array");
}

const ConfigObject& ConfigValue::as_object() const {
  if (const auto* value =
          std::get_if<std::shared_ptr<const ConfigObject>>(&value_)) {
    return **value;
  }
  throw_type_error("object");
}

const ConfigValue& ConfigValue::operator[](std::string_view key) const {
  const ConfigObject& fields = as_object();
  for (const auto& field : fields) {
    if (field.first == key) return field.second;
  }
  throw_missing_key(key);
}

const ConfigValue& ConfigValue::operator[](size_t index) const {
  const ConfigArray& items = as_array();
  if (index >= items.size()) {
    std::ostringstream oss;
    oss << (path_.empty() ? "<root>" : path_) << ": index " << index
        << " out of range, size is " << items.size();
    throw ConfigError(oss.str());
  }
  return items[index];
}

bool ConfigValue::has(std::string_view key) const {
  if (type() != Type::Object) return false;
  const ConfigObject& fields = as_object();
  for (const auto& field : fields) {
    if (field.first == key) return true;
  }
  return false;
}

size_t ConfigValue::size() const {
  if (const auto* value =
          std::get_if<std::shared_ptr<const ConfigArray>>(&value_)) {
    return (*value)->size();
  }
  if (const auto* value =
          std::get_if<std::shared_ptr<const ConfigObject>>(&value_)) {
    return (*value)->size();
  }
  throw_type_error("array or object");
}

std::vector<std::string> ConfigValue::keys() const {
  std::vector<std::string> result;
  const ConfigObject& fields = as_object();
  result.reserve(fields.size());
  for (const auto& field : fields) result.push_back(field.first);
  return result;
}

void ConfigValue::throw_type_error(std::string_view expected) const {
  std::ostringstream oss;
  if (!path_.empty()) oss << path_ << ": ";
  oss << "expected " << expected << ", got " << type_name(type());
  throw ConfigError(oss.str());
}

void ConfigValue::throw_missing_key(std::string_view key) const {
  std::ostringstream oss;
  oss << (path_.empty() ? "<root>" : path_) << ": key '" << key
      << "' not found";
  throw ConfigError(oss.str());
}

}  // namespace common::config

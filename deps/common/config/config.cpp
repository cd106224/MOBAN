#include "config.h"

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <type_traits>

namespace {

std::string join_path(std::string_view parent, std::string_view key) {
  if (parent.empty()) return std::string(key);
  std::string result(parent);
  result.push_back('.');
  result.append(key);
  return result;
}

ConfigValue convert(const rapidjson::Value& node, std::string path) {
  switch (node.GetType()) {
    case rapidjson::kNullType:
      return ConfigValue::make_null(std::move(path));

    case rapidjson::kTrueType:
      return ConfigValue::make_bool(true, std::move(path));

    case rapidjson::kFalseType:
      return ConfigValue::make_bool(false, std::move(path));

    case rapidjson::kNumberType: {
      // rapidjson 用 IsInt64/IsUint64 判断，先按无符号、再按有符号、最后回落
      // double，这样 2121 读出来是 int 而不是 double。
      if (node.IsUint64()) {
        return ConfigValue::make_int(static_cast<int64_t>(node.GetUint64()),
                                     std::move(path));
      }
      if (node.IsInt64()) {
        return ConfigValue::make_int(node.GetInt64(), std::move(path));
      }
      return ConfigValue::make_double(node.GetDouble(), std::move(path));
    }

    case rapidjson::kStringType:
      return ConfigValue::make_string(node.GetString(), std::move(path));

    case rapidjson::kArrayType: {
      ConfigArray items;
      items.reserve(node.Size());
      size_t index = 0;
      for (const auto& item : node.GetArray()) {
        items.push_back(convert(item, join_path(path, std::to_string(index))));
        ++index;
      }
      return ConfigValue::make_array(std::move(items), std::move(path));
    }

    case rapidjson::kObjectType: {
      ConfigObject fields;
      fields.reserve(node.MemberCount());
      for (const auto& member : node.GetObject()) {
        const std::string_view name = member.name.GetString();

        // 配置是人手写的，同一层出现同名 key 基本都是复制粘贴事故。两个值
        // 通常还不相同，静默按"取第一个/最后一个"处理会让运行时行为取决于
        // 哪一行在前，因此这里直接报错。线性扫描即可，配置对象的 key 数量很小。
        for (const auto& field : fields) {
          if (field.first == name) {
            throw ConfigError(join_path(path, name) + ": duplicate key");
          }
        }

        fields.emplace_back(std::string(name),
                            convert(member.value, join_path(path, name)));
      }
      return ConfigValue::make_object(std::move(fields), std::move(path));
    }

    default:
      throw ConfigError("json: unsupported node type at " +
                        (path.empty() ? std::string("<root>") : path));
  }
}

const char* json_type_name(rapidjson::Type type) {
  switch (type) {
    case rapidjson::kNullType:
      return "null";
    case rapidjson::kFalseType:
    case rapidjson::kTrueType:
      return "bool";
    case rapidjson::kNumberType:
      return "number";
    case rapidjson::kStringType:
      return "string";
    case rapidjson::kArrayType:
      return "array";
    case rapidjson::kObjectType:
      return "object";
    default:
      return "unknown";
  }
}

ConfigValue parse_json(std::string_view content) {
  rapidjson::Document document;
  // 配置文件是人写的，放开注释和尾随逗号，少一份因为手误而启动失败的理由。
  document.Parse<rapidjson::kParseCommentsFlag |
                 rapidjson::kParseTrailingCommasFlag>(content.data(),
                                                      content.size());

  if (document.HasParseError()) {
    std::ostringstream oss;
    oss << "json parse error at offset " << document.GetErrorOffset() << ": "
        << rapidjson::GetParseError_En(document.GetParseError());
    throw ConfigError(oss.str());
  }

  if (!document.IsObject()) {
    throw ConfigError("json root must be an object, got " +
                      std::string(json_type_name(document.GetType())));
  }
  return convert(document, std::string());
}

bool is_index(std::string_view segment) {
  if (segment.empty()) return false;
  return std::all_of(segment.begin(), segment.end(),
                     [](unsigned char c) { return std::isdigit(c) != 0; });
}

ConfigFormat format_from_extension(std::string_view path) {
  const size_t dot = path.rfind('.');
  const std::string_view extension = (dot == std::string_view::npos)
                                         ? std::string_view()
                                         : path.substr(dot + 1);

  if (extension == "json") return ConfigFormat::Json;
  throw ConfigError("cannot infer config format from file '" +
                    std::string(path) + "', only .json is supported");
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
  const ConfigFormat resolved =
      (format_ == ConfigFormat::Auto) ? format_from_extension(path) : format_;

  return load_string(content, resolved);
}

bool Config::load_string(std::string_view content, ConfigFormat format) {
  const ConfigFormat resolved =
      (format == ConfigFormat::Auto) ? format_ : format;

  switch (resolved) {
    case ConfigFormat::Auto:
    case ConfigFormat::Json:
      root_ = parse_json(content);
      break;
  }

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

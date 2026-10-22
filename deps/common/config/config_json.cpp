#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>

#include "config_parser.h"

namespace {

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
        items.push_back(convert(
            item, ConfigValue::child_path(path, std::to_string(index))));
        ++index;
      }
      return ConfigValue::make_array(std::move(items), std::move(path));
    }

    case rapidjson::kObjectType: {
      ConfigObject fields;
      fields.reserve(node.MemberCount());
      for (const auto& member : node.GetObject()) {
        const std::string name(member.name.GetString());
        fields.emplace_back(
            name, convert(member.value, ConfigValue::child_path(path, name)));
      }
      // 重复 key 的判定在 make_object 里做，所有格式共用同一套规则。
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

}  // namespace

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

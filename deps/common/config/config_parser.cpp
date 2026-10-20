#include "config_parser.h"

ConfigValue parse_config(std::string_view content, ConfigFormat format) {
  switch (format) {
    case ConfigFormat::Json:
      return parse_json(content);

    case ConfigFormat::Auto:
      break;
  }

  // Auto 只在按扩展名推断时才有意义，内存里的文本没有文件名可依据。
  throw ConfigError(
      "cannot infer config format for in-memory content, "
      "specify the format explicitly");
}

ConfigFormat config_format_from_extension(std::string_view path) {
  const size_t dot = path.rfind('.');
  const std::string_view extension = (dot == std::string_view::npos)
                                         ? std::string_view()
                                         : path.substr(dot + 1);

  if (extension == "json") return ConfigFormat::Json;

  // 新增格式时记得把这里的支持列表补上。
  throw ConfigError("cannot infer config format from file '" +
                    std::string(path) + "', supported extensions: .json");
}

#pragma once

#include <string_view>

#include "config.h"
#include "config_value.h"

// 这里是"认识所有格式"的地方。各格式后端负责把自己的文本转成格式无关的
// ConfigValue 树，第三方库（rapidjson 之类）只出现在对应后端的 .cpp 里。
//
// 新增一种格式（例如 Xml）需要三步，Config 本身不用改：
//   1. 在本文件加一个 parse_xxx 声明；
//   2. 新建 config_xxx.cpp 实现它；
//   3. 在 parse_config() 里加一个分支，同时在 ConfigFormat 里加一个枚举项。

namespace common::config {

// Json 后端：把 JSON 文本转成 ConfigValue 树，失败抛 ConfigError。
ConfigValue parse_json(std::string_view content);

// 按格式分发到对应后端。ConfigFormat::Auto 没有可分发的目标，抛 ConfigError。
ConfigValue parse_config(std::string_view content, ConfigFormat format);

// 从文件扩展名推断格式，认不出来抛 ConfigError。
ConfigFormat config_format_from_extension(std::string_view path);

}  // namespace common::config

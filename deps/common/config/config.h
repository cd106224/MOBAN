#pragma once

#include <string_view>

#include "config_value.h"

// 配置的来源格式。解析由 config_parser.h 里的后端分发完成，Config 本身
// 不感知具体格式：新增 Xml 后端时只要在 config_parser.h 里加一个解析入口
// 和一个枚举项，这里的读取逻辑无需改动。
enum class ConfigFormat { Auto, Json };

class Config {
 public:
  explicit Config(ConfigFormat format = ConfigFormat::Auto) noexcept
      : format_(format) {}

  // 读取失败（文件不存在、格式错误、根节点不是对象）一律抛 ConfigError。
  // 返回 true 表示加载成功。
  bool load_file(std::string_view path);
  bool load_string(std::string_view content, ConfigFormat format);

  bool loaded() const noexcept { return loaded_; }
  ConfigFormat format() const noexcept { return format_; }
  const ConfigValue& root() const noexcept { return root_; }

  // 点号路径访问，数组索引用数字段表示，如 "servers.0.host"。
  // 空路径返回根节点。
  const ConfigValue* find(std::string_view path) const;
  const ConfigValue& node(std::string_view path) const;
  bool has(std::string_view path) const { return find(path) != nullptr; }

  // 只支持显式实例化过的那几个类型：bool、int32_t/uint32_t、int64_t/uint64_t、
  // float、double、std::string。定义在 .cpp 里，不暴露到头文件。
  template <typename T>
  T get(std::string_view path) const;

  // 路径不存在返回 fallback；存在但类型不符仍然抛错——那是配置写错了，
  // 静默兜底会把问题埋到运行期。
  template <typename T>
  T get_or(std::string_view path, T fallback) const;

 private:
  ConfigFormat format_;
  ConfigValue root_;
  bool loaded_ = false;
};

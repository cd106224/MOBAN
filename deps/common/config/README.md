# MOBAN Common Config

`deps/common/config/` 是配置读取模块：**格式解析**与**配置读取**分离，格式后端（当前
只有 JSON）负责把原始文本转成格式无关的值树，上层读取逻辑只认这棵值树。

新增一种格式（XML/YAML/TOML）不需要改动 `Config` 的读取逻辑，只要在
`config_parser.h` 里加一个后端入口。

**命名空间**：本目录所有对外类型都在 `common::config` 下（`Config`、`ConfigFormat`、
`ConfigValue`、`ConfigArray`/`ConfigObject`、`ConfigError`，以及格式分发函数
`parse_json()` / `parse_config()` / `config_format_from_extension()`）。
调用方需显式限定，不提供全局别名。

---

## 1. 文件与职责

| 文件 | 内容 |
| --- | --- |
| `config_value.h` | `ConfigValue` 值树、`ConfigArray`/`ConfigObject`、`ConfigError` |
| `config.h` | `Config` 读取入口、`ConfigFormat` 枚举 |
| `config_parser.h` | 格式分发：`parse_config()`、`parse_json()`、`config_format_from_extension()` |
| `config_json.cpp` | JSON 后端（**唯一**出现 rapidjson 的地方，`PRIVATE` 链接） |
| `config_value.cpp` / `config.cpp` | 值树与读取逻辑的实现 |

依赖方向：`config.cpp` → `config_parser.h` → 后端 `.cpp`。公共头文件里不出现任何
第三方类型。

---

## 2. 对外接口速查

下列类型与函数都在 `common::config` 命名空间下（签名里省略该限定）。

### 2.1 `Config`

```cpp
class Config {
 public:
  explicit Config(ConfigFormat format = ConfigFormat::Auto) noexcept;

  bool load_file(std::string_view path);
  bool load_string(std::string_view content, ConfigFormat format);

  bool loaded() const noexcept;
  ConfigFormat format() const noexcept;
  const ConfigValue& root() const noexcept;

  const ConfigValue* find(std::string_view path) const;   // 找不到返回 nullptr
  const ConfigValue& node(std::string_view path) const;   // 找不到抛 ConfigError
  bool has(std::string_view path) const;

  template <typename T> T get(std::string_view path) const;
  template <typename T> T get_or(std::string_view path, T fallback) const;
};
```

用法：

```cpp
common::config::Config config;
config.load_file("etc/server.json");          // 格式按扩展名推断

const int port = config.get<int>("server.port");
const std::string host = config.get<std::string>("server.host");
const int timeout = config.get_or<int>("server.timeout", 30);
```

### 2.2 `ConfigValue`

```cpp
class ConfigValue {
 public:
  enum class Type { Null, Bool, Int, Double, String, Array, Object };

  bool as_bool() const;
  int64_t as_int() const;
  double as_double() const;                   // Int 会放宽成 Double
  const std::string& as_string() const;
  const ConfigArray& as_array() const;
  const ConfigObject& as_object() const;

  const ConfigValue& operator[](std::string_view key) const;   // 对象成员
  const ConfigValue& operator[](size_t index) const;           // 数组元素

  bool has(std::string_view key) const;
  size_t size() const;                        // 仅 Array/Object
  std::vector<std::string> keys() const;      // 仅 Object

  const std::string& path() const noexcept;   // 该节点的点号路径
};
```

遍历示例：

```cpp
const common::config::ConfigValue& servers = config.node("servers");
for (const std::string& key : servers.keys()) {
  const common::config::ConfigValue& item = servers[key];
  // item["host"].as_string()
}
```

### 2.3 `ConfigError`

```cpp
class ConfigError : public std::runtime_error { ... };
```

配置相关的所有失败（文件打不开、格式错误、根节点不是对象、路径不存在、类型不符、
重复 key）统一抛这个异常，异常信息里带**点号路径**用于定位。

---

## 3. 语义与注意事项

### 3.1 点号路径

- 路径用 `.` 分隔层级，数组索引用数字段表示：`"servers.0.host"`。
- 空路径返回根节点。
- **对象上的纯数字段按 key 处理，不是数组下标**：`{"0": ...}` 要用
  `node("0")` 取，只有当前节点确实是数组时才按下标解释。
- 路径段为空（如 `"a..b"`）视为查不到，返回 `nullptr`。

### 3.2 加载

- `load_file` 在 `ConfigFormat::Auto` 下按**文件扩展名**推断格式；扩展名不认识就抛
  `ConfigError`。扩展名匹配**区分大小写**，只有 `.json` 被支持。
- `load_string` 的 `format` 参数不能是 `Auto`——内存里的文本没有文件名可依据。
  `Auto` 时会退回 `Config` 构造时指定的格式；构造时也是 `Auto` 就抛 `ConfigError`。
- 加载成功返回 `true`，`loaded()` 之后为 `true`；重新加载会**整体替换**上一棵树。
- **未加载就访问**（`find`/`node`/`get`）抛 `ConfigError("config not loaded")`。
- JSON 后端开启了 rapidjson 的 `kParseCommentsFlag` 和 `kParseTrailingCommasFlag`：
  允许注释和尾随逗号（配置文件是人写的，少一份因为手误启动失败的理由）。
- JSON **根节点必须是对象**，否则抛 `ConfigError`。

### 3.3 取值与类型

- `get<T>` 只支持显式实例化过的类型：`bool`、`int32_t`/`uint32_t`、
  `int64_t`/`uint64_t`、`float`、`double`、`std::string`。其他类型（如 `size_t`、
  `unsigned long`）**链接期报错**，不是编译期。
- 读无符号类型时值为负会抛 `ConfigError`（不静默回绕）。
- `get_or` **只在路径不存在时**用 fallback；路径存在但类型不符仍然抛错——
  那是配置写错了，静默兜底会把问题埋到运行期。
- `as_double()` 对整数是**无损放宽**（Int → Double 放行，Double → Int 不放行）。

### 3.4 值树

- `ConfigValue` 的数组/对象成员用 `shared_ptr<const ...>` 间接持有（递归类型不能
  直接放进 `variant`）。
- **对象不允许重复 key**：`make_object` 线性扫描检查，重复时抛 `ConfigError` 并报出
  完整点号路径。校验放在值树这一层，所以所有格式共用同一套规则。
- 每个节点都带自己的点号路径（`path()`），由后端在构造时用
  `ConfigValue::child_path()` 拼出，同时也是错误信息里的定位信息。

---

## 4. 扩展一种新格式

以 XML 为例，三步，`Config` 本身不用改：

1. 在 `config_parser.h` 加 `ConfigValue parse_xml(std::string_view content);`
2. 新建 `config_xml.cpp` 实现它——第三方 XML 库只出现在这个 `.cpp` 里，
   并在 `CMakeLists.txt` 里 `PRIVATE` 链接；
3. 在 `config_parser.cpp` 的 `parse_config()` 里加分支，并在 `ConfigFormat` 里加枚举项；
   若要支持按扩展名推断，还要在 `config_format_from_extension()` 里补上扩展名。

---

## 5. 测试覆盖现状

测试在 `deps/common/tests/config_test/`，数据文件在同目录 `data/`，路径通过
`MOBAN_TEST_DATA_DIR` 编译期宏传入（不依赖运行时工作目录）。

`config_test.cpp` 共 24 个用例，覆盖：完整服务端配置的加载与各类标量读取、Int→Double
放宽、对象数组、对象 key 列举、注释与尾随逗号、数字 key 与数组下标的区分、缺逗号、
根节点非对象、同层重复 key（顶层与嵌套）、文件不存在、扩展名不认识、类型不符、
数组下标越界、超长下标、加载前访问、路径不存在、`get_or` 兜底、重新加载、
`load_string` 显式指定格式、`Auto` 无法推断等。

#include "config/config.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace {

// 样例配置文件在 tests/data 下，路径由 CMake 以编译期宏传进来，
// 这样用例不依赖运行时的工作目录。
std::string DataPath(std::string_view name) {
  return std::string(MOBAN_TEST_DATA_DIR) + "/" + std::string(name);
}

Config LoadDataFile(std::string_view name) {
  Config config;
  config.load_file(DataPath(name));
  return config;
}

}  // namespace

// ==================== 读取完整配置 ====================
TEST(ConfigTest, LoadsFullServerConfig) {
  Config config = LoadDataFile("server.json");

  EXPECT_TRUE(config.loaded());
  EXPECT_EQ(config.format(), ConfigFormat::Json);
  EXPECT_EQ(config.get<std::string>("server.name"), "MOBAN Backend");
  EXPECT_EQ(config.get<std::string>("server.listen.host"), "0.0.0.0");
  EXPECT_EQ(config.get<int32_t>("server.listen.port"), 9000);
  EXPECT_TRUE(config.get<bool>("server.listen.reuse_addr"));
}

TEST(ConfigTest, ReadsEveryScalarTypeFromServerConfig) {
  Config config = LoadDataFile("server.json");

  EXPECT_EQ(config.get<std::string>("log.level"), "info");
  EXPECT_TRUE(config.get<bool>("log.console"));
  EXPECT_EQ(config.get<int32_t>("log.rotate_count"), 5);
  EXPECT_DOUBLE_EQ(config.get<double>("backend.gpu.memory_fraction"), 0.8);
  EXPECT_TRUE(config.node("backend.fallback").is_null());

  // 1 GiB 会越过 int32，必须按 int64 读出来而不是溢出。
  EXPECT_EQ(config.get<int64_t>("storage.quota_bytes"), 1073741824LL);
  EXPECT_EQ(config.get<uint32_t>("storage.quota_bytes"), 1073741824u);
}

TEST(ConfigTest, WidensIntToDouble) {
  Config config = LoadDataFile("server.json");
  // JSON 里是整数，按 double 读要放宽而不是报类型错。
  EXPECT_DOUBLE_EQ(config.get<double>("log.max_size_mb"), 64.0);
  // 反过来：double 不能悄悄截断成 int。
  EXPECT_THROW((void)config.get<int32_t>("backend.gpu.memory_fraction"),
               ConfigError);
}

TEST(ConfigTest, ReadsArrayOfObjects) {
  Config config = LoadDataFile("server.json");

  const ConfigValue& backends = config.node("backends");
  ASSERT_EQ(backends.size(), 2u);
  EXPECT_EQ(config.get<std::string>("backends.0.name"), "alpha");
  EXPECT_DOUBLE_EQ(config.get<double>("backends.0.weight"), 1.0);
  EXPECT_EQ(config.get<std::string>("backends.1.endpoint"), "10.0.0.2:8080");
}

// ==================== 顶层访问器 ====================
TEST(ConfigTest, InspectsObjectKeys) {
  Config config = LoadDataFile("server.json");

  EXPECT_TRUE(config.has("server.listen.port"));
  EXPECT_FALSE(config.has("server.listen.backpressure"));
  EXPECT_FALSE(config.has("server.listen.port.tls"));  // 标量不能再往下取
  EXPECT_EQ(config.node("").size(), 5u);

  const std::vector<std::string> keys = config.node("server.listen").keys();
  ASSERT_EQ(keys.size(), 4u);
  EXPECT_EQ(keys[0], "host");
  EXPECT_EQ(keys[3], "reuse_addr");

  EXPECT_TRUE(config.node("server.listen").has("port"));
  EXPECT_FALSE(config.node("server.name").has("port"));  // 非 object 返回 false
}

// ==================== 手写配置的宽容度 ====================
TEST(ConfigTest, AcceptsCommentsAndTrailingCommas) {
  Config config = LoadDataFile("comments.json");

  EXPECT_EQ(config.get<std::string>("server.listen.host"), "127.0.0.1");
  EXPECT_EQ(config.get<int32_t>("server.listen.port"), 8080);
  EXPECT_EQ(config.get<int32_t>("worker_threads"), 4);
  EXPECT_EQ(config.node("tags").size(), 3u);
}

// ==================== 数字 key 与数组下标 ====================
TEST(ConfigTest, NumericKeyOnObjectIsAKeyNotAnIndex) {
  Config config = LoadDataFile("numeric_keys.json");

  EXPECT_EQ(config.get<std::string>("http_status.200"), "OK");
  EXPECT_EQ(config.get<std::string>("http_status.500"),
            "Internal Server Error");
}

TEST(ConfigTest, DistinguishesArrayIndexFromObjectKey) {
  Config config = LoadDataFile("numeric_keys.json");

  EXPECT_EQ(config.get<int32_t>("ports.0"), 2121);
  EXPECT_EQ(config.get<int32_t>("ports.2"), 2123);
  EXPECT_EQ(config.get<std::string>("matrix.1.name"), "row1");
  EXPECT_EQ(config.get<int32_t>("matrix.1.cells.2"), 6);
}

// ==================== 错误处理 ====================
TEST(ConfigTest, ThrowsOnMissingComma) {
  Config config;
  // 少一个逗号是人手写配置最常见的错误，报错要能指出位置。
  EXPECT_THROW(config.load_file(DataPath("invalid_syntax.json")), ConfigError);
  EXPECT_FALSE(config.loaded());
}

TEST(ConfigTest, ThrowsWhenRootIsNotObject) {
  Config config;
  EXPECT_THROW(config.load_file(DataPath("invalid_root.json")), ConfigError);
}

TEST(ConfigTest, ThrowsOnDuplicateKeyAtTopLevel) {
  Config config;
  try {
    config.load_file(DataPath("duplicate_top_key.json"));
    FAIL() << "expected ConfigError";
  } catch (const ConfigError& error) {
    EXPECT_STREQ(error.what(), "port: duplicate key");
  }
  EXPECT_FALSE(config.loaded());
}

TEST(ConfigTest, ThrowsOnDuplicateKeyAndReportsFullPath) {
  Config config;
  // 嵌套层的重复 key，报错要能直接定位到出问题的那一项。
  try {
    config.load_file(DataPath("duplicate_key.json"));
    FAIL() << "expected ConfigError";
  } catch (const ConfigError& error) {
    EXPECT_STREQ(error.what(), "server.listen.port: duplicate key");
  }
}

TEST(ConfigTest, DistinctKeysInSameObjectAreAccepted) {
  Config config = LoadDataFile("server.json");
  // 确认重复 key 检测没有误伤正常对象。
  EXPECT_EQ(config.node("server.listen").size(), 4u);
  EXPECT_EQ(config.get<int32_t>("server.listen.port"), 9000);
}

TEST(ConfigTest, ThrowsOnMissingFile) {
  Config config;
  EXPECT_THROW(config.load_file(DataPath("does_not_exist.json")), ConfigError);
}

TEST(ConfigTest, ThrowsOnUnknownExtension) {
  Config config;
  try {
    config.load_file(DataPath("legacy.yaml"));
    FAIL() << "expected ConfigError";
  } catch (const ConfigError& error) {
    const std::string message = error.what();
    EXPECT_NE(message.find("cannot infer config format"), std::string::npos)
        << message;
    EXPECT_NE(message.find("legacy.yaml"), std::string::npos) << message;
  }
}

TEST(ConfigTest, ThrowsOnTypeMismatch) {
  Config config = LoadDataFile("server.json");

  // 路径存在但取错类型，错误信息要带上完整路径便于定位。
  try {
    (void)config.get<std::string>("server.listen.port");
    FAIL() << "expected ConfigError";
  } catch (const ConfigError& error) {
    const std::string message = error.what();
    EXPECT_NE(message.find("server.listen.port"), std::string::npos) << message;
    EXPECT_NE(message.find("expected string"), std::string::npos) << message;
    EXPECT_NE(message.find("got int"), std::string::npos) << message;
  }
}

TEST(ConfigTest, ThrowsOnArrayIndexOutOfRange) {
  Config config = LoadDataFile("numeric_keys.json");
  EXPECT_THROW((void)config.get<int32_t>("ports.5"), ConfigError);
}

TEST(ConfigTest, HandlesHugeArrayIndexAsOutOfRange) {
  Config config = LoadDataFile("numeric_keys.json");

  // 位数远超 size_t，不能让它以 std::out_of_range 的形式漏出 ConfigError 之外。
  const std::string path = "ports.999999999999999999999999";
  EXPECT_FALSE(config.has(path));
  EXPECT_THROW((void)config.get<int32_t>(path), ConfigError);
}

TEST(ConfigTest, ThrowsWhenAccessingBeforeLoad) {
  Config config;
  EXPECT_THROW((void)config.node("server"), ConfigError);
}

TEST(ConfigTest, ThrowsOnMissingPath) {
  Config config = LoadDataFile("minimal.json");
  EXPECT_FALSE(config.has("server.host"));
  EXPECT_THROW((void)config.get<int32_t>("server.host"), ConfigError);
}

// ==================== get_or ====================
TEST(ConfigTest, GetOrReturnsFallbackWhenMissing) {
  Config config = LoadDataFile("minimal.json");

  EXPECT_EQ(config.get_or<int32_t>("server.host", 0), 0);
  EXPECT_EQ(config.get_or<std::string>("server.host", "127.0.0.1"),
            "127.0.0.1");
  EXPECT_EQ(config.get_or<int32_t>("server.port", 9000), 2121);
}

// ==================== reload ====================
TEST(ConfigTest, ReloadReplacesPreviousTree) {
  Config config = LoadDataFile("server.json");
  EXPECT_TRUE(config.has("server.listen"));

  config.load_file(DataPath("minimal.json"));
  EXPECT_FALSE(config.has("server.listen"));
  EXPECT_TRUE(config.has("server.port"));
}

/**
 * Morphect - JSON Parser Tests
 */

#include <gtest/gtest.h>
#include "common/json_parser.hpp"

using namespace morphect;

TEST(JsonParserTest, ParseString) {
    auto json = JsonParser::parse(R"("hello")");
    EXPECT_TRUE(json.isString());
    EXPECT_EQ(json.asString(), "hello");
}

TEST(JsonParserTest, ParseNumber) {
    auto json = JsonParser::parse("42");
    EXPECT_TRUE(json.isNumber());
    EXPECT_EQ(json.asInt(), 42);

    json = JsonParser::parse("3.14");
    EXPECT_NEAR(json.asDouble(), 3.14, 0.001);
}

TEST(JsonParserTest, ParseNegativeNumber) {
    auto json = JsonParser::parse("-123");
    EXPECT_TRUE(json.isNumber());
    EXPECT_EQ(json.asInt(), -123);
}

TEST(JsonParserTest, ParseBool) {
    auto t = JsonParser::parse("true");
    auto f = JsonParser::parse("false");

    EXPECT_TRUE(t.isBool());
    EXPECT_TRUE(t.asBool());

    EXPECT_TRUE(f.isBool());
    EXPECT_FALSE(f.asBool());
}

TEST(JsonParserTest, ParseNull) {
    auto json = JsonParser::parse("null");
    EXPECT_TRUE(json.isNull());
}

TEST(JsonParserTest, ParseArray) {
    auto json = JsonParser::parse("[1, 2, 3]");

    EXPECT_TRUE(json.isArray());
    EXPECT_EQ(json.size(), 3);
    EXPECT_EQ(json[0].asInt(), 1);
    EXPECT_EQ(json[1].asInt(), 2);
    EXPECT_EQ(json[2].asInt(), 3);
}

TEST(JsonParserTest, ParseEmptyArray) {
    auto json = JsonParser::parse("[]");
    EXPECT_TRUE(json.isArray());
    EXPECT_EQ(json.size(), 0);
}

TEST(JsonParserTest, ParseObject) {
    auto json = JsonParser::parse(R"({"name": "test", "value": 42})");

    EXPECT_TRUE(json.isObject());
    EXPECT_TRUE(json.has("name"));
    EXPECT_TRUE(json.has("value"));
    EXPECT_EQ(json["name"].asString(), "test");
    EXPECT_EQ(json["value"].asInt(), 42);
}

TEST(JsonParserTest, ParseEmptyObject) {
    auto json = JsonParser::parse("{}");
    EXPECT_TRUE(json.isObject());
}

TEST(JsonParserTest, ParseNestedObject) {
    auto json = JsonParser::parse(R"({
        "settings": {
            "probability": 0.85,
            "enabled": true
        }
    })");

    EXPECT_TRUE(json.isObject());
    EXPECT_TRUE(json["settings"].isObject());
    EXPECT_NEAR(json["settings"]["probability"].asDouble(), 0.85, 0.001);
    EXPECT_TRUE(json["settings"]["enabled"].asBool());
}

TEST(JsonParserTest, DotNotationAccess) {
    auto json = JsonParser::parse(R"({
        "settings": {
            "probability": 0.85,
            "enabled": true
        }
    })");

    EXPECT_NEAR(json.get("settings.probability").asDouble(), 0.85, 0.001);
    EXPECT_TRUE(json.get("settings.enabled").asBool());
}

TEST(JsonParserTest, StringEscapes) {
    auto json = JsonParser::parse(R"("hello\nworld")");
    EXPECT_EQ(json.asString(), "hello\nworld");

    json = JsonParser::parse(R"("tab\there")");
    EXPECT_EQ(json.asString(), "tab\there");
}

TEST(JsonParserTest, ArrayOfObjects) {
    auto json = JsonParser::parse(R"([{"a": 1}, {"b": 2}])");

    EXPECT_TRUE(json.isArray());
    EXPECT_EQ(json.size(), 2);
    EXPECT_EQ(json[0]["a"].asInt(), 1);
    EXPECT_EQ(json[1]["b"].asInt(), 2);
}

TEST(JsonParserTest, DefaultValues) {
    auto json = JsonParser::parse(R"({})");

    EXPECT_EQ(json["nonexistent"].asInt(42), 42);
    EXPECT_EQ(json["nonexistent"].asString("default"), "default");
    EXPECT_EQ(json["nonexistent"].asBool(true), true);
}

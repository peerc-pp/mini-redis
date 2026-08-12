#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "protocol/resp_encoder.h"
#include "protocol/resp_parser.h"
#include "protocol/resp_value.h"

namespace {

using mini_redis::Buffer;
using mini_redis::ParseResult;
using mini_redis::ParseStatus;
using mini_redis::RespEncoder;
using mini_redis::RespParser;
using mini_redis::RespValue;

bool test_string_values() {
    const RespValue simple = RespValue::simple_string("OK");
    if (simple.type() != RespValue::Type::kSimpleString || simple.string_value() != "OK") {
        return false;
    }

    const RespValue error = RespValue::error("ERR bad");
    if (error.type() != RespValue::Type::kError || error.string_value() != "ERR bad") {
        return false;
    }

    const RespValue bulk = RespValue::bulk_string("foo");
    if (bulk.type() != RespValue::Type::kBulkString || bulk.string_value() != "foo") {
        return false;
    }

    const RespValue empty_bulk = RespValue::bulk_string("");
    return empty_bulk.type() == RespValue::Type::kBulkString && empty_bulk.string_value().empty();
}

bool test_integer_value() {
    const RespValue value = RespValue::integer(123);
    const RespValue negative = RespValue::integer(-42);

    return value.type() == RespValue::Type::kInteger && value.integer_value() == 123 &&
           negative.type() == RespValue::Type::kInteger && negative.integer_value() == -42;
}

bool test_null_bulk_string() {
    const RespValue value = RespValue::null_bulk_string();

    return value.type() == RespValue::Type::kNullBulkString;
}

bool test_array_value() {
    std::vector<RespValue> elements;
    elements.push_back(RespValue::bulk_string("GET"));
    elements.push_back(RespValue::bulk_string("foo"));

    const RespValue value = RespValue::array(std::move(elements));
    if (value.type() != RespValue::Type::kArray || value.array_value().size() != 2) {
        return false;
    }

    const RespValue& command = value.array_value()[0];
    const RespValue& key = value.array_value()[1];
    return command.type() == RespValue::Type::kBulkString && command.string_value() == "GET" &&
           key.type() == RespValue::Type::kBulkString && key.string_value() == "foo";
}

bool test_resp_encoder_scalars() {
    return RespEncoder::encode(RespValue::simple_string("OK")) == "+OK\r\n" &&
           RespEncoder::encode(RespValue::error("ERR bad")) == "-ERR bad\r\n" &&
           RespEncoder::encode(RespValue::integer(123)) == ":123\r\n" &&
           RespEncoder::encode(RespValue::integer(-42)) == ":-42\r\n" &&
           RespEncoder::encode(RespValue::bulk_string("foo")) == "$3\r\nfoo\r\n" &&
           RespEncoder::encode(RespValue::bulk_string("")) == "$0\r\n\r\n" &&
           RespEncoder::encode(RespValue::null_bulk_string()) == "$-1\r\n";
}

bool test_resp_encoder_array() {
    std::vector<RespValue> elements;
    elements.push_back(RespValue::bulk_string("GET"));
    elements.push_back(RespValue::bulk_string("foo"));

    if (RespEncoder::encode(RespValue::array(std::move(elements))) !=
        "*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n") {
        return false;
    }

    std::vector<RespValue> nested;
    nested.push_back(RespValue::array(std::vector<RespValue>{RespValue::integer(1)}));
    nested.push_back(RespValue::null_bulk_string());

    return RespEncoder::encode(RespValue::array(std::move(nested))) == "*2\r\n*1\r\n:1\r\n$-1\r\n";
}

bool buffer_equals(const Buffer& buffer, std::string_view expected) {
    return buffer.readable_bytes() == expected.size() &&
           std::string_view(buffer.peek(), buffer.readable_bytes()) == expected;
}

bool is_bulk_string(const ParseResult& result, std::string_view expected) {
    return result.status == ParseStatus::kComplete && result.value.has_value() &&
           result.value->type() == RespValue::Type::kBulkString &&
           result.value->string_value() == expected;
}

bool test_resp_parser_bulk_strings() {
    RespParser parser;

    Buffer regular;
    regular.append("$3\r\nfoo\r\n");
    const ParseResult regular_result = parser.parse(regular);
    if (!is_bulk_string(regular_result, "foo") || !regular.empty()) {
        return false;
    }

    Buffer empty;
    empty.append("$0\r\n\r\n");
    const ParseResult empty_result = parser.parse(empty);
    if (!is_bulk_string(empty_result, "") || !empty.empty()) {
        return false;
    }

    Buffer null_bulk;
    null_bulk.append("$-1\r\n");
    const ParseResult null_result = parser.parse(null_bulk);
    return null_result.status == ParseStatus::kComplete && null_result.value.has_value() &&
           null_result.value->type() == RespValue::Type::kNullBulkString && null_bulk.empty();
}

bool test_resp_parser_binary_bulk_string() {
    std::string encoded = "$5\r\n";
    encoded.append("a\r\n", 3);
    encoded.push_back('\0');
    encoded.push_back('b');
    encoded.append("\r\n");

    const std::string expected("a\r\n\0b", 5);
    Buffer input;
    input.append(encoded);

    RespParser parser;
    const ParseResult result = parser.parse(input);
    return is_bulk_string(result, expected) && input.empty();
}

bool test_resp_parser_arrays() {
    Buffer input;
    input.append("*2\r\n$3\r\nGET\r\n*2\r\n$3\r\nkey\r\n$-1\r\n");

    RespParser parser;
    const ParseResult result = parser.parse(input);
    if (result.status != ParseStatus::kComplete || !result.value.has_value() ||
        result.value->type() != RespValue::Type::kArray || !input.empty()) {
        return false;
    }

    const auto& outer = result.value->array_value();
    if (outer.size() != 2 || outer[0].type() != RespValue::Type::kBulkString ||
        outer[0].string_value() != "GET" || outer[1].type() != RespValue::Type::kArray) {
        return false;
    }

    const auto& nested = outer[1].array_value();
    return nested.size() == 2 && nested[0].type() == RespValue::Type::kBulkString &&
           nested[0].string_value() == "key" &&
           nested[1].type() == RespValue::Type::kNullBulkString;
}

bool test_resp_parser_consumes_one_value() {
    constexpr std::string_view first = "$3\r\none\r\n";
    constexpr std::string_view second = "$3\r\ntwo\r\n";

    Buffer input;
    input.append(std::string(first) + std::string(second));

    RespParser parser;
    const ParseResult first_result = parser.parse(input);
    if (!is_bulk_string(first_result, "one") || !buffer_equals(input, second)) {
        return false;
    }

    const ParseResult second_result = parser.parse(input);
    return is_bulk_string(second_result, "two") && input.empty();
}

bool test_resp_parser_need_more_keeps_buffer() {
    const std::vector<std::string> incomplete_values{
        "$3\r",
        "$3\r\nfo",
        "$3\r\nfoo\r",
        "*2\r\n$3\r\nGET\r\n$3\r\nke",
    };

    RespParser parser;
    for (const std::string& encoded : incomplete_values) {
        Buffer input;
        input.append(encoded);

        const ParseResult result = parser.parse(input);
        if (result.status != ParseStatus::kNeedMoreData || result.value.has_value() ||
            !result.error_message.empty() || !buffer_equals(input, encoded)) {
            return false;
        }
    }

    return true;
}

bool test_resp_parser_protocol_errors_keep_buffer() {
    const std::vector<std::string> invalid_values{
        "!bad\r\n", "$x\r\n", "$-2\r\n", "$67108865\r\n", "$3\r\nfooXX", "*-1\r\n", "*1025\r\n",
    };

    RespParser parser;
    for (const std::string& encoded : invalid_values) {
        Buffer input;
        input.append(encoded);

        const ParseResult result = parser.parse(input);
        if (result.status != ParseStatus::kError || result.value.has_value() ||
            result.error_message.empty() || !buffer_equals(input, encoded)) {
            return false;
        }
    }

    return true;
}

bool test_resp_parser_nesting_limit() {
    std::string encoded;
    for (std::size_t depth = 0; depth <= 32; ++depth) {
        encoded.append("*1\r\n");
    }
    encoded.append("$1\r\nx\r\n");

    Buffer input;
    input.append(encoded);

    RespParser parser;
    const ParseResult result = parser.parse(input);
    return result.status == ParseStatus::kError && !result.value.has_value() &&
           !result.error_message.empty() && buffer_equals(input, encoded);
}

bool run_test(const char* name, bool (*test)()) {
    if (test()) {
        return true;
    }

    std::cerr << name << " failed\n";
    return false;
}

}  // namespace

int main() {
    if (!run_test("string values", test_string_values) ||
        !run_test("integer value", test_integer_value) ||
        !run_test("null bulk string", test_null_bulk_string) ||
        !run_test("array value", test_array_value) ||
        !run_test("resp encoder scalars", test_resp_encoder_scalars) ||
        !run_test("resp encoder array", test_resp_encoder_array) ||
        !run_test("resp parser bulk strings", test_resp_parser_bulk_strings) ||
        !run_test("resp parser binary bulk string", test_resp_parser_binary_bulk_string) ||
        !run_test("resp parser arrays", test_resp_parser_arrays) ||
        !run_test("resp parser consumes one value", test_resp_parser_consumes_one_value) ||
        !run_test("resp parser need more keeps buffer", test_resp_parser_need_more_keeps_buffer) ||
        !run_test("resp parser protocol errors keep buffer",
                  test_resp_parser_protocol_errors_keep_buffer) ||
        !run_test("resp parser nesting limit", test_resp_parser_nesting_limit)) {
        return 1;
    }

    return 0;
}

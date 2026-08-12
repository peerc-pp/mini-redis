#include "server/command_registry.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace mini_redis {

CommandRegistry::CommandRegistry() {
    commands_.emplace(
        "PING",
        CommandSpec{0, 1, &CommandRegistry::execute_ping});

    commands_.emplace(
        "ECHO",
        CommandSpec{1, 1, &CommandRegistry::execute_echo});
}

RespValue CommandRegistry::execute(const RespValue& request) const {
    if (request.type() != RespValue::Type::kArray) {
        return RespValue::error(
            "ERR command request must be an array");
    }

    const RequestElements& elements = request.array_value();

    if (elements.empty()) {
        return RespValue::error(
            "ERR command array must not be empty");
    }

    for (const RespValue& element : elements) {
        if (element.type() != RespValue::Type::kBulkString) {
            return RespValue::error(
                "ERR command and arguments must be bulk strings");
        }
    }

    const std::string& original_name =
        elements.front().string_value();

    if (original_name.empty()) {
        return RespValue::error("ERR command name must not be empty");
    }

    const std::string command_name =
        normalize_command_name(original_name);

    const auto command = commands_.find(command_name);
    if (command == commands_.end()) {
        return RespValue::error(
            "ERR unknown command '" + original_name + "'");
    }

    const std::size_t argument_count = elements.size() - 1;
    const CommandSpec& spec = command->second;

    if (argument_count < spec.min_argument_count ||
        argument_count > spec.max_argument_count) {
        return RespValue::error(
            "ERR wrong number of arguments for '" +
            original_name + "' command");
    }

    return spec.handler(elements);
}

std::string CommandRegistry::normalize_command_name(std::string_view command_name) {
    std::string normalized(command_name);

    std::transform(
        normalized.begin(),
        normalized.end(),
        normalized.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::toupper(character));
        });

    return normalized;
}

RespValue CommandRegistry::execute_ping(
    const RequestElements& request_elements) {
    if (request_elements.size() == 1) {
        return RespValue::simple_string("PONG");
    }

    return RespValue::bulk_string(
        request_elements[1].string_value());
}

RespValue CommandRegistry::execute_echo(
    const RequestElements& request_elements) {
    return RespValue::bulk_string(
        request_elements[1].string_value());
}

}  // namespace mini_redis
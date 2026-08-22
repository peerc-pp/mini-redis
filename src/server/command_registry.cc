#include "server/command_registry.h"

#include "server/double_parser.h"
#include "server/integer_arithmetic.h"
#include "server/integer_parser.h"
#include "server/list_range.h"
#include "storage/database.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace mini_redis {

CommandRegistry::CommandRegistry(Database& database)
    : database_(database) {
    commands_.emplace(
        "PING",
        CommandSpec{0, 1, &CommandRegistry::execute_ping});

    commands_.emplace(
        "ECHO",
        CommandSpec{1, 1, &CommandRegistry::execute_echo});

    commands_.emplace(
        "SET",
        CommandSpec{2, 2, &CommandRegistry::execute_set});

    commands_.emplace(
        "GET",
        CommandSpec{1, 1, &CommandRegistry::execute_get});

    commands_.emplace(
        "INCR",
        CommandSpec{1, 1, &CommandRegistry::execute_incr});

    commands_.emplace(
        "DECR",
        CommandSpec{1, 1, &CommandRegistry::execute_decr});

    commands_.emplace(
        "LPUSH",
        CommandSpec{2, std::numeric_limits<std::size_t>::max(),
                    &CommandRegistry::execute_lpush});

    commands_.emplace(
        "RPUSH",
        CommandSpec{2, std::numeric_limits<std::size_t>::max(),
                    &CommandRegistry::execute_rpush});

    commands_.emplace(
        "LPOP",
        CommandSpec{1, 1, &CommandRegistry::execute_lpop});

    commands_.emplace(
        "RPOP",
        CommandSpec{1, 1, &CommandRegistry::execute_rpop});

    commands_.emplace(
        "LLEN",
        CommandSpec{1, 1, &CommandRegistry::execute_llen});

    commands_.emplace(
        "LRANGE",
        CommandSpec{3, 3, &CommandRegistry::execute_lrange});

    commands_.emplace(
        "HSET",
        CommandSpec{3, std::numeric_limits<std::size_t>::max(),
                    &CommandRegistry::execute_hset});

    commands_.emplace(
        "HGET",
        CommandSpec{2, 2, &CommandRegistry::execute_hget});

    commands_.emplace(
        "HDEL",
        CommandSpec{2, std::numeric_limits<std::size_t>::max(),
                    &CommandRegistry::execute_hdel});

    commands_.emplace(
        "HEXISTS",
        CommandSpec{2, 2, &CommandRegistry::execute_hexists});

    commands_.emplace(
        "HLEN",
        CommandSpec{1, 1, &CommandRegistry::execute_hlen});

    commands_.emplace(
        "HGETALL",
        CommandSpec{1, 1, &CommandRegistry::execute_hgetall});

    commands_.emplace(
        "ZADD",
        CommandSpec{3, 3, &CommandRegistry::execute_zadd});

    commands_.emplace(
        "ZREM",
        CommandSpec{2, std::numeric_limits<std::size_t>::max(),
                    &CommandRegistry::execute_zrem});

    commands_.emplace(
        "ZSCORE",
        CommandSpec{2, 2, &CommandRegistry::execute_zscore});

    commands_.emplace(
        "ZRANK",
        CommandSpec{2, 2, &CommandRegistry::execute_zrank});

    commands_.emplace(
        "ZRANGE",
        CommandSpec{3, 3, &CommandRegistry::execute_zrange});

    commands_.emplace(
        "DEL",
        CommandSpec{1, std::numeric_limits<std::size_t>::max(),
                    &CommandRegistry::execute_del});

    commands_.emplace(
        "EXISTS",
        CommandSpec{1, std::numeric_limits<std::size_t>::max(),
                    &CommandRegistry::execute_exists});
}

RespValue CommandRegistry::execute(const RespValue& request) {
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

    return (this->*spec.handler)(elements);
}

std::string CommandRegistry::normalize_command_name(
    std::string_view command_name) {
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

RespValue CommandRegistry::execute_set(
    const RequestElements& request_elements) {
    database_.set(
        request_elements[1].string_value(),
        Value::string(request_elements[2].string_value()));
    return RespValue::simple_string("OK");
}

RespValue CommandRegistry::execute_get(
    const RequestElements& request_elements) {
    const Value* value =
        database_.find(request_elements[1].string_value());
    if (value == nullptr) {
        return RespValue::null_bulk_string();
    }

    const Value::String* string_value = value->as_string();
    if (string_value == nullptr) {
        return RespValue::error(
            "WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    return RespValue::bulk_string(*string_value);
}

RespValue CommandRegistry::execute_incr(
    const RequestElements& request_elements) {
    return execute_integer_change(request_elements, 1);
}

RespValue CommandRegistry::execute_decr(
    const RequestElements& request_elements) {
    return execute_integer_change(request_elements, -1);
}

RespValue CommandRegistry::execute_integer_change(
    const RequestElements& request_elements,
    std::int64_t delta) {
    const std::string& key = request_elements[1].string_value();
    Value* value = database_.find(key);

    if (value == nullptr) {
        database_.set(key, Value::string(std::to_string(delta)));
        return RespValue::integer(delta);
    }

    Value::String* string_value = value->as_string();
    if (string_value == nullptr) {
        return RespValue::error(
            "WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    const auto current = parse_integer(*string_value);
    if (!current.has_value()) {
        return RespValue::error(
            "ERR value is not an integer or out of range");
    }

    const auto updated = checked_add(*current, delta);
    if (!updated.has_value()) {
        return RespValue::error(
            "ERR value is not an integer or out of range");
    }

    *string_value = std::to_string(*updated);
    return RespValue::integer(*updated);
}

RespValue CommandRegistry::execute_lpush(
    const RequestElements& request_elements) {
    return execute_list_push(request_elements, ListEnd::kLeft);
}

RespValue CommandRegistry::execute_rpush(
    const RequestElements& request_elements) {
    return execute_list_push(request_elements, ListEnd::kRight);
}

RespValue CommandRegistry::execute_list_push(
    const RequestElements& request_elements,
    ListEnd end) {
    const std::string& key = request_elements[1].string_value();
    Value* value = database_.find(key);

    if (value == nullptr) {
        Value::List list;
        for (std::size_t index = 2;
             index < request_elements.size(); ++index) {
            const std::string& element =
                request_elements[index].string_value();
            if (end == ListEnd::kLeft) {
                list.push_front(element);
            } else {
                list.push_back(element);
            }
        }

        const std::int64_t length =
            static_cast<std::int64_t>(list.size());
        database_.set(key, Value::list(std::move(list)));
        return RespValue::integer(length);
    }

    Value::List* list = value->as_list();
    if (list == nullptr) {
        return RespValue::error(
            "WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    for (std::size_t index = 2;
         index < request_elements.size(); ++index) {
        const std::string& element =
            request_elements[index].string_value();
        if (end == ListEnd::kLeft) {
            list->push_front(element);
        } else {
            list->push_back(element);
        }
    }

    return RespValue::integer(
        static_cast<std::int64_t>(list->size()));
}

RespValue CommandRegistry::execute_lpop(
    const RequestElements& request_elements) {
    return execute_list_pop(request_elements, ListEnd::kLeft);
}

RespValue CommandRegistry::execute_rpop(
    const RequestElements& request_elements) {
    return execute_list_pop(request_elements, ListEnd::kRight);
}

RespValue CommandRegistry::execute_list_pop(
    const RequestElements& request_elements,
    ListEnd end) {
    const std::string& key = request_elements[1].string_value();
    Value* value = database_.find(key);
    if (value == nullptr) {
        return RespValue::null_bulk_string();
    }

    Value::List* list = value->as_list();
    if (list == nullptr) {
        return RespValue::error(
            "WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    if (list->empty()) {
        (void)database_.erase(key);
        return RespValue::null_bulk_string();
    }

    std::string popped_value;
    if (end == ListEnd::kLeft) {
        popped_value = std::move(list->front());
        list->pop_front();
    } else {
        popped_value = std::move(list->back());
        list->pop_back();
    }

    if (list->empty()) {
        (void)database_.erase(key);
    }

    return RespValue::bulk_string(std::move(popped_value));
}

RespValue CommandRegistry::execute_llen(
    const RequestElements& request_elements) {
    const Value* value =
        database_.find(request_elements[1].string_value());
    if (value == nullptr) {
        return RespValue::integer(0);
    }

    const Value::List* list = value->as_list();
    if (list == nullptr) {
        return RespValue::error(
            "WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    return RespValue::integer(
        static_cast<std::int64_t>(list->size()));
}

RespValue CommandRegistry::execute_lrange(
    const RequestElements& request_elements) {
    const auto start =
        parse_integer(request_elements[2].string_value());
    const auto stop =
        parse_integer(request_elements[3].string_value());
    if (!start.has_value() || !stop.has_value()) {
        return RespValue::error(
            "ERR value is not an integer or out of range");
    }

    const Value* value =
        database_.find(request_elements[1].string_value());
    if (value == nullptr) {
        return RespValue::array({});
    }

    const Value::List* list = value->as_list();
    if (list == nullptr) {
        return RespValue::error(
            "WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    const auto range =
        normalize_list_range(list->size(), *start, *stop);
    if (!range.has_value()) {
        return RespValue::array({});
    }

    std::vector<RespValue> elements;
    elements.reserve(range->end - range->begin);
    for (std::size_t index = range->begin;
         index < range->end; ++index) {
        elements.push_back(RespValue::bulk_string((*list)[index]));
    }

    return RespValue::array(std::move(elements));
}

RespValue CommandRegistry::execute_hset(
    const RequestElements& request_elements) {
    if ((request_elements.size() - 2) % 2 != 0) {
        return RespValue::error(
            "ERR wrong number of arguments for '" +
            request_elements.front().string_value() + "' command");
    }

    const std::string& key = request_elements[1].string_value();
    Value* value = database_.find(key);

    if (value == nullptr) {
        Value::Hash hash;
        std::int64_t added_count = 0;
        for (std::size_t index = 2;
             index < request_elements.size(); index += 2) {
            const auto [position, inserted] = hash.insert_or_assign(
                request_elements[index].string_value(),
                request_elements[index + 1].string_value());
            (void)position;
            if (inserted) {
                ++added_count;
            }
        }

        database_.set(key, Value::hash(std::move(hash)));
        return RespValue::integer(added_count);
    }

    Value::Hash* hash = value->as_hash();
    if (hash == nullptr) {
        return RespValue::error(
            "WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    std::int64_t added_count = 0;
    for (std::size_t index = 2;
         index < request_elements.size(); index += 2) {
        const auto [position, inserted] = hash->insert_or_assign(
            request_elements[index].string_value(),
            request_elements[index + 1].string_value());
        (void)position;
        if (inserted) {
            ++added_count;
        }
    }

    return RespValue::integer(added_count);
}

RespValue CommandRegistry::execute_hget(
    const RequestElements& request_elements) {
    const Value* value =
        database_.find(request_elements[1].string_value());
    if (value == nullptr) {
        return RespValue::null_bulk_string();
    }

    const Value::Hash* hash = value->as_hash();
    if (hash == nullptr) {
        return RespValue::error(
            "WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    const auto field =
        hash->find(request_elements[2].string_value());
    if (field == hash->end()) {
        return RespValue::null_bulk_string();
    }

    return RespValue::bulk_string(field->second);
}

RespValue CommandRegistry::execute_hdel(
    const RequestElements& request_elements) {
    const std::string& key = request_elements[1].string_value();
    Value* value = database_.find(key);
    if (value == nullptr) {
        return RespValue::integer(0);
    }

    Value::Hash* hash = value->as_hash();
    if (hash == nullptr) {
        return RespValue::error(
            "WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    std::int64_t deleted_count = 0;
    for (std::size_t index = 2;
         index < request_elements.size(); ++index) {
        if (hash->erase(request_elements[index].string_value()) != 0) {
            ++deleted_count;
        }
    }

    if (hash->empty()) {
        (void)database_.erase(key);
    }

    return RespValue::integer(deleted_count);
}

RespValue CommandRegistry::execute_hexists(
    const RequestElements& request_elements) {
    const Value* value =
        database_.find(request_elements[1].string_value());
    if (value == nullptr) {
        return RespValue::integer(0);
    }

    const Value::Hash* hash = value->as_hash();
    if (hash == nullptr) {
        return RespValue::error(
            "WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    const bool exists =
        hash->find(request_elements[2].string_value()) != hash->end();
    return RespValue::integer(exists ? 1 : 0);
}

RespValue CommandRegistry::execute_hlen(
    const RequestElements& request_elements) {
    const Value* value =
        database_.find(request_elements[1].string_value());
    if (value == nullptr) {
        return RespValue::integer(0);
    }

    const Value::Hash* hash = value->as_hash();
    if (hash == nullptr) {
        return RespValue::error(
            "WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    return RespValue::integer(
        static_cast<std::int64_t>(hash->size()));
}

RespValue CommandRegistry::execute_hgetall(
    const RequestElements& request_elements) {
    const Value* value =
        database_.find(request_elements[1].string_value());
    if (value == nullptr) {
        return RespValue::array({});
    }

    const Value::Hash* hash = value->as_hash();
    if (hash == nullptr) {
        return RespValue::error(
            "WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    std::vector<RespValue> elements;
    elements.reserve(hash->size() * 2);
    for (const auto& [field, field_value] : *hash) {
        elements.push_back(RespValue::bulk_string(field));
        elements.push_back(RespValue::bulk_string(field_value));
    }

    return RespValue::array(std::move(elements));
}

RespValue CommandRegistry::execute_zadd(
    const RequestElements& request_elements) {
    const auto score =
        parse_double(request_elements[2].string_value());
    if (!score.has_value()) {
        return RespValue::error(
            "ERR value is not a valid float");
    }

    const std::string& key =
        request_elements[1].string_value();
    const std::string& member =
        request_elements[3].string_value();
    Value* value = database_.find(key);

    if (value == nullptr) {
        Value new_value = Value::zset();
        Value::SortedSet* zset = new_value.as_zset();
        const auto result = zset->add(*score, member);
        if (result == Value::SortedSet::AddResult::kInvalidScore) {
            return RespValue::error(
                "ERR value is not a valid float");
        }
        database_.set(key, std::move(new_value));
        return RespValue::integer(1);
    }

    Value::SortedSet* zset = value->as_zset();
    if (zset == nullptr) {
        return RespValue::error(
            "WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    const auto result = zset->add(*score, member);
    if (result == Value::SortedSet::AddResult::kInvalidScore) {
        return RespValue::error(
            "ERR value is not a valid float");
    }
    return RespValue::integer(
        result == Value::SortedSet::AddResult::kAdded ? 1 : 0);
}

RespValue CommandRegistry::execute_zrem(
    const RequestElements& request_elements) {
    const std::string& key =
        request_elements[1].string_value();
    Value* value = database_.find(key);
    if (value == nullptr) {
        return RespValue::integer(0);
    }

    Value::SortedSet* zset = value->as_zset();
    if (zset == nullptr) {
        return RespValue::error(
            "WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    std::int64_t removed = 0;
    for (std::size_t index = 2;
         index < request_elements.size(); ++index) {
        if (zset->remove(
                request_elements[index].string_value())) {
            ++removed;
        }
    }

    if (zset->empty()) {
        static_cast<void>(database_.erase(key));
    }
    return RespValue::integer(removed);
}

RespValue CommandRegistry::execute_zscore(
    const RequestElements& request_elements) {
    const Value* value =
        database_.find(request_elements[1].string_value());
    if (value == nullptr) {
        return RespValue::null_bulk_string();
    }

    const Value::SortedSet* zset = value->as_zset();
    if (zset == nullptr) {
        return RespValue::error(
            "WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    const auto score =
        zset->score_of(request_elements[2].string_value());
    if (!score.has_value()) {
        return RespValue::null_bulk_string();
    }
    return RespValue::bulk_string(format_double(*score));
}

RespValue CommandRegistry::execute_zrank(
    const RequestElements& request_elements) {
    const Value* value =
        database_.find(request_elements[1].string_value());
    if (value == nullptr) {
        return RespValue::null_bulk_string();
    }

    const Value::SortedSet* zset = value->as_zset();
    if (zset == nullptr) {
        return RespValue::error(
            "WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    const auto rank =
        zset->rank_of(request_elements[2].string_value());
    if (!rank.has_value()) {
        return RespValue::null_bulk_string();
    }
    return RespValue::integer(
        static_cast<std::int64_t>(*rank));
}

RespValue CommandRegistry::execute_zrange(
    const RequestElements& request_elements) {
    const auto start =
        parse_integer(request_elements[2].string_value());
    const auto stop =
        parse_integer(request_elements[3].string_value());
    if (!start.has_value() || !stop.has_value()) {
        return RespValue::error(
            "ERR value is not an integer or out of range");
    }

    const Value* value =
        database_.find(request_elements[1].string_value());
    if (value == nullptr) {
        return RespValue::array({});
    }

    const Value::SortedSet* zset = value->as_zset();
    if (zset == nullptr) {
        return RespValue::error(
            "WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    const auto range =
        normalize_list_range(zset->size(), *start, *stop);
    if (!range.has_value()) {
        return RespValue::array({});
    }

    const auto entries =
        zset->range_by_rank(range->begin, range->end - 1);
    std::vector<RespValue> members;
    members.reserve(entries.size());
    for (const auto& entry : entries) {
        members.push_back(
            RespValue::bulk_string(entry.member));
    }
    return RespValue::array(std::move(members));
}

RespValue CommandRegistry::execute_del(
    const RequestElements& request_elements) {
    std::int64_t deleted_count = 0;
    for (std::size_t index = 1;
         index < request_elements.size(); ++index) {
        if (database_.erase(request_elements[index].string_value())) {
            ++deleted_count;
        }
    }

    return RespValue::integer(deleted_count);
}

RespValue CommandRegistry::execute_exists(
    const RequestElements& request_elements) {
    std::int64_t exists_count = 0;
    for (std::size_t index = 1;
         index < request_elements.size(); ++index) {
        if (database_.exists(request_elements[index].string_value())) {
            ++exists_count;
        }
    }

    return RespValue::integer(exists_count);
}

}  // namespace mini_redis

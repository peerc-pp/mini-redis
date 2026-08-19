#include "server/session.h"

#include "net/tcp_connection.h"
#include "protocol/resp_encoder.h"

#include <string>

namespace mini_redis {

Session::Session(CommandRegistry& commands)
    : commands_(commands) {}

void Session::on_message(
    TcpConnection& connection,
    Buffer& input) {
    while (!input.empty()) {
        ParseResult result = parser_.parse(input);

        if (result.status == ParseStatus::kNeedMoreData) {
            return;
        }

        if (result.status == ParseStatus::kError) {
            const RespValue error = RespValue::error(
                "ERR Protocol error: " + result.error_message);

            const std::string encoded =
                RespEncoder::encode(error);

            // Parser 遇到错误时不会消费输入，因此这里必须清空。
            // 否则下一次回调还会解析同一段错误数据。
            input.retrieve_all();

            if (!connection.send(encoded)) {
                connection.force_close();
                return;
            }

            // 等待错误响应发送完毕，然后关闭写方向。
            connection.shutdown();
            return;
        }

        if (!result.value.has_value()) {
            connection.force_close();
            return;
        }

        const RespValue response =
            commands_.execute(*result.value);

        const std::string encoded =
            RespEncoder::encode(response);

        if (!connection.send(encoded)) {
            connection.force_close();
            return;
        }
    }
}

}  // namespace mini_redis
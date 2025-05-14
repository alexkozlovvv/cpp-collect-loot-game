#pragma once
// boost.beast будет использовать std::string_view вместо boost::string_view
#define BOOST_BEAST_USE_STD_STRING_VIEW
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <boost/log/core.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/format.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/json.hpp>
#include <chrono>
#include <variant>

namespace server_log {

    namespace logging = boost::log;
    namespace keywords = boost::log::keywords;
    namespace json = boost::json;
    namespace net = boost::asio;
    namespace beast = boost::beast;
    namespace http = beast::http;
    using namespace std::literals;
    using StringResponse = http::response<http::string_body>;
    using FileResponse = http::response<http::file_body>;
    using Response = std::variant<StringResponse, FileResponse>;
    using HttpRequest = http::request<http::string_body>;

    BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)
    BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)

    void MyFormatter(logging::record_view const& rec, logging::formatting_ostream& strm);
    unsigned GetResponseResult(const Response& response);
    std::string_view GetResponseContentType(const Response& response);

    /* Декоратор */
    template<class SomeRequestHandler>
    class LoggingRequestHandler {
    public:
        explicit LoggingRequestHandler(std::shared_ptr<SomeRequestHandler> decorated_ptr) : decorated_ptr_(decorated_ptr) {}

        static void InitLogging() {
            logging::add_common_attributes();
            logging::add_console_log( 
                std::cout,
                keywords::format = &MyFormatter,
                keywords::auto_flush = true
            ); 
        }

        static void LogOnError(beast::error_code ec, std::string_view where_str) {
            json::value error_data{{"code"s, ec.value()}, {"text"s, ec.message()}, {"where"s, where_str}};
            BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, error_data)
                    << "error"sv;
        }

        void LogStartServer(const net::ip::address& address, const net::ip::port_type& port) {
            json::value start_data{{"port"s, port}, {"address"s, address.to_string()}};
            BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, start_data)
                    << "server started"sv;
        }
        void LogExitServer() {
            json::value exit_data{{"code"s, 0}};
            BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, exit_data)
                    << "server exited"sv;
        }
    private:

        template <typename Body, typename Allocator>
        static void LogRequest(const http::request<Body, http::basic_fields<Allocator>>& req, const std::string& client_ip) {
            if(req.method() == http::verb::get ) {
                json::value request_data{{"ip"s, client_ip}, {"URI"s, static_cast<std::string>(req.target())}, {"method"s, "GET"s}};
                BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, request_data)
                    << "request received"sv;
            }

        }
    public:
        static void LogResponse(const Response& r, std::string client_ip, int64_t duration_response_time){
            auto content_type = GetResponseContentType(r);
            json::value content_type_object;
            if(!content_type.empty()) {
                content_type_object.emplace_string() = std::string(content_type);
            }
            json::value response_data{{"ip"s, client_ip}, {"response_time"s, duration_response_time}, {"code"s, GetResponseResult(r)}, 
                                      {"content_type"s, content_type_object}};
                BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, response_data)
                    << "response sent"sv;
        }

        template <typename Body, typename Allocator, typename Send>
        void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, const std::string& client_ip, Send&& send) {
            LogRequest(req, client_ip);
            (*decorated_ptr_)(std::move(req), std::move(send));
        }

    private:
        //SomeRequestHandler& decorated_;
        std::shared_ptr<SomeRequestHandler> decorated_ptr_;
    };
}
#pragma once
// boost.beast будет использовать std::string_view вместо boost::string_view
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include  <boost/json.hpp>
#include  <filesystem>
#include "model.h"
#include "app.h"
#include  <variant>
#include <iostream>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
using namespace std::literals;
namespace fs = std::filesystem;
namespace sys = boost::system;
namespace net = boost::asio;

// Запрос, тело которого представлено в виде строки
using StringRequest = http::request<http::string_body>;
// Ответ, тело которого представлено в виде строки
using StringResponse = http::response<http::string_body>;
using FileResponse = http::response<http::file_body>;
using Response = std::variant<StringResponse, FileResponse>;

struct ContentType {
ContentType() = delete;
    constexpr static std::string_view TEXT_HTML = "text/html"sv;
    constexpr static std::string_view TEXT_CSS = "text/css"sv;
    constexpr static std::string_view TEXT_JS = "text/javascript"sv;
    constexpr static std::string_view XML = "application/xml"sv;
    constexpr static std::string_view PNG = "image/png"sv;
    constexpr static std::string_view JPEG = "image/jpeg"sv;
    constexpr static std::string_view GIF = "image/gif"sv;
    constexpr static std::string_view BMP = "image/bmp"sv;
    constexpr static std::string_view ICON = "image/vnd.microsoft.icon"sv;
    constexpr static std::string_view TIFF = "image/tiff"sv;
    constexpr static std::string_view SVG_XML = "image/svg+xml"sv;
    constexpr static std::string_view MPEG = "audio/mpeg"sv;
    constexpr static std::string_view TEXT_PLAIN = "text/plain"sv;
    constexpr static std::string_view JSON = "application/json"sv;
    constexpr static std::string_view UNKNOWN = "application/octet-stream"sv;

    // При необходимости внутрь ContentType можно добавить и другие типы контента
};

class ApiHandler {
public:
    ApiHandler(app::Application& app) : 
               app_(app) {}

    Response HandleApiRequest(const StringRequest& request) const;

private:
    app::Application& app_;

    Response Join(const StringRequest& req) const;
    Response ListPlayers(const StringRequest& req) const;
    Response GetGameState(const StringRequest& req) const;
    Response MovePlayers(const StringRequest& req) const;
    Response TickTime(const StringRequest& req) const;
    Response GetMapsInfo(const StringRequest& req) const;
    Response ListRetirePlayers(const StringRequest& req) const;
};

class RequestHandler : public std::enable_shared_from_this<RequestHandler> {
public:
    using Strand = net::strand<net::io_context::executor_type>;

    explicit RequestHandler(Strand api_strand, app::Application& app,
            const std::filesystem::path& root_dir_path)
        : api_strand_{api_strand}, api_handler_(app) {

        root_dir_path_ = fs::weakly_canonical(root_dir_path);
        root_dir_path_ = fs::absolute(root_dir_path_);
        std::string temp_str(root_dir_path_);
        if(temp_str.back() == '/') {
            temp_str.pop_back();
            root_dir_path_ = temp_str;
        }
        if(!fs::exists(root_dir_path_)) {
            throw std::runtime_error("Каталог по указанному пути не найден: " + root_dir_path_.string());
        }
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string trg =  static_cast<std::string>(req.target());
        try {
            if (trg.find("/api/") != std::string::npos) {
                auto handle = [self = shared_from_this(), this, send,
                                req = std::forward<decltype(req)>(req)] {
                    try {
                        // Этот assert не выстрелит, так как лямбда-функция будет выполняться внутри strand
                        assert(self->api_strand_.running_in_this_thread());
                        auto res = api_handler_.HandleApiRequest(req);
                        return send(res);
                    } catch (...) {
                        //send(self->ReportServerError());
                    }
                };
                return net::dispatch(api_strand_, handle);
            }
            auto res = HandleFileRequest(req);
            send(std::forward<decltype(res)>(res));
        } catch (...) {
            //send(ReportServerError(version, keep_alive));
        }
    }

private:
    Response HandleFileRequest(const StringRequest& req) const;
    Response MakeValidFileResponse(http::status status, unsigned http_version, 
                                    const fs::path path) const;
    Response MakeInValidFileResponse(http::status status, unsigned http_version,
                                    std::string_view content_type) const;

    Strand api_strand_;
    ApiHandler api_handler_;
    fs::path root_dir_path_;
};

}  // namespace http_handler

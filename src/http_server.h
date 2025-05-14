#pragma once
#include "sdk.h"
// boost.beast будет использовать std::string_view вместо boost::string_view
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <iostream>
#include  <variant>
#include "logging.h"
#include "request_handler.h"

namespace http_server {

namespace net = boost::asio;
using tcp = net::ip::tcp;
using namespace std::literals;
using namespace server_log;
namespace beast = boost::beast;
namespace http = beast::http;
namespace sys = boost::system;
using StringResponse = http::response<http::string_body>;
using FileResponse = http::response<http::file_body>;
using Response = std::variant<StringResponse, FileResponse>;

void ReportError(beast::error_code ec, std::string_view what);

class SessionBase {
protected:
    using HttpRequest = http::request<http::string_body>;
    explicit SessionBase(tcp::socket&& socket);

    template <typename Body, typename Fields>
    void Write(http::response<Body, Fields>&& response) {
        // Запись выполняется асинхронно, поэтому response перемещаем в область кучи
        auto safe_response = std::make_shared<http::response<Body, Fields>>(std::move(response));
        
        auto self = GetSharedThis();
        http::async_write(stream_, *safe_response,
                          [safe_response, self](beast::error_code ec, std::size_t bytes_written) {
                              self->OnWrite(safe_response->need_eof(), ec, bytes_written);
                          });
    }

    ~SessionBase() = default;
public:
    SessionBase(const SessionBase&) = delete;
    SessionBase& operator=(const SessionBase&) = delete;
    
    void Run();

private:
    void Read();
    void OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read);
    void OnWrite(bool close, beast::error_code ec, [[maybe_unused]] std::size_t bytes_written);
    void Close();

    // tcp_stream содержит внутри себя сокет и добавляет поддержку таймаутов
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    HttpRequest request_;

    // Обработку запроса делегируем подклассу
    virtual void HandleRequest(HttpRequest&& request, std::string client_ip) = 0;

    virtual std::shared_ptr<SessionBase> GetSharedThis() = 0;
};

template <typename RequestHandler>
class Session : public SessionBase, public std::enable_shared_from_this<Session<RequestHandler>> {
public:
    template <typename Handler>
    Session(tcp::socket&& socket, Handler&& request_handler)
        : SessionBase(std::move(socket))
        , request_handler_(std::forward<Handler>(request_handler)) {
    }

private:
    void HandleRequest(HttpRequest&& request, std::string client_ip) override {
        // Захватываем умный указатель на текущий объект Session в лямбде,
        // чтобы продлить время жизни сессии до вызова лямбды.
        // Используется generic-лямбда функция, способная принять response произвольного типа

        auto start_response_time = std::chrono::high_resolution_clock::now();
        request_handler_(std::move(request), client_ip, [self = this->shared_from_this(), this, client_ip, start_response_time](auto&& response) {  
            auto end_response_time = std::chrono::high_resolution_clock::now();
            auto duration_response_time = std::chrono::duration_cast<std::chrono::microseconds>(end_response_time - start_response_time).count();
            if(std::holds_alternative<StringResponse>(response)){
                request_handler_.LogResponse(response, client_ip, duration_response_time);
                self->Write(std::move(std::get<StringResponse>(response)));
            } else if(std::holds_alternative<FileResponse>(response)) {
                request_handler_.LogResponse(response, client_ip, duration_response_time);
                self->Write(std::move(std::get<FileResponse>(response)));
            }
        });
    }

    std::shared_ptr<SessionBase> GetSharedThis() override {
        return this->shared_from_this();
    }
    
    RequestHandler request_handler_;
};

template <typename RequestHandler>
class Listener : public std::enable_shared_from_this<Listener<RequestHandler>> {
public:    
    template <typename Handler>
    Listener(net::io_context& ioc, const tcp::endpoint& endpoint, Handler&& request_handler)
        : ioc_(ioc)
        // Обработчики асинхронных операций acceptor_ будут вызываться в своём strand
        , acceptor_(net::make_strand(ioc))
        , request_handler_(std::forward<Handler>(request_handler)) {
        // Открываем acceptor, используя протокол (IPv4 или IPv6), указанный в endpoint
        acceptor_.open(endpoint.protocol());

        // После закрытия TCP-соединения сокет некоторое время может считаться занятым,
        // чтобы компьютеры могли обменяться завершающими пакетами данных.
        // Однако это может помешать повторно открыть сокет в полузакрытом состоянии.
        // Флаг reuse_address разрешает открыть сокет, когда он "наполовину закрыт"
        acceptor_.set_option(net::socket_base::reuse_address(true));
        // Привязываем acceptor к адресу и порту endpoint
        acceptor_.bind(endpoint);
        // Переводим acceptor в состояние, в котором он способен принимать новые соединения
        // Благодаря этому новые подключения будут помещаться в очередь ожидающих соединений
        acceptor_.listen(net::socket_base::max_listen_connections);
    }
    void Run() {
        DoAccept();
    }

private:
    void AsyncRunSession(tcp::socket&& socket) {
        std::make_shared<Session<RequestHandler>>(std::move(socket), request_handler_)->Run();
    }

    void DoAccept() {
        acceptor_.async_accept(
            // Передаём последовательный исполнитель, в котором будут вызываться обработчики
            // асинхронных операций сокета
            net::make_strand(ioc_),
            // С помощью bind_front_handler создаём обработчик, привязанный к методу OnAccept
            // текущего объекта.
            // Так как Listener — шаблонный класс, нужно подсказать компилятору, что
            // shared_from_this — метод класса, а не свободная функция.
            // Для этого вызываем его, используя this
            // Этот вызов bind_front_handler аналогичен
            // namespace ph = std::placeholders;
            // std::bind(&Listener::OnAccept, this->shared_from_this(), ph::_1, ph::_2)
            beast::bind_front_handler(&Listener::OnAccept, this->shared_from_this()));
    }

    // Метод socket::async_accept создаст сокет и передаст его передан в OnAccept
    void OnAccept(sys::error_code ec, tcp::socket socket) {
        using namespace std::literals;

        if (ec) {
            return ReportError(ec, "accept"sv);
        }

        // Асинхронно обрабатываем сессию
        AsyncRunSession(std::move(socket));

        // Принимаем новое соединение
        DoAccept();
    }
 
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    RequestHandler request_handler_;
};

template <typename RequestHandler>
void ServeHttp(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler&& handler) {
    // При помощи decay_t исключим ссылки из типа RequestHandler,
    // чтобы Listener хранил RequestHandler по значению
    using MyListener = Listener<std::decay_t<RequestHandler>>;

    std::make_shared<MyListener>(ioc, endpoint, std::forward<RequestHandler>(handler))->Run();
}

}  // namespace http_server

#include "sdk.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <optional>
#include "http_server.h"
#include "json_loader.h"
#include "request_handler.h"
#include "app.h"
#include "extra_data.h"
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include "model_serialization.h"
#include "app_serialization.h"
#include "infrastructure.h"
#include <exception>
#include "connection_pool.h"


using namespace std::literals;
using namespace server_log;
namespace net = boost::asio;
namespace sys = boost::system;
namespace http = boost::beast::http;

namespace {

//constexpr const char DB_URL_ENV_NAME[]{"GAME_DB_URL"};

// app::AppConfig GetConfigFromEnv() {
//     app::AppConfig config;
//     if (const auto* url = std::getenv(DB_URL_ENV_NAME)) {
//         config.db_url = url;
//     } else {
//         throw std::runtime_error(DB_URL_ENV_NAME + " environment variable not found"s);
//     }
//     return config;
// }

// Запускает функцию fn на n потоках, включая текущий
template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    // Запускаем n-1 рабочих потоков, выполняющих функцию fn
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

struct Args {
    std::string tick_period;
    std::string config_file;
    std::string root_dir;
    std::string save_file;
    std::string save_period;
    bool is_randomize = false;
};

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;

    po::options_description desc{"Allowed options"s};

    Args args;
    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value(&args.tick_period)->value_name("milliseconds"s), "set tick period")
        ("config-file,c", po::value(&args.config_file)->value_name("file"s), "set config file path")
        ("www-root,w", po::value(&args.root_dir)->value_name("dir"s), "set static files root")
        ("state-file,s", po::value(&args.save_file)->value_name("state file"s), "set save file")
        ("save-state-period,p", po::value(&args.save_period)->value_name("milliseconds"s), "set save state period")
        ("randomize-spawn-points", "spawn dogs at random positions");

    // variables_map хранит значения опций после разбора
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.contains("help"s)) {
        // Если был указан параметр --help, то выводим справку и возвращаем nullopt
        std::cout << desc;
        return std::nullopt;
    }

    // Проверяем наличие опций
    if (!vm.contains("config-file"s)) {
        throw std::runtime_error("Config file have not been specified"s);
    }
    if (!vm.contains("www-root"s)) {
        throw std::runtime_error("Root dir path is not specified"s);
    }

    if (!vm.contains("tick-period"s)) {
        args.tick_period = {};
    }

    if (vm.contains("randomize-spawn-points"s)) {
        args.is_randomize = true;
    }

    if (!vm.contains("state-file"s)) {
        args.save_period = {};
    }

    // С опциями программы всё в порядке, возвращаем структуру args
    return args;
}

class Ticker : public std::enable_shared_from_this<Ticker> {
public:
    using Strand = net::strand<net::io_context::executor_type>;
    using Handler = std::function<void(std::chrono::milliseconds delta)>;

    // Функция handler будет вызываться внутри strand с интервалом period
    Ticker(Strand strand, std::chrono::milliseconds period, Handler handler)
        : strand_{strand}
        , period_{period}
        , handler_{std::move(handler)} {
    }

    void Start() {
        net::dispatch(strand_, [self = shared_from_this(), this] {
            last_tick_ = Clock::now();
            self->ScheduleTick();
        });
    }

private:
    void ScheduleTick() {
        assert(strand_.running_in_this_thread());
        timer_.expires_after(period_);
        timer_.async_wait([self = shared_from_this()](sys::error_code ec) {
            self->OnTick(ec);
        });
    }

    void OnTick(sys::error_code ec) {
        using namespace std::chrono;
        assert(strand_.running_in_this_thread());

        if (!ec) {
            auto this_tick = Clock::now();
            auto delta = duration_cast<milliseconds>(this_tick - last_tick_);
            last_tick_ = this_tick;
            try {
                handler_(delta);
            } catch (...) {
            }
            ScheduleTick();
        }
    }

    using Clock = std::chrono::steady_clock;

    Strand strand_;
    std::chrono::milliseconds period_;
    net::steady_timer timer_{strand_};
    Handler handler_;
    std::chrono::steady_clock::time_point last_tick_;
};

}  // namespace

int main(int argc, const char* argv[]) {
    try { 
        if (auto args_opt = ParseCommandLine(argc, argv)) {
            auto args = args_opt.value();

            LoggingRequestHandler<http_handler::RequestHandler>::InitLogging(); 
            // 1. Загружаем карту из файла и построить модель игры  
            model::Game game = json_loader::LoadGame(args.config_file.c_str());
            extra::ExtraData extra_data(args.config_file.c_str());
            app::Players players(game);
            app::PlayerTokens player_tokens;
            std::filesystem::path state_file_path;
            //Проверка необходимо ли восстанавливать состояние игры
            if(!args.save_file.empty()) {
                state_file_path = std::filesystem::weakly_canonical(args.save_file);
                state_file_path = std::filesystem::absolute(state_file_path);
                if(std::filesystem::exists(state_file_path)) {
                    std::ifstream in{state_file_path, std::ios_base::binary};
                    boost::archive::binary_iarchive ar{in};
                    serialization::SessionsRepr sessions_repr{};
                    serialization::PlayersRepr players_repr{};
                    serialization::PlayerTokensRepr players_tokens_repr{};
                    ar >> sessions_repr;
                    ar >> players_repr;
                    ar >> players_tokens_repr;
                    game.SetSessions(sessions_repr.Restore(game));
                    players.SetPlayers(players_repr.Restore());
                    player_tokens.SetTokens(players_tokens_repr.Restore(players));
                }
            }

            if(args.is_randomize) {
                game.SetRandomize();
            }

            const char* db_url = std::getenv("GAME_DB_URL");
            if (!db_url) {
                throw std::runtime_error("GAME_DB_URL is not specified");
            }
            postgres::DBParams db_params{10, std::string(db_url)};

            app::Application app(game, players, player_tokens, extra_data, db_params);
            infra::SerializationListener seria_listener(game, players, player_tokens);
            
            if((!args.save_file.empty())) {
                seria_listener.SetPathToStateFile(state_file_path);
                if(game.IsGameAuto()) {
                    if(!(args.save_period.empty())) {
                        uint64_t save_period_milliseconds = std::stoll(args.save_period);
                        std::chrono::milliseconds save_period(save_period_milliseconds);
                        seria_listener.SetSavePeriod(save_period);
                        app.SetApplicationListener(&seria_listener);
                    }
                } else {
                    app.SetApplicationListener(&seria_listener);
                }
            }

            // 2. Инициализируем io_context
            const unsigned num_threads = std::thread::hardware_concurrency();
            net::io_context ioc(num_threads);

            // strand для выполнения запросов к API
            auto api_strand = net::make_strand(ioc);

            // 3. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
            net::signal_set signals(ioc, SIGINT, SIGTERM);
            signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
                if (!ec) {
                    ioc.stop();
                }
            });
            
            //4. Если игра предполагает автоматическое обновление времени запускаем Ticker
            if(!args.tick_period.empty()) {
                game.SetAutoTick();
                uint64_t milliseconds = std::stoll(args.tick_period);
                std::chrono::milliseconds tick_duration(milliseconds);

                auto ticker = std::make_shared<Ticker>(api_strand, tick_duration,
                    [&app](std::chrono::milliseconds delta) { app.TickTimeUseCase(delta.count()); }
                );
                ticker->Start();
            }

            // 5. Создаём обработчик запросов в куче, управляемый shared_ptr
            auto handler = std::make_shared<http_handler::RequestHandler>(
                api_strand, app, args.root_dir.c_str());

            LoggingRequestHandler<http_handler::RequestHandler> LoggingDecorator(handler);
            const auto address = net::ip::make_address("0.0.0.0");
            constexpr net::ip::port_type port = 8080;

            // 6. Запустить обработчик HTTP-запросов, делегируя их декоратору
            http_server::ServeHttp(ioc, {address, port}, LoggingDecorator);

            std::cout << "Server has started"sv << std::endl;
            LoggingDecorator.LogStartServer(address, port);

            // 7. Запускаем обработку асинхронных операций
            RunWorkers(std::max(1u, num_threads), [&ioc] {
                ioc.run();
            });

            LoggingDecorator.LogExitServer();

            // Если указан файл сохранения, необходимо сохранить состояние игры
            if(!args.save_file.empty()) {
                //Сохраняем игровое состояние во временный файл
                auto temp_path = state_file_path.string() + "_temp"s;
                std::ofstream out{temp_path, std::ios_base::binary};
                boost::archive::binary_oarchive ar{out};
                serialization::SessionsRepr sessions_repr(game.GetSessions());
                serialization::PlayersRepr players_repr{players};
                serialization::PlayerTokensRepr players_tokens_repr{player_tokens};
                ar << sessions_repr;
                ar << players_repr;
                ar << players_tokens_repr;
                // Перезаписываем временный файл в целевой
                std::filesystem::rename(temp_path, state_file_path);
            }
        }
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}


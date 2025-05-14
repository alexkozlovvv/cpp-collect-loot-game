#pragma once
#include <pqxx/connection>
#include <pqxx/transaction>
#include <pqxx/pqxx>
#include <pqxx/zview.hxx>
#include "connection_pool.h"
#include "model.h"

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;
    
struct DBParams {
    size_t connection_count;
    std::string db_url;
}; 

struct PlayerRetireInfo {
    std::string name;
    int score;
    double playTime;
};

class Database {
public:
    explicit Database(const DBParams& db_params) : conn_pool_{db_params.connection_count, [url = db_params.db_url] () {
                                        auto conn = std::make_shared<pqxx::connection>(url);
                                        return conn; }} {
        auto conn = conn_pool_.GetConnection();
        {
            pqxx::work work{*conn};
            work.exec(R"(
            CREATE TABLE IF NOT EXISTS retired_players (
                id SERIAL PRIMARY KEY,
                name varchar(100) NOT NULL,
                score INTEGER NOT NULL,
                playTime float8 NOT NULL
            );
            )"_zv);

            work.exec(R"(
            CREATE INDEX IF NOT EXISTS  score_player_time_name_idx ON retired_players (score DESC, playTime, name);
            )"_zv);
            work.commit();
        }
    }

    void SetDogToDB(const model::Dog& dog) const {
        auto conn = conn_pool_.GetConnection();
        {
            auto time_game = dog.GetInGameTime();
            double seconds = static_cast<double>(time_game.count()) / 1000;
            pqxx::work work{*conn};
            work.exec_params(R"(
            INSERT INTO retired_players (name, score, playTime) 
            VALUES ($1, $2, $3);
            )"_zv, dog.GetName(), dog.GetScore(), seconds);
            work.commit();
        }
    }

    std::vector<PlayerRetireInfo> GetRetirePlayersInfo(std::string start_idx, std::string limit) const {
        std::vector<PlayerRetireInfo> retire_players_info;
        auto conn = conn_pool_.GetConnection();
        {
            pqxx::work work{*conn};
            std::string query = "SELECT name, score, playTime FROM retired_players ORDER BY score DESC, playTime, name LIMIT " + limit + " OFFSET " + start_idx + ";";
            try{
                for (auto [name, score, playTime] : work.query<std::string, int, double>(query)) {
                    retire_players_info.push_back(PlayerRetireInfo{name, score, playTime});
                }
            } catch (const pqxx::sql_error &e) {
                std::cerr << e.what() << std::endl;
            }
            work.commit();
        }
        return retire_players_info;
    }

private:
    mutable ConnectionPool conn_pool_;
};

}
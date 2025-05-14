#include "infrastructure.h"
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include "model_serialization.h"
#include "app_serialization.h"


namespace infra {
    
void SerializationListener::OnTick(SerializationListener::TimeInterval time_delta) {
    if(game_.IsGameAuto()) {
        if(time_since_save_ + time_delta > save_period_) {
            time_since_save_ = TimeInterval{0};
        } else {
            time_since_save_ += time_delta;
        }
    }
    auto temp_path = path_to_state_file_.string() + "_temp"s;
    std::ofstream out{temp_path, std::ios_base::binary};
    boost::archive::binary_oarchive ar{out};
    serialization::SessionsRepr sessions_repr(game_.GetSessions());
    serialization::PlayersRepr players_repr{players_};
    serialization::PlayerTokensRepr players_tokens_repr{player_tokens_};
    ar << sessions_repr;
    ar << players_repr;
    ar << players_tokens_repr;
    // Перезаписываем временный файл в целевой
    std::filesystem::rename(temp_path, path_to_state_file_);
}

void SerializationListener::SetSavePeriod(std::chrono::milliseconds save_period) {
    save_period_ = save_period;
}

void SerializationListener::SetPathToStateFile(std::filesystem::path path_to_state_file) {
    path_to_state_file_ = path_to_state_file;
}

}
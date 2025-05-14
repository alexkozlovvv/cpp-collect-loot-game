#pragma once

#include "app.h"


using namespace std::literals;

namespace infra {


class SerializationListener : public app::ApplicationListener {
public:
    using TimeInterval = std::chrono::milliseconds;
    SerializationListener(model::Game& game, app::Players& players, app::PlayerTokens& player_tokens) :
        game_(game), players_(players), player_tokens_(player_tokens) {}

    void OnTick(std::chrono::milliseconds time_delta) override;
    void SetSavePeriod(std::chrono::milliseconds save_period);
    void SetPathToStateFile(std::filesystem::path path_to_state_file);

private:
    model::Game& game_;
    app::Players& players_;
    app::PlayerTokens& player_tokens_;
    TimeInterval save_period_;
    TimeInterval time_since_save_;
    std::filesystem::path path_to_state_file_;
};




}
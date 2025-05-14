#include "app.h"

namespace app {

using namespace std::literals;

bool IsValidAuthorizationField(std::string_view authorization_field, app::Token& token) {
    if(authorization_field.find("Bearer") != std::string::npos) {
        auto last_space_pos = authorization_field.find_last_of(' ');
        if(last_space_pos == std::string::npos) {
            return false;
        } 
        auto token_str = std::string(authorization_field).substr(last_space_pos + 1);
        if (token_str.size() == 32) {
            *token = token_str;
            return true;
        }
    }
    return false;
}

bool IsValidMoveDirection(const std::string& move_direction) {
    if(move_direction.empty()){
        return true;
    }
    if(move_direction != "L" && move_direction != "R" &&
       move_direction != "U" && move_direction != "D" ) {
        return false;
    }
    return true;
}

double roundToOneDecimal(double value) {
    return std::round(value * 10.0) / 10.0;
}

std::string Player::GetName() const {
    return user_name_;
}

Player::Id Player::GetPlayerId() const {
    return player_index_;
}

model::Map::Id Player::GetMapId() const {
    return map_id_;
}

Token PlayerTokens::AddPlayer(Player& player) {
    // Генерируем новый токен
    std::uint64_t value1 = generator1_();
    std::uint64_t value2 = generator2_();

    // Преобразуем числа в шестнадцатеричные строки
    std::stringstream ss1;
    ss1 << std::hex << std::setfill('0') << std::setw(16) << value1;
    std::string hex_value1 = ss1.str();

    std::stringstream ss2;
    ss2 << std::hex << std::setfill('0') << std::setw(16) << value2;
    std::string hex_value2 = ss2.str();

    Token new_token(hex_value1 + hex_value2);

    // Добавляем нового игрока
    auto added_token_it = token_to_player.insert({new_token, &player}).first;

    return (*added_token_it).first;
}

Player* PlayerTokens::FindPlayerByToken(Token token) {
    if(token_to_player.count(token) == 1) {
        return token_to_player.at(token);
    }
    return nullptr;
}

Player& Players::Add(util::Tagged<std::string, model::Map> map_id, std::string user_name) {
    auto session_ptr = game_.FindSession(map_id);
    if(session_ptr == nullptr) {                            /* Это лишняя проверка т.к. сессия уже была создана и проверена */
        // Добавляем новую сессию
        auto map_ptr = game_.FindMap(map_id);
        model::GameSession session(map_ptr, game_.GetRandomize(), game_.GetLootGeneratorConfig());
        game_.AddSession(session);
        session_ptr = game_.FindSession(map_id);
    }
    auto added_dog_index = session_ptr->AddDog(user_name);
    Player new_player(user_name, added_dog_index, map_id);
    players_.insert({{added_dog_index, map_id}, new_player}).first;
    return players_.at({added_dog_index, map_id});
}

size_t Players::GetPlayersCount(model::Map::Id map_id) {
    auto session_ptr = game_.FindSession(map_id);
    return session_ptr->GetDogsCount(); 
}

Player* Players::FindByPlayerIdAndMapId(size_t i, model::Map::Id player_map_id) {
    return &players_.at({i, player_map_id});
}

const model::Map* Application::GetMapUseCase(const std::string& map_id_str) const {
    const auto& maps = game_.GetMaps();
    const auto is_target_map = [&map_id_str](const model::Map& map) { 
        return *(map.GetId()) == map_id_str; };
    auto it = find_if(maps.begin(), maps.end(), is_target_map);
    if(it != maps.end()) {
        return &(*it);
    }
    return nullptr;
} 

const model::Game::Maps& Application::ListMapsUseCase () const {
    return game_.GetMaps();
}

bool Application::IsGameAuto () const {
    return game_.IsGameAuto();
}

const std::vector<Player*> Application::ListPlayersUseCase(const Token& token) const {
    std::vector<Player*> session_players;
    auto player_ptr = player_tokens_.FindPlayerByToken(token);
    if(player_ptr == nullptr) {
        return {};
    }
    // Возвращаем список игроков из одной игровой сессии
    auto player_map_id = player_ptr->GetMapId();
    auto players_count = players_.GetPlayersCount(player_map_id);

    for(auto i = 0U; i < players_count; i++) {
        auto session_player = players_.FindByPlayerIdAndMapId(i, player_map_id);
        session_players.push_back(session_player);
    }
    return session_players;
}

JoinGameResult Application::JoinGameUseCase(std::string user_name, const model::Map::Id& map_id) const {

    auto map_ptr = game_.FindMap(map_id);

    if(user_name.empty()) {
        throw JoinGameError(JoinGameError::JoinGameErrorReason::INVALID_NAME);
    } else if (map_ptr == nullptr){
        throw JoinGameError(JoinGameError::JoinGameErrorReason::INVALID_MAP);
    }
    if(game_.FindSession(map_id) == nullptr) {
        model::GameSession session(map_ptr, game_.GetRandomize(), game_.GetLootGeneratorConfig());
        game_.AddSession(session);
    }
    auto& added_player = players_.Add(map_id, user_name);
    auto new_token = player_tokens_.AddPlayer(added_player);

    return {new_token, added_player.GetPlayerId()};
}

const model::GameSession* Application::GetGameStateUseCase(std::string_view authorization_field) const {
    app::Token token(std::string(""));
    if(!app::IsValidAuthorizationField(authorization_field, token)) {
        throw GetGameStateError(GetGameStateError::GetGameStateErrorReason::INVALID_AUTH_FIELD);
    }
    const auto player_ptr = player_tokens_.FindPlayerByToken(token);
    
    if(player_ptr == nullptr) {
        throw GetGameStateError(GetGameStateError::GetGameStateErrorReason::INVALID_TOKEN);
    }
    auto player_map_id = player_ptr->GetMapId();
    const auto session_ptr = game_.FindSession(player_map_id);
    //return session_ptr->GetDogs();
    return session_ptr;
}

const extra::ExtraData& Application::GetExtraData() const {
    return extra_data_;
}

void Application::MovePlayersUseCase(std::string_view authorization_field, std::string move_direction) const {
    app::Token token(std::string(""));
    if(!app::IsValidAuthorizationField(authorization_field, token)) {
        throw MovePlayersError(MovePlayersError::MovePlayersErrorReason::INVALID_AUTH_FIELD);
    }
    const auto player_ptr = player_tokens_.FindPlayerByToken(token);
    if(player_ptr == nullptr) {
        throw MovePlayersError(MovePlayersError::MovePlayersErrorReason::INVALID_TOKEN);
    }
    if(!IsValidMoveDirection(move_direction)) {
        throw MovePlayersError(MovePlayersError::MovePlayersErrorReason::INVALID_MOVE_ARG);
    }
    auto player_session = game_.FindSession(player_ptr->GetMapId());
    auto map_dog_speed = game_.FindMap(player_ptr->GetMapId())->GetDogSpeed();
    player_session->FindDog(*(player_ptr->GetPlayerId()))->SetSpeed(move_direction, map_dog_speed);
}

const std::vector<postgres::PlayerRetireInfo> Application::GetRetirePlayersUseCase(std::vector<std::pair<std::string, std::string>> params) const {
    std::string start_idx = "0"s;
    std::string size = "100"s;
    for(auto& param_item : params) {
        if(param_item.first == "start"s) {
            start_idx = param_item.second;
        } else if(param_item.first == "maxItems"s) {
            size = param_item.second;
        }
    }
    return DB.GetRetirePlayersInfo(start_idx, size);
}

void Application::TickTimeUseCase(std::uint64_t time_Delta) const {
    const double shift = 0.4;
    double time_Delta_sec = static_cast<double>(time_Delta) / 1000;
    auto& sessions = game_.GetSessions();
    for(auto& session_items : sessions) {
        std::vector<collision_detector::Gatherer> gatherers;
        std::vector<std::uint64_t> Dogs_to_delete_idxs;
        auto& dogs = session_items.second.GetDogs();
        for(auto& dog : dogs) {
            const auto& dog_coords = dog.second.GetCoords();
            geom::Point2D gatherer_start_pos(dog_coords.x, dog_coords.y);
            const auto& dog_speed = dog.second.GetSpeed();
            if(dog_speed.h_s == 0 && dog_speed.v_s == 0) {
                if(game_.GetDogRetireTime() <= (dog.second.GetStandByTime() + std::chrono::milliseconds(time_Delta))) {
                    Dogs_to_delete_idxs.push_back(dog.first);
                    auto dif = game_.GetDogRetireTime() - dog.second.GetStandByTime();
                    dog.second.AddInGameTime(dog.second.GetStandByTime() + dif);
                } else {
                    dog.second.AddStandByTime(std::chrono::milliseconds(time_Delta));
                }
            } else {
                if(dog.second.GetStandByTime().count() != 0) {
                    dog.second.AddInGameTime(dog.second.GetStandByTime());
                    dog.second.SetStandByTime(std::chrono::milliseconds(0));
                }
                dog.second.AddInGameTime(std::chrono::milliseconds(time_Delta));
            }
            auto map_ptr = game_.FindMap(session_items.second.GetIDMap());
            auto hor_road = map_ptr->FindHorRoad(dog_coords.x, dog_coords.y);
            auto ver_road = map_ptr->FindVertRoad(dog_coords.x, dog_coords.y);
            if(dog_speed.h_s < 0) {
                double hor_left_limit;
                if(hor_road != nullptr){
                    hor_left_limit = std::min(hor_road->GetStart().x, hor_road->GetEnd().x) - shift;
                } else {
                    hor_left_limit = ver_road->GetStart().x - shift;
                }
                double new_x_coord = dog_coords.x + (dog_speed.h_s * time_Delta_sec);
                if(new_x_coord > hor_left_limit) {
                    dog.second.SetCoords({(new_x_coord), (dog_coords.y)});
                } else {
                    dog.second.SetCoords({roundToOneDecimal(hor_left_limit), (dog_coords.y)});
                    dog.second.SetSpeed({}, 0.0);
                }
            } else if(dog_speed.h_s > 0) {
                double hor_right_limit;
                if(hor_road != nullptr){
                    hor_right_limit = std::max(hor_road->GetStart().x, hor_road->GetEnd().x) + shift;
                } else {
                    hor_right_limit = ver_road->GetStart().x + shift;
                }
                double new_x_coord = dog_coords.x + (dog_speed.h_s * time_Delta_sec);
                if(new_x_coord < hor_right_limit) {
                    dog.second.SetCoords({(new_x_coord), (dog_coords.y)});
                } else {
                    dog.second.SetCoords({roundToOneDecimal(hor_right_limit), (dog_coords.y)});
                    dog.second.SetSpeed({}, 0.0);
                }
            } else if (dog_speed.v_s < 0) {
                double ver_up_limit;
                if(ver_road != nullptr){
                    ver_up_limit = std::min(ver_road->GetStart().y, ver_road->GetEnd().y) - shift;
                } else {
                    ver_up_limit = hor_road->GetStart().y - shift;
                }
                double new_y_coord = dog_coords.y + (dog_speed.v_s * time_Delta_sec);
                if(new_y_coord > ver_up_limit) {
                    dog.second.SetCoords({(dog_coords.x), (new_y_coord)});
                } else {
                    dog.second.SetCoords({(dog_coords.x), roundToOneDecimal(ver_up_limit)});
                    dog.second.SetSpeed({}, 0.0);
                }
            } else if (dog_speed.v_s > 0) {
                double ver_down_limit;
                if(ver_road != nullptr){
                    ver_down_limit = std::max(ver_road->GetStart().y, ver_road->GetEnd().y) + shift;
                } else {
                    ver_down_limit = hor_road->GetStart().y + shift;
                }
                double new_y_coord = dog_coords.y + (dog_speed.v_s * time_Delta_sec);
                if(new_y_coord < ver_down_limit) {
                    dog.second.SetCoords({(dog_coords.x), (new_y_coord)});
                } else {
                    dog.second.SetCoords({(dog_coords.x), roundToOneDecimal(ver_down_limit)});
                    dog.second.SetSpeed({}, 0.0);
                }
            }
            geom::Point2D gatherer_end_pos(dog_coords.x, dog_coords.y);
            gatherers.push_back({gatherer_start_pos, gatherer_end_pos, 0.3});
        }
        session_items.second.HandleEvents(gatherers);
        if(!Dogs_to_delete_idxs.empty()) {
            for(auto dog_idx : Dogs_to_delete_idxs) {
                const auto& dog = dogs.at(dog_idx);
                DB.SetDogToDB(dog);
                auto tokens = player_tokens_.GetTokens();
                for(auto it = tokens.begin(); it != tokens.end(); ) {
                    if((*(*it).second->GetPlayerId() == dog_idx) &&
                         (*it).second->GetName() == dog.GetName() &&
                         (*it).second->GetMapId() == session_items.second.GetIDMap()) {
                        it = tokens.erase(it);
                    } else {
                        ++it;
                    }
                }
                player_tokens_.SetTokens(tokens);
                players_.DeletePlayer(dog_idx , session_items.second.GetIDMap());
                dogs.erase(dog_idx);
            }
        }
        session_items.second.AddLoot(std::chrono::milliseconds(time_Delta));
        if(listener_ != nullptr) {
            listener_->OnTick(std::chrono::milliseconds(time_Delta));
        }
    }
}

void Application::SetApplicationListener(ApplicationListener* listener) {
    listener_ = listener;
}

}
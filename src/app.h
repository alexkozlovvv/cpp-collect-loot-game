#pragma once
#include <random>
#include <sstream>
#include <iomanip>
#include "model.h"
#include "extra_data.h"
#include "loot_generator.h"
#include "collision_detector.h"
#include "postgres.h"

namespace app { 

namespace detail {
struct TokenTag {};
}  // namespace detail

struct AppConfig {
    std::string db_url;
};

using Token = util::Tagged<std::string, detail::TokenTag>;

class Player {
public:
    using Id = util::Tagged<std::uint64_t, Player>;

    Player(std::string user_name, std::uint64_t player_index, model::Map::Id map_id) : 
            user_name_(user_name), player_index_(player_index), map_id_(map_id) {}

    std::string GetName() const;
    Id GetPlayerId() const;
    model::Map::Id GetMapId() const;

private:
    std::string user_name_;
    Id player_index_;
    model::Map::Id map_id_;
};

class PlayerTokens {
public:
    using TokenToPlayerPtr = std::unordered_map<Token, Player*, util::TaggedHasher<Token>>;
    
    Token AddPlayer(Player& player);
    Player* FindPlayerByToken(Token token);
    TokenToPlayerPtr GetTokens() const {
        return token_to_player;
    }
    // TokenToPlayerPtr& GetTokensRef() {
    //     return token_to_player;
    // }
    void SetTokens(const TokenToPlayerPtr& token_to_player_){
        token_to_player = token_to_player_;
    }
    
private:
    std::random_device random_device_;
    std::mt19937_64 generator1_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};
    std::mt19937_64 generator2_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};
    // Чтобы сгенерировать токен, получаем из generator1_ и generator2_
    // два 64-разрядных числа и, переводим их в hex-строки, склеиваем в одну.
    // Можно поэкспериментировать с алгоритмом генерирования токенов,
    // чтобы сделать их подбор ещё более затруднительным

    std::unordered_map<Token, Player*, util::TaggedHasher<Token>> token_to_player;
};
 
class Players {
public:
    struct DogIdMapIdHasher {
        size_t operator()(const std::pair<std::uint64_t, model::Map::Id>& pair) const {
            size_t hash_value = std::hash<std::uint64_t>()(pair.first); 
            hash_value ^= MapIdHasher()(pair.second); // Комбинируем хэши
            return hash_value;
        }
    };
    using MapIdHasher = util::TaggedHasher<model::Map::Id>;
    using DogIdMapIdToPlayer = std::unordered_map<std::pair<std::uint64_t, model::Map::Id>, Player, DogIdMapIdHasher>;

    explicit Players(model::Game& game) : game_(game) {}

    Player& Add(util::Tagged<std::string, model::Map> map_id, std::string user_name);
    size_t GetPlayersCount(model::Map::Id map_id);
    Player* FindByPlayerIdAndMapId(size_t i, model::Map::Id player_map_id);
    DogIdMapIdToPlayer GetPlayers() const {
        return players_;
    }
    void SetPlayers(const DogIdMapIdToPlayer& players){
        players_ = players;
    }

    void DeletePlayer(std::uint64_t player_idx ,model::Map::Id map_id) {
        players_.erase({player_idx, map_id});
    }

private:
    model::Game& game_;
    DogIdMapIdToPlayer players_;
};

struct JoinGameResult {
    Token player_token;
    Player::Id player_id;
};

class JoinGameError {
public:
    enum class JoinGameErrorReason{
        INVALID_NAME,
        INVALID_MAP
    };

    explicit JoinGameError(JoinGameErrorReason reason) : reason_(reason) {}
    
    const JoinGameErrorReason What() const {
        return reason_;
    }

private:
    JoinGameErrorReason reason_;
};


class GetGameStateError {
public:
    enum class GetGameStateErrorReason{
        INVALID_AUTH_FIELD,
        INVALID_TOKEN
    };

    explicit GetGameStateError(GetGameStateErrorReason reason) : reason_(reason) {}
    
    const GetGameStateErrorReason What() const {
        return reason_;
    }

private:
    GetGameStateErrorReason reason_;
};

class MovePlayersError {
public:
    enum class MovePlayersErrorReason{
        INVALID_AUTH_FIELD,
        INVALID_TOKEN,
        INVALID_MOVE_ARG
    };

    explicit MovePlayersError(MovePlayersErrorReason reason) : reason_(reason) {}
    
    const MovePlayersErrorReason What() const {
        return reason_;
    }
private:
    MovePlayersErrorReason reason_;
};

bool IsValidAuthorizationField(std::string_view authorization_field, app::Token& token);

bool IsValidMoveDirection(const std::string& move_direction);

double roundToOneDecimal(double value);

class ApplicationListener {
public:
    virtual void OnTick(std::chrono::milliseconds time_delta) = 0;
};

class Application {
public:
    Application(model::Game& game, app::Players& players, app::PlayerTokens& player_tokens, extra::ExtraData& extra_data, const postgres::DBParams& db_params) :
        game_(game), players_(players), player_tokens_(player_tokens), extra_data_(extra_data), DB(postgres::Database(db_params)) {}

    bool IsGameAuto () const;

    const model::Map* GetMapUseCase(const std::string& map_id_str) const;
    const extra::ExtraData& GetExtraData() const;
    const model::Game::Maps& ListMapsUseCase () const;
    const std::vector<Player*> ListPlayersUseCase(const Token& token) const;
    JoinGameResult JoinGameUseCase(std::string user_name, const model::Map::Id& map_id) const;
    const model::GameSession* GetGameStateUseCase(std::string_view authorization_field) const;
    void MovePlayersUseCase(std::string_view authorization_field, std::string move_direction) const;
    void TickTimeUseCase(std::uint64_t time_Delta) const;
    void SetApplicationListener(ApplicationListener* listener);
    const std::vector<postgres::PlayerRetireInfo> GetRetirePlayersUseCase(std::vector<std::pair<std::string, std::string>> params) const;

private:
    model::Game& game_;
    app::Players& players_;
    app::PlayerTokens& player_tokens_;
    extra::ExtraData& extra_data_;
    ApplicationListener* listener_ = nullptr;
    postgres::Database DB;
};

}
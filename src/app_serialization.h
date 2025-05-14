#pragma once
#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/unordered_map.hpp>

#include "app.h"

using namespace std::literals;

namespace model {
    template <typename Archive>
    void serialize(Archive& ar, model::Map::Id& map_id, [[maybe_unused]] const unsigned version) {
        ar&(*map_id);
    }
}

namespace app {

    template <typename Archive>
    void serialize(Archive& ar, app::Token& token, [[maybe_unused]] const unsigned version) {
        ar& (*token);
    }

}  // namespace app

namespace serialization {

class PlayerRepr {
public:
    PlayerRepr() = default;

    explicit PlayerRepr(const app::Player& player)
        : user_name_(player.GetName())
        , player_index_(*(player.GetPlayerId()))
        , map_id_(*(player.GetMapId())) {
    }

    [[nodiscard]] app::Player Restore() const {
        app::Player player{user_name_, player_index_, map_id_};
        return player;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& user_name_;
        ar& player_index_;
        ar& *map_id_;
    }

private:
    std::string user_name_{""s};;
    std::uint64_t player_index_{0};
    model::Map::Id map_id_{""s};
};

class PlayersRepr {
public:
    using PlayersReprs = std::vector<PlayerRepr>;
    PlayersRepr() = default;

    explicit PlayersRepr(const app::Players& players)
    {
        auto players_items = players.GetPlayers();
        for(auto& player_item : players_items) {
            PlayerRepr player_repr(player_item.second);
            players_reprs_.push_back(player_repr);
        }
    }

    [[nodiscard]] app::Players::DogIdMapIdToPlayer Restore() {
        //app::Players players{game};
        for(const auto& player_repr : players_reprs_) {
            auto restored_player = player_repr.Restore();
            auto key = std::pair(*(restored_player.GetPlayerId()), restored_player.GetMapId());
            players_.insert({key, restored_player});
        }
        return players_;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& players_reprs_;
    }

private:
    app::Players::DogIdMapIdToPlayer players_;
    PlayersReprs players_reprs_;
};

class PlayerTokensRepr {
public:

    using TokenToPlayerRepr = std::map<std::string, PlayerRepr>;
    PlayerTokensRepr() = default;

    explicit PlayerTokensRepr(const app::PlayerTokens& players_tokens)
        {
            auto tokens_to_players = players_tokens.GetTokens();
            for(const auto& token_to_player : tokens_to_players) {
                PlayerRepr player_repr(*(token_to_player.second));
                tokens_to_players_reprs_.insert({*(token_to_player.first), player_repr});
        }
    }

    [[nodiscard]] app::PlayerTokens::TokenToPlayerPtr Restore(app::Players& players) {
        /* Проходимся по всем токенам и узнаем данные игрока
           ищем указатель на этого игрока из players и формируем players_tokens */
        for(auto& token_to_playerrepr : tokens_to_players_reprs_) {
            auto player = token_to_playerrepr.second.Restore();
            auto player_ptr = players.FindByPlayerIdAndMapId(*(player.GetPlayerId()), player.GetMapId());
            tokens_to_players_ptrs_.insert({app::Token(token_to_playerrepr.first), player_ptr});
        }
        return tokens_to_players_ptrs_;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& tokens_to_players_reprs_;
    }

private:
    app::PlayerTokens::TokenToPlayerPtr tokens_to_players_ptrs_;
    TokenToPlayerRepr tokens_to_players_reprs_;
};


}  // namespace serialization
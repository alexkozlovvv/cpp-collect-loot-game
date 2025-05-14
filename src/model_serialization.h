#pragma once
#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>

#include "model.h"

using namespace std::literals;

namespace geom {

template <typename Archive>
void serialize(Archive& ar, Point2D& point, [[maybe_unused]] const unsigned version) {
    ar& point.x;
    ar& point.y;
}

template <typename Archive>
void serialize(Archive& ar, Vec2D& vec, [[maybe_unused]] const unsigned version) {
    ar& vec.x;
    ar& vec.y;
}

}  // namespace geom

namespace model {

template <typename Archive>
void serialize(Archive& ar, Dog::coords& obj, [[maybe_unused]] const unsigned version) {
    ar&(obj.x);
    ar&(obj.y);
}

template <typename Archive>
void serialize(Archive& ar, Loot::coords& obj, [[maybe_unused]] const unsigned version) {
    ar&(obj.x);
    ar&(obj.y);
}

template <typename Archive>
void serialize(Archive& ar, Dog::speed& obj, [[maybe_unused]] const unsigned version) {
    ar&(obj.h_s);
    ar&(obj.v_s);
}

}  // namespace model

namespace serialization {

// DogRepr (DogRepresentation) - сериализованное представление класса Dog
class DogRepr {
public:
    DogRepr() = default;

    explicit DogRepr(const model::Dog& dog, double map_dog_speed)
        : name_(dog.GetName())
        , coords_(dog.GetCoords())
        , speed_(dog.GetSpeed())
        , direction_(dog.GetDirection())
        , bag_content_(dog.GetBag())
        , score_(dog.GetScore())
        , map_dog_speed_(map_dog_speed) {
    }

    [[nodiscard]] model::Dog Restore() const {
        model::Dog dog{name_, coords_};
        dog.SetSpeedOnly(speed_);
        dog.SetDirection(direction_);
        dog.AddScore(score_);
        for (const auto& item : bag_content_) {
            model::Loot temp_loot(item.second, {0.0, 0.0});
            dog.PutLootInTheBag(item.first, temp_loot);
            // if (!dog.PutLootInTheBag(item)) {
            //     throw std::runtime_error("Failed to put bag content");
            // }
        }
        return dog;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& name_;
        ar& coords_;
        ar& speed_;
        ar& direction_;
        ar& bag_content_;
        ar& score_;
        ar& map_dog_speed_;
    }

private:
    std::string name_;
    model::Dog::coords coords_;
    model::Dog::speed speed_;
    model::Dog::Direction direction_ = model::Dog::Direction::NORTH;
    model::Dog::IndexToLootType bag_content_;
    int score_ = 0;
    double map_dog_speed_;
};

class LootRepr {
public:
    LootRepr() = default;

    explicit LootRepr(const model::Loot& loot)
        : loot_type_(loot.GetLootType())
        , coords_(loot.GetCoords()) {
    }

    [[nodiscard]] model::Loot Restore() const {
        model::Loot Loot{loot_type_, coords_};
        return Loot;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& loot_type_;
        ar& coords_;
    }

private:
    int loot_type_ = 0;
    model::Loot::coords coords_;
};

class SessionRepr {
public:
    using IndexToDogRepr = std::map<std::uint64_t, DogRepr>;
    using IndexToLootRepr = std::map<std::uint64_t, LootRepr>;
    
    SessionRepr() = default;

    explicit SessionRepr(const model::GameSession& session)
        : map_ptr_(session.GetMapPtr())
        , map_id_(*(session.GetIDMap()))
        , dogs_(session.GetDogs())
        , loots_(session.GetLoots())
        , dog_index_(session.GetDogsIndex())
        , loot_index_(session.GetLootsIndex()) {
        map_dog_speed_ = session.GetMapPtr()->GetDogSpeed();
    }

    [[nodiscard]] model::GameSession Restore(model::Game& game) {
        /* Находим карту с указанным ID в игре */
        auto map_ptr = game.FindMap(map_id_);
        map_dog_speed_ = map_ptr->GetDogSpeed();
        if(map_ptr == nullptr) {
            model::GameSession invalid_session(nullptr, false, {});
            return invalid_session;
        }
        auto is_game_randomize = game.GetRandomize();
        auto loot_generator_conf = game.GetLootGeneratorConfig();
        model::GameSession session{map_ptr, is_game_randomize, loot_generator_conf};
        session.SetDogsIndex(dog_index_);
        session.SetLootsIndex(loot_index_);
        /* Восстанавливаем dogs_ */
        for(const auto& dog_item : dogs_reprs_) {
            //model::Dog dog = dog_item.second.Restore();
            dogs_.insert({dog_item.first, dog_item.second.Restore()});
        }
        session.SetDogs(dogs_);
        /* Восстанавливаем loots_ */
        for(const auto& loot_item : loots_reprs_) {
            loots_.insert({loot_item.first, loot_item.second.Restore()});
        }
        session.SetLoots(loots_);
        return session;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& *(map_id_);
        /* Преобразование в DogRepr */
        for(const auto& dog_item : dogs_) {
            DogRepr dogrepr(dog_item.second, map_dog_speed_);
            dogs_reprs_.insert({dog_item.first, dogrepr});
        }
        ar& dogs_reprs_;
        /* Преобразование в LootRepr */
        for(const auto& loot_item : loots_) {
            LootRepr lootrepr(loot_item.second);
            loots_reprs_.insert({loot_item.first, lootrepr});
        }
        ar& loots_reprs_;
        ar& dog_index_;
        ar& loot_index_;
    }

private:
    const model::Map* map_ptr_ = nullptr;
    model::Map::Id map_id_{""};
    model::GameSession::IndexToDog dogs_;
    model::GameSession::IndexToLoot loots_;
    std::uint64_t dog_index_ = 0;
    std::uint64_t loot_index_ = 0;
    IndexToDogRepr dogs_reprs_;
    IndexToLootRepr loots_reprs_;
    double map_dog_speed_;

};

class SessionsRepr {
public:
    SessionsRepr() = default;

    explicit SessionsRepr(const model::Game::MapIdToSession& sessions) {
        for(const auto& session_item : sessions) {
            SessionRepr sesson_repr(session_item.second);
            sessions_reprs_.push_back(sesson_repr);
        }
    }

    [[nodiscard]] model::Game::MapIdToSession Restore(model::Game &game) {
        /* Находим карту с указанным ID в игре */
        for(auto& session_repr : sessions_reprs_) {
            auto restored_session = session_repr.Restore(game);
            restored_sessions_.insert({restored_session.GetIDMap(), restored_session});
        }
        return restored_sessions_;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& sessions_reprs_;
    }

private:
    std::vector<SessionRepr> sessions_reprs_; 
    model::Game::MapIdToSession restored_sessions_;
};

}  // namespace serialization
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <catch2/catch_test_macros.hpp>
#include <sstream>

#include "../src/model.h"
#include "../src/model_serialization.h"
#include "../src/app.h"
#include "../src/app_serialization.h"

using namespace model;
using namespace app;
using namespace std::literals;
namespace {

using InputArchive = boost::archive::text_iarchive;
using OutputArchive = boost::archive::text_oarchive;

struct Fixture {
    std::stringstream strm;
    OutputArchive output_archive{strm};
};

}  // namespace

// SCENARIO_METHOD(Fixture, "Point serialization") {
//     GIVEN("A point") {
//         const geom::Point2D p{10, 20};
//         WHEN("point is serialized") {
//             output_archive << p;

//             THEN("it is equal to point after serialization") {
//                 InputArchive input_archive{strm};
//                 geom::Point2D restored_point;
//                 input_archive >> restored_point;
//                 CHECK(p == restored_point);
//             }
//         }
//     }
// }
template<typename map_type>
bool AreEqual(const map_type& map1, const map_type& map2){
    bool areEqual = true;
    for (auto it1 = map1.begin(), it2 = map2.begin(); 
        it1 != map1.end() && it2 != map2.end();
        ++it1, ++it2) {
        if (it1->first != it2->first || it1->second != it2->second) {
            areEqual = false;
            break;
        }
    }
    return areEqual;
}

SCENARIO_METHOD(Fixture, "Dog Serialization") {
    GIVEN("a dog") {
        const auto dog = [] {
            Dog dog{"Pluto"s, {42.2, 12.5}};
            dog.SetSpeed("R"s, 3);
            dog.PutLootInTheBag(0, Loot(1, {1.0, 0.0}));
            dog.PutLootInTheBag(1, Loot(1, {2.0, 0.0}));
            dog.AddScore(42);
            return dog;
        }();

        WHEN("dog is serialized") {
            {
                serialization::DogRepr repr{dog, 3.0};
                output_archive << repr;
            }

            THEN("it can be deserialized") {
                InputArchive input_archive{strm};
                serialization::DogRepr repr;
                input_archive >> repr;
                const auto restored = repr.Restore();

                CHECK(dog.GetName() == restored.GetName());
                CHECK(dog.GetCoords() == restored.GetCoords());
                CHECK(dog.GetSpeed() == restored.GetSpeed());
                CHECK(dog.GetDirection() == restored.GetDirection());
                CHECK(dog.GetBag() == restored.GetBag());
            }
        }
    }
}
SCENARIO_METHOD(Fixture, "GameSession Serialization") {
    GIVEN("a game with session") {
        using loot_gen::LootGenerator;
        using TimeInterval = LootGenerator::TimeInterval;

        TimeInterval generator_base_interval{1000};
        double probability{0.8};

        Road road1(model::Road::HORIZONTAL, {0, 0}, 30);
        Road road2(model::Road::VERTICAL, {30, 0}, 30);
        Road road3(model::Road::HORIZONTAL, {30, 30}, 0);
        Road road4(model::Road::VERTICAL, {0, 30}, 0);

        Map::Id map_id("map_1"s);
        Map test_map(map_id, "Test_map"s);
        test_map.AddRoad(road1);
        test_map.AddRoad(road2);
        test_map.AddRoad(road3);
        test_map.AddRoad(road4);
        test_map.SetLootTypeCount(6);
        test_map.SetDogSpeed(3.0);

        Game game;
        game.AddMap(test_map);
        game.SetLootGeneratorConfig(generator_base_interval, probability);
        auto map_ptr = game.FindMap(map_id);
        GameSession session{map_ptr, game.GetRandomize(), game.GetLootGeneratorConfig()};
        session.AddDog("Scooby Doo");
        session.AddDog("Hatiko");
        auto dog1_ptr = session.FindDog(0);
        auto dog2_ptr = session.FindDog(1);
        dog1_ptr->AddScore(48);
        dog1_ptr->PutLootInTheBag(0, Loot(1, {1.0, 0.0}));
        dog1_ptr->PutLootInTheBag(1, Loot(5, {1.0, 0.0}));
        dog2_ptr->AddScore(12);
        dog1_ptr->PutLootInTheBag(2, Loot(2, {1.0, 0.0}));
        dog1_ptr->PutLootInTheBag(3, Loot(4, {1.0, 0.0}));
        TimeInterval time_delta{5000};
        session.AddLoot(time_delta);
        WHEN("session is serialized") {
            {
                serialization::SessionRepr repr{session};
                output_archive << repr;
            }

            THEN("it can be deserialized") {
                InputArchive input_archive{strm};
                serialization::SessionRepr repr;
                input_archive >> repr;
                const auto restored = repr.Restore(game);
                CHECK(*(session.GetIDMap()) == *(restored.GetIDMap()));
                CHECK(session.GetDogsIndex() == restored.GetDogsIndex());
                CHECK(session.GetLootsIndex() == restored.GetLootsIndex());
                const auto& restored_dogs = restored.GetDogs();
                const auto& stored_dogs = session.GetDogs();
                CHECK(AreEqual(stored_dogs, restored_dogs) == true);
                const auto& restored_loots = restored.GetLoots();
                const auto& stored_loots = session.GetLoots();
                CHECK(AreEqual(stored_loots, restored_loots) == true);
            }
        }
    }
}
SCENARIO_METHOD(Fixture, "Players Serialization") {
    GIVEN("a players") {
        Game game;
        Player player1("player1"s, 0, Map::Id("map1"s));
        Player player2("player2"s, 1, Map::Id("map2"s));
        Player player3("player3"s, 2, Map::Id("map3"s));
        Players players(game);
        Players::DogIdMapIdToPlayer dog_id_mapid_to_player;
        dog_id_mapid_to_player.insert({{0, Map::Id("map1"s)}, player1});
        dog_id_mapid_to_player.insert({{1, Map::Id("map2"s)}, player2});
        dog_id_mapid_to_player.insert({{2, Map::Id("map3"s)}, player3});
        players.SetPlayers(dog_id_mapid_to_player);
        WHEN("players is serialized") {
            {
                serialization::PlayersRepr repr{players};
                output_archive << repr;
            }
            THEN("it can be deserialized") {
                InputArchive input_archive{strm};
                serialization::PlayersRepr repr;
                input_archive >> repr;
                const auto restored_players = repr.Restore();
                Players restored_players_class(game);
                restored_players_class.SetPlayers(restored_players);
                //auto restored_players = restored.GetPlayers();
                CHECK(restored_players.size() == 3);
                CHECK(restored_players.count({0, Map::Id("map1"s)}) == 1);
                CHECK(restored_players.count({1, Map::Id("map2"s)}) == 1);
                CHECK(restored_players.count({2, Map::Id("map3"s)}) == 1);
                auto restored_player1 = restored_players.at({0, Map::Id("map1"s)});
                CHECK(player1.GetMapId() == restored_player1.GetMapId());
                CHECK(player1.GetName() == restored_player1.GetName());
                CHECK(player1.GetPlayerId() == restored_player1.GetPlayerId());
                auto restored_player2 = restored_players.at({1, Map::Id("map2"s)});
                CHECK(player2.GetMapId() == restored_player2.GetMapId());
                CHECK(player2.GetName() == restored_player2.GetName());
                CHECK(player2.GetPlayerId() == restored_player2.GetPlayerId());
                auto restored_player3 = restored_players.at({2, Map::Id("map3"s)});
                CHECK(player3.GetMapId() == restored_player3.GetMapId());
                CHECK(player3.GetName() == restored_player3.GetName());
                CHECK(player3.GetPlayerId() == restored_player3.GetPlayerId());
            }
        }
    }
}
SCENARIO_METHOD(Fixture, "PlayerTokens Serialization") {
    GIVEN("a players and their tokens") {
        Game game;
        Player player1("player1"s, 0, Map::Id("map1"s));
        Player player2("player2"s, 1, Map::Id("map2"s));
        Player player3("player3"s, 2, Map::Id("map3"s));
        Players players(game);
        Players::DogIdMapIdToPlayer dog_id_mapid_to_player;
        dog_id_mapid_to_player.insert({{0, Map::Id("map1"s)}, player1});
        dog_id_mapid_to_player.insert({{1, Map::Id("map2"s)}, player2});
        dog_id_mapid_to_player.insert({{2, Map::Id("map3"s)}, player3});
        players.SetPlayers(dog_id_mapid_to_player);
        app::PlayerTokens::TokenToPlayerPtr token_to_player_ptr;
        token_to_player_ptr.insert({app::Token("3ce09cee8194cb91787bb6a9333c9b2f"), players.FindByPlayerIdAndMapId(0, Map::Id("map1"s))});
        token_to_player_ptr.insert({app::Token("c8f7cd8856a07951c99b92128d4309ed"), players.FindByPlayerIdAndMapId(1, Map::Id("map2"s))});
        token_to_player_ptr.insert({app::Token("8507b9c5dc5b58fdc7bc01b6bceb463e"), players.FindByPlayerIdAndMapId(2, Map::Id("map3"s))});
        PlayerTokens players_tokens;
        players_tokens.SetTokens(token_to_player_ptr);
        WHEN("players tokens is serialized") {
            {
                serialization::PlayerTokensRepr repr{players_tokens};
                output_archive << repr;
            }
            THEN("it can be deserialized") {
                InputArchive input_archive{strm};
                serialization::PlayerTokensRepr repr;
                input_archive >> repr;
                PlayerTokens restored_players_tokens;
                restored_players_tokens.SetTokens(repr.Restore(players));
                auto restored_tokens_map = restored_players_tokens.GetTokens();
                CHECK(restored_tokens_map.count(app::Token("3ce09cee8194cb91787bb6a9333c9b2f")) == 1);
                CHECK(restored_tokens_map.count(app::Token("c8f7cd8856a07951c99b92128d4309ed")) == 1);
                CHECK(restored_tokens_map.count(app::Token("8507b9c5dc5b58fdc7bc01b6bceb463e")) == 1);
                CHECK(restored_players_tokens.FindPlayerByToken(app::Token("3ce09cee8194cb91787bb6a9333c9b2f")) == 
                      players_tokens.FindPlayerByToken(app::Token("3ce09cee8194cb91787bb6a9333c9b2f")));
                CHECK(restored_players_tokens.FindPlayerByToken(app::Token("c8f7cd8856a07951c99b92128d4309ed")) == 
                      players_tokens.FindPlayerByToken(app::Token("c8f7cd8856a07951c99b92128d4309ed")));
                CHECK(restored_players_tokens.FindPlayerByToken(app::Token("8507b9c5dc5b58fdc7bc01b6bceb463e")) == 
                      players_tokens.FindPlayerByToken(app::Token("8507b9c5dc5b58fdc7bc01b6bceb463e")));
            }
        }
    }
}
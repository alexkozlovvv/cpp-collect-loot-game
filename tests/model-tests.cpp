#include <catch2/catch_test_macros.hpp>

#include "../src/loot_generator.h"
#include "../src/model.h"
#include "../src/tagged.h"

using namespace std::literals;
using namespace model;
using namespace util;
using loot_gen::LootGenerator;
using TimeInterval = LootGenerator::TimeInterval;

bool LootIsOnTheTestMapRoad(const Map* map, const Loot::coords& loot_coords) {
    const auto& roads = map->GetRoads();
    for(const auto& road : roads) {
        if(road.IsHorizontal()) {
            auto start = road.GetStart().x;
            auto end = road.GetEnd().x;
            if((std::min(start, end) <= loot_coords.x) &&
                (loot_coords.x <= std::max(start, end)) && (loot_coords.y == static_cast<double>(road.GetStart().y))) {
                    return true;
                }
        }
        auto start = road.GetStart().y;
        auto end = road.GetEnd().y;
        if((std::min(start, end) <= loot_coords.y) &&
                (loot_coords.x <= std::max(start, end)) && (loot_coords.x == static_cast<double>(road.GetStart().x))) {
                    return true;
                }
    }
    return false;
}

SCENARIO("Add loot on map") {

    TimeInterval generator_base_iinterval{1000};
    double probability{0.5};


    GIVEN("a map with roads and game_session") {
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

        lootGeneratorConfig loot_conf{generator_base_iinterval, probability};
        GameSession TestSession(&test_map, false, loot_conf);
        
        constexpr TimeInterval TIME_INTERVAL = 10s;

        WHEN("session has no looters") {
            THEN("no loot is generated") {
                TestSession.AddLoot(TIME_INTERVAL);
                CHECK(TestSession.GetLoots().empty() == true);
            }
        }
        WHEN("session has looters") {
            TestSession.AddDog("Scooby Doo"s);
            TestSession.AddDog("Muhtar"s);
            TestSession.AddDog("Hatiko"s);
            TestSession.AddLoot(TIME_INTERVAL);
            const auto& generated_loots = TestSession.GetLoots();
            THEN("loot is generated") {
                CHECK((!generated_loots.empty() && generated_loots.size() <= 3));
            } 
            THEN("Generated loot is on the test_map") {
                for(const auto& loot_map_item : generated_loots) {
                    CHECK(LootIsOnTheTestMapRoad(&test_map, loot_map_item.second.GetCoords()) == true);
                }
            }
            THEN("Generated loot has a valid type") {
                for(const auto& loot_map_item : generated_loots) {
                    CHECK(((loot_map_item.second.GetLootType() >= 0) &&
                          (loot_map_item.second.GetLootType() <= 5)) == true);  
                }
            }
        }
    }
}
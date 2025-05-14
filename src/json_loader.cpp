#include "json_loader.h"
#include  <boost/json.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include  <fstream>
#include  <iterator>
#include <iostream>

namespace json_loader {

namespace json = boost::json;

void ParseRoads(model::Map& map, const boost::property_tree::ptree& json_roads) {
    for (const auto &json_road : json_roads) {
        int x0 = json_road.second.get<int>("x0");
        int y0 = json_road.second.get<int>("y0");
        if (boost::optional<int> x1 = json_road.second.get_optional<int>("x1")) {
            model::Road road(model::Road::HORIZONTAL, {x0, y0}, *x1);
            map.AddRoad(road);                                       
        } else {
            int y1 = json_road.second.get<int>("y1");
            model::Road road(model::Road::VERTICAL, {x0, y0}, y1);
            map.AddRoad(road); 
        }
    }
}

void ParseBuildings(model::Map& map, const boost::property_tree::ptree& json_buildings) {
    for (auto &json_building : json_buildings) {
        int x = json_building.second.get<int>("x");
        int y = json_building.second.get<int>("y");
        int w = json_building.second.get<int>("w");
        int h = json_building.second.get<int>("h");
        model::Building building({{x, y}, {w, h}});
        map.AddBuilding(building);
    }
}

void ParseOffices(model::Map& map, const boost::property_tree::ptree& json_offices) {
    for (auto &json_office : json_offices) {
        model::Office::Id office_id(std::move(json_office.second.get<std::string>("id")));
        int x = json_office.second.get<int>("x");
        int y = json_office.second.get<int>("y");
        int off_x = json_office.second.get<int>("offsetX");
        int off_y = json_office.second.get<int>("offsetY");

        model::Office office(office_id, {x, y}, {off_x, off_y});
        map.AddOffice(office);
    }
}

model::Game LoadGame(const std::filesystem::path& json_path) {

    // Загрузить содержимое файла json_path
    // Распарсить строку как JSON
    // Загрузить модель игры из файла

    boost::property_tree::ptree pt;
    model::Game game;
    boost::optional<double> default_speed;
    boost::optional<int> default_bag_capacity;
    boost::optional<double> dog_retirement_time;

    std::ifstream file(json_path);
    if (file.is_open()) {
        boost::property_tree::read_json(file, pt);
        if(default_speed = pt.get_optional<double>("defaultDogSpeed")) {
            game.SetDefaultSpeed(*default_speed);
        }
        if(default_bag_capacity = pt.get_optional<int>("defaultBagCapacity")) {
            game.SetDefaultBagCapacity(*default_bag_capacity);
        }
        if (pt.count("lootGeneratorConfig") > 0) {
            boost::property_tree::ptree lootGeneratorConfig = pt.get_child("lootGeneratorConfig"); 
            std::chrono::milliseconds period = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<double>(lootGeneratorConfig.get<double>("period")));
            //double period = lootGeneratorConfig.get<double>("period");
            double probability = lootGeneratorConfig.get<double>("probability");
            game.SetLootGeneratorConfig(period, probability);
        }
        if(dog_retirement_time = pt.get_optional<double>("dogRetirementTime")) {
            auto dog_retirement_time_milsec = static_cast<uint64_t>(*dog_retirement_time * 1000);
            game.SetDogRetirementTime(std::chrono::milliseconds(dog_retirement_time_milsec));
        }
        boost::property_tree::ptree json_maps = pt.get_child("maps");

        for (auto &json_map : json_maps) {
            // Получение данных о карте
            boost::optional<double> dog_speed;
            boost::optional<int> map_bag_capacity;
            model::Map::Id mapId(std::move(json_map.second.get<std::string>("id")));
            std::string mapName = json_map.second.get<std::string>("name");
            model::Map map(mapId, mapName);

            if(dog_speed = json_map.second.get_optional<double>("dogSpeed")) {
                map.SetDogSpeed(*dog_speed);
            } else {
                map.SetDogSpeed(game.GetDefaultSpeed());
            }
            if(map_bag_capacity = json_map.second.get_optional<int>("bagCapacity")) {
                map.SetDogBagCapacity(*map_bag_capacity);
            } else {
                map.SetDogBagCapacity(game.GetDefaultBagCapacity());
            }
            const auto json_loot_types = json_map.second.get_child("lootTypes");
            map.SetLootTypeCount(json_loot_types.size());
            int i = 0;
            for(auto& json_loot : json_loot_types) {
                auto value = json_loot.second.get<int>("value");
                map.SetLootTypeValue(i, value);
            }
            //extra_data.AddMapLootData(*mapId, json_loot_types);

            /* Парсинг и добавление дорог */
            const auto json_roads = json_map.second.get_child("roads");
            ParseRoads(map, json_roads);
        
            /* Парсинг и добавление зданий */
            const auto json_buildings = json_map.second.get_child("buildings");
            ParseBuildings(map, json_buildings);

            /* Парсинг и добавление бюро находок */
            const auto json_offices = json_map.second.get_child("offices");
            ParseOffices(map, json_offices);

            game.AddMap(map);
        }
    } else {
        throw std::runtime_error("Ошибка открытия файла: " + json_path.string());
    }
    return game;
}

}  // namespace json_loader

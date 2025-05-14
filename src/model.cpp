#include "model.h"

#include <stdexcept>

namespace model {
using namespace std::literals;

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        // Удаляем офис из вектора, если не удалось вставить в unordered_map
        offices_.pop_back();
        throw;
    }
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

bool operator<(const limit_of_road& lhs, const limit_of_road& rhs) {
    // Сначала сравниваем понижнему краю (down_y_)
    if(lhs.down_y_ < rhs.down_y_) {
        return true;
    } else if (lhs.down_y_ > rhs.down_y_) {
        return false;
    }
    // Сравниваем по левому краю 
    if (lhs.left_x_ < rhs.left_x_) {
        return true;
    } else if (lhs.left_x_ > rhs.left_x_) {
        return false;
    }
    return false;
}
bool operator<(const limit_of_road& lhs, const coords& rhs) {
    // Сначала сравниваем по левой границе (left_x_)
    if (lhs.down_y_ < rhs.y) {
        return true;
    } else if (lhs.right_x_ < rhs.x) {
        return true;
    }
    return false;
}
bool operator<(const coords& lhs, const limit_of_road& rhs) {
    // Сначала сравниваем по левой границе (left_x_)
    if (lhs.y < rhs.up_y_) {
        return true;
    } else if (lhs.x < rhs.left_x_) {
        return true;
    }
    return false;
}

double roundToOneDecimal(double value) {
    return std::round(value * 10.0) / 10.0;
}

void Map::AddRoad(const Road& road) {
    roads_.emplace_back(road);
    if(road.IsHorizontal()) {
        try{
        if(road.GetStart().x < road.GetEnd().x) {
            hor_roads_.insert({{roundToOneDecimal(road.GetStart().x - 0.4), roundToOneDecimal(road.GetEnd().x + 0.4),
                                roundToOneDecimal(road.GetStart().y - 0.4), roundToOneDecimal(road.GetStart().y + 0.4)},
                road});
        } else if(road.GetStart().x > road.GetEnd().x) {
            hor_roads_.insert({{roundToOneDecimal(road.GetEnd().x - 0.4), roundToOneDecimal(road.GetStart().x + 0.4),
                roundToOneDecimal(road.GetStart().y - 0.4), roundToOneDecimal(road.GetStart().y + 0.4)},
                road});
        }
            auto start_coord_y = static_cast<double>(road.GetStart().y);
            auto start_coord_x = static_cast<double>(road.GetStart().x);
            coords start_coords(start_coord_x, start_coord_y);
            auto& finded_road = (*(hor_roads_.find(start_coords))).second;
            int q = 1;
        } catch (const std::out_of_range& e) {
            std::cout << "Ошибка: ключ не найден." << std::endl;
        }
    } else if (road.IsVertical()){
        if(road.GetStart().y < road.GetEnd().y) {
            vert_roads_.insert({{roundToOneDecimal(road.GetStart().x - 0.4), roundToOneDecimal(road.GetStart().x + 0.4),
                                    roundToOneDecimal(road.GetStart().y - 0.4), roundToOneDecimal(road.GetEnd().y + 0.4)},
                road});
        } else if (road.GetStart().y > road.GetEnd().y) {
            vert_roads_.insert({{roundToOneDecimal(road.GetStart().x - 0.4), roundToOneDecimal(road.GetStart().x + 0.4),
                                    roundToOneDecimal(road.GetEnd().y - 0.4), roundToOneDecimal(road.GetStart().y + 0.4)},
                road});
        }

    }
}

const Road* Map::FindHorRoad(double x_coord, double y_coord) const {
    coords start_coords(x_coord, y_coord);
    for(auto& road_items : hor_roads_) {
        if(road_items.first.IsInside(start_coords)) {
            return &road_items.second;
        }
    }
    return nullptr;
}

const Road* Map::FindVertRoad(double x_coord, double y_coord) const {
    coords start_coords(x_coord, y_coord);
    for(auto& road_items : vert_roads_) {
        if(road_items.first.IsInside(start_coords)) {
            return &road_items.second;
        }
    }
    return nullptr;
}

void Map::AddBuilding(const Building& building) {
    buildings_.emplace_back(building);
}

void Dog::SetSpeed(std::string move_direction, double map_dog_speed) {
    if(move_direction == "L"s) {
        speed_ = {-map_dog_speed, 0.0};
        direction_ = {Direction::WEST};
    } else if(move_direction == "R"s) {
        speed_ = {map_dog_speed, 0.0};
        direction_ = {Direction::EAST};
    } else if(move_direction == "U"s) {
        speed_ = {0.0, -map_dog_speed};
        direction_ = {Direction::NORTH};
    } else if(move_direction == "D"s) {
        speed_ = {0.0, map_dog_speed};
        direction_ = {Direction::SOUTH};
    } else {
        speed_ = {0.0, 0.0};
    }
}

std::uint64_t GameSession::AddDog(std::string dog_name) {
    // Получаем случайное значение начальной координаты нового пса
    Dog::coords start_coords;
    if(is_randomize_) {
        std::random_device random_device_;
        std::mt19937_64 generator(random_device_());
        const auto& map_roads = map_->GetRoads();
        std::uniform_int_distribution<int> distribution(0, map_roads.size() - 1);
        int road_number = distribution(generator);
        const auto& road = map_roads.at(road_number);
        if(road.IsHorizontal()) {
            start_coords.y = static_cast<double>(road.GetStart().y);
            std::uniform_real_distribution<double> distribution1(road.GetStart().x, road.GetEnd().x);
            start_coords.x = distribution1(generator);
        } else if (road.IsVertical()){
            start_coords.x = static_cast<double>(road.GetStart().x);
            std::uniform_real_distribution<double> distribution1(road.GetStart().y, road.GetEnd().y);
            start_coords.y = distribution1(generator);
        }
    } else {
        const auto& map_roads = map_->GetRoads();
        const auto& road = map_roads.at(0);
        start_coords.x = static_cast<double>(road.GetStart().x);
        start_coords.y = static_cast<double>(road.GetStart().y);
    }

    auto created_dog = Dog(dog_name, start_coords);
    auto last_index = dog_index_;
    // формируем идентификатор
    dogs_.insert({dog_index_++, created_dog});
    return last_index;
}

void GameSession::AddLoot (loot_gen::LootGenerator::TimeInterval time_delta) {
    auto new_loot_count = loot_generator_.Generate(time_delta, loots_.size(), dogs_.size());
    for(int i = 0; i < new_loot_count; i++) {
        Loot::coords coords;
        int new_loot_type;
        if(is_randomize_) {
            std::random_device random_device_;
            std::mt19937_64 generator(random_device_());
            const auto& map_roads = map_->GetRoads();
            std::uniform_int_distribution<int> distribution(0, map_roads.size() - 1);
            int road_number = distribution(generator);
            const auto& road = map_roads.at(road_number);
            if(road.IsHorizontal()) {
                coords.y = static_cast<double>(road.GetStart().y);
                std::uniform_real_distribution<double> distribution1(road.GetStart().x, road.GetEnd().x);
                coords.x = distribution1(generator);
            } else if (road.IsVertical()){
                coords.x = static_cast<double>(road.GetStart().x);
                std::uniform_real_distribution<double> distribution1(road.GetStart().y, road.GetEnd().y);
                coords.y = distribution1(generator);
            }
            std::uniform_int_distribution<int> distribution2(0, std::max(0, map_->GetLootTypeCount() - 1));
            new_loot_type = distribution2(generator);
        } else {
            coords = {1.0, 0.0};
            new_loot_type = 0;
        }
        auto created_loot = Loot(new_loot_type, coords);
        loots_.insert({loot_index_++, created_loot});
    }
    //loot_count_ += new_loot_count;
}

void GameSession::HandleEvents(const std::vector<collision_detector::Gatherer>& gatherers) {
    //auto& loots = GetLoots();
    const auto& offices = map_->GetOffices();
    const auto start_offices_index = loots_.size();
    const auto end_offices_index = start_offices_index + (offices.size() - 1);
    std::vector<collision_detector::Item> items;
    std::vector<int> indexes_of_collected_loot;
    for(const auto& loot_items : loots_) {
        collision_detector::Item item({loot_items.second.GetCoords().x, loot_items.second.GetCoords().y},0);
        items.push_back(item);
    }
    for(const auto& office : offices) {
        collision_detector::Item item({static_cast<double>(office.GetPosition().x), static_cast<double>(office.GetPosition().y)}, 0.25);
        items.push_back(item);
    }
    collision_detector::VectorItemGathererProvider provider(items, gatherers);
    auto events = collision_detector::FindGatherEvents(provider);
    for(const auto& event : events) {
        if((event.item_id < start_offices_index) && 
            (std::find(indexes_of_collected_loot.begin(), indexes_of_collected_loot.end(), event.item_id) == indexes_of_collected_loot.end())) {
            //auto dog_ptr = FindDog(event.gatherer_id);
            auto it = dogs_.begin();
            std::advance(it, event.gatherer_id);
            auto dog_ptr = &((*it).second);
            if(dog_ptr->GetBag().size() < 3) {
                int i = 0;
                std::uint64_t key_of_deleted_loot = 0;
                for(const auto& loot_items : loots_) {
                    if(i == event.item_id) {
                        dog_ptr->PutLootInTheBag(loot_items.first, loot_items.second);
                        indexes_of_collected_loot.push_back(event.item_id);
                        key_of_deleted_loot = loot_items.first;
                        break;
                    }
                    i++;
                } 
                loots_.erase(key_of_deleted_loot);
            }
        } else if ( (start_offices_index <= event.item_id) && (event.item_id <= end_offices_index) ) {
            //auto dog_ptr = FindDog(event.gatherer_id);
            auto it = dogs_.begin();
            std::advance(it, event.gatherer_id);
            auto dog_ptr = &((*it).second);
            const auto dog_bag = dog_ptr->GetBag();
            dog_ptr->ExtractAllLoot();
            if(!dog_bag.empty()) {
                int value = 0;
                for(const auto& bag_items : dog_bag) {
                    value += map_->GetLootTypeValue(bag_items.second);
                }
                dog_ptr->AddScore(value);
            }
        }
    }
}

}  // namespace model

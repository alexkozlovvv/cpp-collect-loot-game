#pragma once
#include <string>
#include <random>
#include <unordered_map>
#include <vector>
#include <map>
#include <iterator>
#include <iostream>

#include "tagged.h"
#include "loot_generator.h"
#include "collision_detector.h"

namespace model {
using namespace std::literals;

using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

class Road {
    struct HorizontalTag {
        HorizontalTag() = default;
    };

    struct VerticalTag {
        VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {
    }

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y} {
    }

    bool IsHorizontal() const noexcept {
        return start_.y == end_.y;
    }

    bool IsVertical() const noexcept {
        return start_.x == end_.x;
    }

    Point GetStart() const noexcept {
        return start_;
    }

    Point GetEnd() const noexcept {
        return end_;
    }

private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    const Rectangle& GetBounds() const noexcept {
        return bounds_;
    }

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    Point GetPosition() const noexcept {
        return position_;
    }

    Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    Point position_;
    Offset offset_;
};

struct coords {
    double x;
    double y;
};

struct limit_of_road {
    
    bool operator==(limit_of_road rhs) const {
        return left_x_ == rhs.left_x_ && 
               right_x_ == rhs.right_x_ &&
               up_y_ == rhs.up_y_ &&
               down_y_ == rhs.down_y_;
    }
    bool IsInside (coords dog_coords) const {
        return (dog_coords.x <= right_x_) && (dog_coords.x >= left_x_) && 
               (dog_coords.y >= up_y_) && (dog_coords.y <= down_y_);
    }

    double left_x_;
    double right_x_;
    double up_y_;
    double down_y_;
};

bool operator<(const limit_of_road& lhs, const limit_of_road& rhs);
bool operator<(const limit_of_road& lhs, const coords& rhs);
bool operator<(const coords& lhs, const limit_of_road& rhs);

double roundToOneDecimal(double value);

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name) noexcept
        : id_(std::move(id))
        , name_(std::move(name)) {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    const Buildings& GetBuildings() const noexcept {
        return buildings_;
    }

    const Roads& GetRoads() const noexcept {
        return roads_;
    }

    const Offices& GetOffices() const noexcept {
        return offices_;
    }

    void SetDogSpeed(double dog_speed) {
        dog_speed_ = dog_speed;
    }

    void SetDogBagCapacity(int bag_capacity) {
        bag_capacity_ = bag_capacity;
    }

    void SetLootTypeCount(size_t loot_types_count){
        loot_types_count_ = loot_types_count;
    }

    void SetLootTypeValue(int type_id, int value) {
        loot_type_to_value.insert({type_id, value});
    }
    
    const double GetDogSpeed() const {
        return dog_speed_;
    }

    int GetLootTypeValue(int type_id) const {
        return loot_type_to_value.at(type_id);
    }

    const int GetDogBagCapacity() const {
        return bag_capacity_;
    }
    
    int GetLootTypeCount() const {
        return loot_types_count_;
    }

    void AddRoad(const Road& road);

    const Road* FindHorRoad(double x_coord, double y_coord) const;

    const Road* FindVertRoad(double x_coord, double y_coord) const;

    void AddBuilding(const Building& building);

    void AddOffice(Office office);

    struct DoubleHash {
        std::size_t operator()(const double& value) const {
            return std::hash<double>()(value);
        }
    };
    struct CoordEqual {
        bool operator()(const double& lhs, const double& rhs) const {
            bool res = (((lhs + 0.4) >= rhs) && ((lhs - 0.4) <= rhs));
            return /*(lhs == rhs) ||*/ res;
        }
    };

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    int loot_types_count_;
    std::map<int, int> loot_type_to_value;
    double dog_speed_;
    int bag_capacity_;
    Roads roads_;
    Buildings buildings_;
    std::map<limit_of_road, Road, std::less<>> hor_roads_;
    std::map<limit_of_road, Road, std::less<>> vert_roads_;
    
    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
};

class Loot {
public:
    struct coords {
        double x;
        double y;

        constexpr auto operator<=>(const coords&) const = default;
    };
    Loot(int loot_type, coords coords) : loot_type_(loot_type), coords_(coords) {}

    int GetLootType() const {
        return loot_type_;
    }

    const coords& GetCoords() const {
        return coords_;
    }

    bool operator==(const Loot& rhs) const {
        return loot_type_ == rhs.loot_type_ && coords_ == rhs.coords_;
    }

private:
    int loot_type_ = 0;
    coords coords_;
};

class Dog {
public:  
    using IndexToLootType = std::map<std::uint64_t, std::uint64_t>;
    struct coords {
        double x;
        double y;

        constexpr auto operator<=>(const coords&) const = default;
    };
    struct speed {
        double h_s;
        double v_s;

        constexpr auto operator<=>(const speed&) const = default;
    };

    enum class Direction {
        NORTH,
        SOUTH,
        WEST,
        EAST
    };

    explicit Dog(std::string name, coords spawn_point) : name_(name), coords_(spawn_point) {}

    std::string GetName() const {
        return name_;
    }

    const coords& GetCoords() const {
        return coords_;
    }

    const speed& GetSpeed() const {
        return speed_;
    }

    const Direction& GetDirection() const {
        return direction_;
    }

    int GetScore() const {
        return score_;
    }

    void PutLootInTheBag(int index, Loot loot){
        bag_.insert({index, loot.GetLootType()});
    }

    const IndexToLootType& GetBag() const {
        return bag_;
    }

    void AddScore(int score) {
        score_ += score;
    }

    void AddInGameTime(std::chrono::milliseconds time) {
        in_game_time_ += time;
    }

    void AddStandByTime(std::chrono::milliseconds time) {
        standby_time_ += time;
    }

    void SetStandByTime(std::chrono::milliseconds time) {
        standby_time_ = time;
    }

    std::chrono::milliseconds GetInGameTime() const {
        return in_game_time_;
    }

    std::chrono::milliseconds GetStandByTime() const {
        return standby_time_;
    }

    void ExtractAllLoot() {
        return bag_.clear();
    }

    void SetCoords(coords coords_var) {
        coords_ = {coords_var.x, coords_var.y};
    }

    void SetSpeed(std::string move_direction, double map_dog_speed);

    void SetSpeedOnly(speed speed){
        speed_ = speed;
    }

    void SetDirection(Direction direction) {
        direction_ = direction;
    }

    bool operator==(const Dog& rhs) const {
        return name_ == rhs.name_ && coords_ == rhs.coords_ &&
               speed_ == rhs.speed_ && direction_ == rhs.direction_ &&
               bag_ == rhs.bag_ && score_ == rhs.score_;
    }


private:
    std::string name_;
    coords coords_;
    speed speed_{0.0, 0.0};
    Direction direction_{Direction::NORTH};
    IndexToLootType bag_;
    int score_ = 0;
    std::chrono::milliseconds standby_time_{0};
    std::chrono::milliseconds in_game_time_{0};
};

struct lootGeneratorConfig {
    loot_gen::LootGenerator::TimeInterval period;
    double probability;
};

class GameSession {
public:
    using IndexToDog = std::map<std::uint64_t, Dog>;
    using IndexToLoot = std::map<std::uint64_t, Loot>;
    
    //GameSession() = default;
    explicit GameSession(const Map* map, bool is_randomize, lootGeneratorConfig loot_cong) : map_(map), is_randomize_(is_randomize), 
                                                                                             loot_generator_(loot_cong.period, loot_cong.probability) {}

    // Отношение композиции. Создаем собаку внутри сессии. Передаем имя
    std::uint64_t AddDog(std::string dog_name);
    void AddLoot (loot_gen::LootGenerator::TimeInterval time_delta);
    void HandleEvents(const std::vector<collision_detector::Gatherer>& gatherers);

    Map::Id GetIDMap() const {
        return (*map_).GetId();
    }

    const Map* GetMapPtr() const {
        return map_;
    }

    size_t GetDogsCount() const {
        return dogs_.size();
    }

    std::uint64_t GetDogsIndex() const {
        return dog_index_;
    }

    std::uint64_t GetLootsIndex() const {
        return loot_index_;
    }

    IndexToDog& GetDogs() {
        return dogs_;
    }

    const IndexToDog& GetDogs() const {
        return dogs_;
    }

    IndexToLoot& GetLoots() {
        return loots_;
    }

    const IndexToLoot& GetLoots() const {
        return loots_;
    }

    bool GetSessionRandomize() {
        return is_randomize_;
    }

    Dog* FindDog(std::uint64_t dog_index) {
        return &dogs_.at(dog_index);
    }

    void SetDogsIndex(std::uint64_t dog_index) {
        dog_index_ = dog_index;
    }

    void SetLootsIndex(std::uint64_t loot_index) {
        loot_index_ = loot_index;
    }

    void SetLoots(const IndexToLoot& loots) {
        loots_ = loots;
    }

    void SetDogs(const IndexToDog& dogs) {
        dogs_ = dogs;
    }

private:
    const Map* map_;
    bool is_randomize_;
    loot_gen::LootGenerator loot_generator_;
    IndexToDog dogs_;
    IndexToLoot loots_;
    std::uint64_t dog_index_ = 0;
    std::uint64_t loot_index_ = 0;
};

class Game {
public:
    using Maps = std::vector<Map>;
    using MapIdToSession = std::map<Map::Id, GameSession>;

    explicit Game(double default_speed = 1.0, int default_bag_capacity = 3) : default_speed_(default_speed), 
                                                                              default_bag_capacity_(default_bag_capacity) {}

    void AddMap(Map map);

    void AddSession(GameSession session) {
        sessions_.insert({session.GetIDMap(), session});
    }

    void SetDefaultSpeed(double default_speed) {
        default_speed_ = default_speed;
    }

    void SetDefaultBagCapacity(double default_bag_capacity) {
        default_bag_capacity_ = default_bag_capacity;
    }

    void SetRandomize(){
        is_randomize_ = true;
    }
    void SetAutoTick(){
        is_auto_tick_ = true;
    }
    void SetLootGeneratorConfig(std::chrono::milliseconds period, double probability) {
        loot_generator_config_ = {period, probability};
    }
    void SetDogRetirementTime(std::chrono::milliseconds dog_retirement_time) {
        dog_retirement_time_ = dog_retirement_time;
    }
    void SetSessions(const MapIdToSession& mapid_to_sessions){
        sessions_ = mapid_to_sessions;
    }
    bool IsGameAuto() const {
        return is_auto_tick_;
    }
    bool GetRandomize() const {
        return is_randomize_;
    }

    double GetDefaultSpeed() const {
        return default_speed_;
    }

    std::chrono::milliseconds GetDogRetireTime() const {
        return dog_retirement_time_; 
    }

    int GetDefaultBagCapacity() const {
        return default_bag_capacity_;
    }

    lootGeneratorConfig GetLootGeneratorConfig() const {
        return loot_generator_config_;
    }

    const Maps& GetMaps() const noexcept {
        return maps_;
    }

    const Map* FindMap(const Map::Id& id) const noexcept {
        if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
            return &maps_.at(it->second);
        }
        return nullptr;
    }

    GameSession* FindSession(const Map::Id&  map_id) {
        if(sessions_.count(map_id) != 0) {
            return &(sessions_.at(map_id));
        }
        return nullptr;
    }

    MapIdToSession& GetSessions() {
        return sessions_;
    }

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    double default_speed_;
    int default_bag_capacity_;

    std::vector<Map> maps_;
    MapIdToSession sessions_;
    lootGeneratorConfig loot_generator_config_;
    MapIdToIndex map_id_to_index_;
    std::chrono::milliseconds dog_retirement_time_{60000};
    bool is_randomize_ = false;
    bool is_auto_tick_ = false;
};

}  // namespace model

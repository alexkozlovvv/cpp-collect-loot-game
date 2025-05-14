#pragma once
// boost.beast будет использовать std::string_view вместо boost::string_view
#define BOOST_BEAST_USE_STD_STRING_VIEW
#include  <boost/json.hpp>
#include <filesystem>
#include  <fstream>


namespace extra {

using namespace boost::json;

class ExtraData {
public:
    explicit ExtraData(const std::filesystem::path& json_path) {
        std::ifstream file(json_path);
        std::string input(std::istreambuf_iterator<char>(file), {});

        if (file.is_open()) {
             // 2. Распарсите JSON:
            value json_value = parse(input);

            for (const auto& map_object : json_value.at("maps").as_array()) {
                // 4.1 Извлеките map_id:
                //std::string mapId = map_object.at("id").as_string();
                front_end_data[map_object.at("id").as_string()] = map_object.at("lootTypes").as_array();
            }

            //std::cout << front_end_data << std::endl; 

        } else {
            throw std::runtime_error("Ошибка открытия файла: " + json_path.string());
        }
    }

    const boost::json::object& GetData() const {
        return front_end_data;
    }

    boost::json::object front_end_data;
};

}


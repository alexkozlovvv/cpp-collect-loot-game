#include "request_handler.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <vector>
#include <boost/json.hpp>
#include <string>


namespace json = boost::json;

namespace http_handler {

    using namespace std::literals;

    Response MakeValidMapsResponse(http::verb method, http::status status, unsigned http_version, const model::Game::Maps& maps = {},
                                const model::Map* map_ptr = nullptr, const boost::json::object& extra_data = {}) {

        StringResponse response(status, http_version);
        std::string json_string;
        boost::json::object obj;

        response.set(http::field::content_type, ContentType::JSON);
        if(map_ptr == nullptr) {
            // формирование тела ответа на запрос списка карт
            boost::json::array arr;
            for(const auto& map : maps) {
                obj["id"] = *(map.GetId());
                obj["name"] = map.GetName();
                arr.push_back(obj);
            }
            json_string = boost::json::serialize(arr);
        } else {
            // формирование тела ответа на запрос описания карты
            std::string map_id = *(map_ptr->GetId());
            obj["id"] = map_id;
            obj["name"] = map_ptr->GetName();

            boost::json::array roads_arr;
            for(const auto& road : map_ptr->GetRoads()) { 
                // Добавление данных о дорогах
                if(road.IsHorizontal()){
                    roads_arr.push_back({{"x0", road.GetStart().x}, {"y0", road.GetStart().y}, {"x1", road.GetEnd().x}});
                } else {
                    roads_arr.push_back({{"x0", road.GetStart().x}, {"y0", road.GetStart().y}, {"y1", road.GetEnd().y}});
                }
            }
            obj["roads"] = roads_arr;

            boost::json::array buildings_arr;
            for(const auto& building : map_ptr->GetBuildings()) {
                // Добавление данных о зданиях
                auto bounds = building.GetBounds();
                buildings_arr.push_back({{"x", bounds.position.x}, {"y", bounds.position.y}, 
                                            {"w", bounds.size.width}, {"h", bounds.size.height}});
            }
            obj["buildings"] = buildings_arr;

            boost::json::array offices_arr;
            for(const auto& office : map_ptr->GetOffices()) {
                // Добавление данных об офисах
                offices_arr.push_back({{"id", *(office.GetId())}, {"x", office.GetPosition().x}, 
                                        {"y", office.GetPosition().y}, {"offsetX", office.GetOffset().dx}, 
                                        {"offsetY", office.GetOffset().dy}});
            }
            obj["offices"] = offices_arr;
            obj["lootTypes"] = extra_data.at(map_id);

            json_string = boost::json::serialize(obj);
        }

        if(method == http::verb::get) {
            response.body() = json_string;
        }
        response.content_length(json_string.size());
        response.set(http::field::cache_control, "no-cache");
        return response;
    }

    Response MakeInValidMapsResponse(http::status status, unsigned http_version,
                                                           std::string_view content_type = ContentType::JSON) {

        StringResponse response(status, http_version);
        std::string json_string;
        boost::json::object obj;

        response.set(http::field::content_type, content_type);

        if(status == http::status::not_found) {
            obj["code"] = "mapNotFound";
            obj["message"] = "Map not found";
            json_string = boost::json::serialize(obj);

        } else if(status == http::status::bad_request) {
            obj["code"] = "badRequest";
            obj["message"] = "Bad request";
            json_string = boost::json::serialize(obj);
        } else if (status == http::status::method_not_allowed) {
            response.set(http::field::allow, "GET, HEAD");
            obj["code"] = "invalidMethod";
            obj["message"] = "Only GET method is expected";
            json_string = boost::json::serialize(obj);
        }
        response.body() = json_string;
        response.content_length(json_string.size());
        response.set(http::field::cache_control, "no-cache");
        return response;
    }

    Response MakeInvalidStringResponse(http::status status, unsigned http_version,
                                    std::string_view content_type = ContentType::TEXT_HTML) {
        StringResponse response(status, http_version);
        response.set(http::field::content_type, content_type);
        response.set(http::field::allow, "GET"sv);
        std::string error_str = "Invalid method"s;
        response.body() = error_str;
        response.content_length(error_str.size());
        return response;
    }

    std::string_view GetContentType(const std::string path) {
        auto pos = path.find_last_of('.');
        if(pos == path.npos || pos == path.size()){
            return ContentType::UNKNOWN;
        }
        std::string extension = path.substr(pos + 1);
        std::transform(extension.begin(), extension.end(), extension.begin(), 
            [](unsigned char c){ return std::tolower(c); });
        if(extension == "htm"s  || extension ==  "html"s) {
            return ContentType::TEXT_HTML;
        } else if (extension == "css"s) {
            return ContentType::TEXT_CSS;
        } else if (extension == "txt"s) {
            return ContentType::TEXT_PLAIN;
        } else if (extension == "js"s) {
            return ContentType::TEXT_JS;
        } else if (extension == "json"s) {
            return ContentType::JSON;
        } else if (extension == "xml"s) {
            return ContentType::XML;
        } else if (extension == "png"s) {
            return ContentType::PNG;
        } else if (extension == "jpg"s || extension ==  "jpe"s || extension ==  "jpeg"s) {
            return ContentType::JPEG;
        } else if (extension == "gif"s) {
            return ContentType::GIF;
        } else if (extension == "bmp"s) {
            return ContentType::BMP;
        } else if (extension == "ico"s) {
            return ContentType::ICON;
        } else if (extension == "tiff"s || extension ==  "tif"s) {
            return ContentType::TIFF;
        } else if (extension == "svg"s || extension ==  "svgz"s) {
            return ContentType::SVG_XML;
        } else if (extension == "mp3"s) {
            return ContentType::MPEG;
        }
        return ContentType::UNKNOWN;
    }

    Response RequestHandler::MakeValidFileResponse(http::status status, unsigned http_version, const fs::path path) const {

        using namespace http;
        std::string_view content_type = GetContentType(std::string(path));

        response<file_body> res;
        res.version(http_version);
        res.result(status);
        res.insert(field::content_type, content_type);

        file_body::value_type file;

        if (sys::error_code ec; file.open(path.string().c_str(), beast::file_mode::read, ec), ec) {
            std::cout << "Failed to open file "sv << path << std::endl;
            throw std::runtime_error("Ошибка открытия файла: " + std::string(path));
        }

        res.body() = std::move(file);
        // Метод prepare_payload заполняет заголовки Content-Length и Transfer-Encoding
        // в зависимости от свойств тела сообщения
        res.prepare_payload();
        return res;
    }

    Response RequestHandler::MakeInValidFileResponse(http::status status, unsigned http_version,
                                    std::string_view content_type = ContentType::TEXT_PLAIN) const {
        StringResponse response(status, http_version);
        response.set(http::field::content_type, content_type);
        if(status == http::status::not_found) {
            std::string error_str = "Resource not found"s;
            response.body() = error_str;
            response.content_length(error_str.size());
        } else if(status == http::status::bad_request) {
            std::string error_str = "Bad request"s;
            response.body() = error_str;
            response.content_length(error_str.size());
        }
        return response;
    }

    Response MakeValidJoinResponse(http::status status, unsigned http_version, const app::JoinGameResult& result) {
        StringResponse response(status, http_version);
        std::string json_string;
        boost::json::object obj;

        response.set(http::field::content_type, ContentType::JSON);
        obj["authToken"] = *result.player_token;
        obj["playerId"] = *result.player_id;
        json_string = boost::json::serialize(obj);
        response.body() = json_string;
        response.content_length(json_string.size());
        response.set(http::field::cache_control, "no-cache");

        return response;
    }

    Response MakeInValidJoinResponse(http::status status, unsigned http_version, bool parse_er = false) { 
        StringResponse response(status, http_version);
        std::string json_string;
        boost::json::object obj;

        response.set(http::field::content_type, ContentType::JSON);
        if(status == http::status::not_found) {
            obj["code"] = "mapNotFound";
            obj["message"] = "Map not found"s;
        } else if (status == http::status::bad_request) {
            obj["code"] = "invalidArgument";
            if(parse_er) {
                obj["message"] = "Join game request parse error";
            } else {
                obj["message"] = "Invalid name";
            }
        } else if (status == http::status::method_not_allowed) {
            response.set(http::field::allow, "POST");
            obj["code"] = "invalidMethod";
            obj["message"] = "Only POST method is expected";
        }
        json_string = boost::json::serialize(obj);
        response.body() = json_string;
        response.content_length(json_string.size());
        response.set(http::field::cache_control, "no-cache");

        return response;
    }

    Response MakeValidPlayerListResponse(http::verb method, http::status status, unsigned http_version, 
                                         const std::vector<app::Player*>& session_players) {
        StringResponse response(status, http_version);
        std::string json_string;
        boost::json::object obj;

        response.set(http::field::content_type, ContentType::JSON);
        // for(auto i = 0U; i < session_players.size(); i++) {
        //     boost::json::object internal_obj;
        //     internal_obj["name"] = session_players.at(i)->GetName();
        //     obj[std::to_string(i)] = internal_obj;
        // }
        for(const auto& player : session_players) {
            boost::json::object internal_obj;
            internal_obj["name"] = player->GetName();
            obj[std::to_string(*(player->GetPlayerId()))] = internal_obj;
        }

        json_string = boost::json::serialize(obj);
        if(method == http::verb::get) {
            response.body() = json_string;
        }
        response.content_length(json_string.size());
        response.set(http::field::cache_control, "no-cache");

        return response;
    }

    Response MakeInValidPlayerListResponse(http::status status, unsigned http_version, bool is_bad_auth_header = false) {
        StringResponse response(status, http_version);
        std::string json_string;
        boost::json::object obj;

        response.set(http::field::content_type, ContentType::JSON);
        if (status == http::status::unauthorized) {
            if(is_bad_auth_header) {
                obj["code"] = "invalidToken";
                obj["message"] = "Authorization header is missing";
            } else {
                obj["code"] = "unknownToken";
                obj["message"] = "Player token has not been found";
            }
        } else if (status == http::status::method_not_allowed) {
            response.set(http::field::allow, "GET, HEAD");
            obj["code"] = "invalidMethod";
            obj["message"] = "Invalid method";
        }
        json_string = boost::json::serialize(obj);
        response.body() = json_string;
        response.content_length(json_string.size());
        response.set(http::field::cache_control, "no-cache");

        return response;
    }

    double roundToOneDecimal(double value) {
        return std::round(value * 10.0) / 10.0;
    }

    Response MakeValidGetGameStateResponse(http::verb method, http::status status, unsigned http_version, const model::GameSession* session_ptr) {
        StringResponse response(status, http_version);
        std::string json_string;
        boost::json::object obj;

        response.set(http::field::content_type, ContentType::JSON);
        boost::json::object players;
        const auto dogs = session_ptr->GetDogs();
        // for(auto i = 0U; i < dogs.size(); i++) {
        //     boost::json::object internal_obj;
        //     boost::json::array coords_arr;
        //     coords_arr.push_back((dogs.at(i).GetCoords().x));
        //     coords_arr.push_back((dogs.at(i).GetCoords().y));
        //     internal_obj["pos"] = coords_arr;
        //     boost::json::array speed_arr;
        //     speed_arr.push_back((dogs.at(i).GetSpeed().h_s));
        //     speed_arr.push_back((dogs.at(i).GetSpeed().v_s));
        //     internal_obj["speed"] = speed_arr;
        //     const auto& dir = dogs.at(i).GetDirection();
        //     if(dir == model::Dog::Direction::EAST){
        //         internal_obj["dir"] = "R";
        //     } else if (dir == model::Dog::Direction::NORTH) {
        //         internal_obj["dir"] = "U";
        //     } else if (dir == model::Dog::Direction::SOUTH) {
        //         internal_obj["dir"] = "D";
        //     } else if (dir == model::Dog::Direction::WEST) {
        //         internal_obj["dir"] = "L";
        //     }
        //     boost::json::array loots_arr;
        //     const auto& loots = dogs.at(i).GetBag();
        //     for(const auto& loot_items : loots) {
        //         boost::json::object loot_info;
        //         loot_info["id"] = loot_items.first;
        //         loot_info["type"] = loot_items.second;
        //         loots_arr.push_back(loot_info);
        //     }
        //     internal_obj["bag"] = loots_arr;
        //     internal_obj["score"] = dogs.at(i).GetScore();
        //     players[std::to_string(i)] = internal_obj;
        // }
        int i = 0;
        for(const auto& dog : dogs) {
            boost::json::object internal_obj;
            boost::json::array coords_arr;
            coords_arr.push_back((dog.second.GetCoords().x));
            coords_arr.push_back((dog.second.GetCoords().y));
            internal_obj["pos"] = coords_arr;
            boost::json::array speed_arr;
            speed_arr.push_back((dog.second.GetSpeed().h_s));
            speed_arr.push_back((dog.second.GetSpeed().v_s));
            internal_obj["speed"] = speed_arr;
            const auto& dir = dog.second.GetDirection();
            if(dir == model::Dog::Direction::EAST){
                internal_obj["dir"] = "R";
            } else if (dir == model::Dog::Direction::NORTH) {
                internal_obj["dir"] = "U";
            } else if (dir == model::Dog::Direction::SOUTH) {
                internal_obj["dir"] = "D";
            } else if (dir == model::Dog::Direction::WEST) {
                internal_obj["dir"] = "L";
            }
            boost::json::array loots_arr;
            const auto& loots = dog.second.GetBag();
            for(const auto& loot_items : loots) {
                boost::json::object loot_info;
                loot_info["id"] = loot_items.first;
                loot_info["type"] = loot_items.second;
                loots_arr.push_back(loot_info);
            }
            internal_obj["bag"] = loots_arr;
            internal_obj["score"] = dog.second.GetScore();
            players[std::to_string(dog.first)] = internal_obj;
            i++;
        }
        obj["players"] = players;

        boost::json::object loots_obj;
        const auto loots = session_ptr->GetLoots();
        //for(auto i = 0U; i < loots.size(); i++) { 
        for(const auto& loot_item : loots) {
            boost::json::object internal_obj;
            boost::json::array coords_arr;
            //auto loot = loots.at(i);
            internal_obj["type"] = loot_item.second.GetLootType();
            coords_arr.push_back((loot_item.second.GetCoords().x));
            coords_arr.push_back((loot_item.second.GetCoords().y));
            internal_obj["pos"] = coords_arr;
            //loots_obj[std::to_string(i)] = internal_obj;
            loots_obj[std::to_string(loot_item.first)] = internal_obj;
        }
        obj["lostObjects"] = loots_obj;

        json_string = boost::json::serialize(obj);
        if(method == http::verb::get) {
            response.body() = json_string;
        }
        response.content_length(json_string.size());
        response.set(http::field::cache_control, "no-cache");

        return response;
    }

    Response MakeInValidGetGameStateResponse(http::status status, unsigned http_version, bool is_bad_auth_header = false) {
        StringResponse response(status, http_version);
        std::string json_string;
        boost::json::object obj;

        response.set(http::field::content_type, ContentType::JSON);
        if (status == http::status::unauthorized) {
            if(is_bad_auth_header) {
                obj["code"] = "invalidToken";
                obj["message"] = "Authorization header is required";
            } else {
                obj["code"] = "unknownToken";
                obj["message"] = "Player token has not been found";
            }
        } else if (status == http::status::method_not_allowed) {
            response.set(http::field::allow, "GET, HEAD");
            obj["code"] = "invalidMethod";
            obj["message"] = "Invalid method";
        }
        json_string = boost::json::serialize(obj);
        response.body() = json_string;
        response.content_length(json_string.size());
        response.set(http::field::cache_control, "no-cache");

        return response;
    }

    Response MakeValidMovePlayerResponse(http::status status, unsigned http_version) {
        StringResponse response(status, http_version);
        std::string json_string;
        boost::json::object obj;

        response.set(http::field::content_type, ContentType::JSON);

        json_string = boost::json::serialize(obj);
        response.body() = json_string;
        response.content_length(json_string.size());
        response.set(http::field::cache_control, "no-cache");

        return response;
    }

    Response MakeValidListRetirePlayersResponse(http::status status, unsigned http_version, std::vector<postgres::PlayerRetireInfo>& retire_players_info) {
        StringResponse response(status, http_version);
        std::string json_string;
        //boost::json::object obj;
        boost::json::array arr;

        response.set(http::field::content_type, ContentType::JSON);
        
        for(auto& retire_player_info : retire_players_info) {
            boost::json::object internal_obj;
            internal_obj["name"] = retire_player_info.name;
            internal_obj["score"] = retire_player_info.score;
            internal_obj["playTime"] = retire_player_info.playTime;
            arr.push_back(internal_obj);
        }

        json_string = boost::json::serialize(arr);
        response.body() = json_string;
        response.content_length(json_string.size());
        response.set(http::field::cache_control, "no-cache");

        return response;
    }

    Response MakeInValidListRetirePlayersResponse(http::status status, unsigned http_version) {
        StringResponse response(status, http_version);
        std::string json_string;
        boost::json::object obj;

        response.set(http::field::content_type, ContentType::JSON);
        if (status == http::status::bad_request) {
            obj["code"] = "invalidArgument";
            obj["message"] = "Invalid maxItems";
        } else if (status == http::status::method_not_allowed) {
            response.set(http::field::allow, "GET");
            obj["code"] = "invalidMethod";
            obj["message"] = "Invalid method";
        }
        json_string = boost::json::serialize(obj);
        response.body() = json_string;
        response.content_length(json_string.size());
        response.set(http::field::cache_control, "no-cache");

        return response;
    }

    Response MakeInValidMovePlayerResponse(http::status status, unsigned http_version, bool is_content_type_err = false) {
        StringResponse response(status, http_version);
        std::string json_string;
        boost::json::object obj;

        response.set(http::field::content_type, ContentType::JSON);
        if (status == http::status::bad_request) {
            if(is_content_type_err) {
                obj["code"] = "invalidArgument";
                obj["message"] = "Invalid content type";
            } else {
                obj["code"] = "invalidArgument";
                obj["message"] = "Failed to parse action";
            }
        } else if (status == http::status::method_not_allowed) {
            response.set(http::field::allow, "POST");
            obj["code"] = "invalidMethod";
            obj["message"] = "Invalid method";
        }
        json_string = boost::json::serialize(obj);
        response.body() = json_string;
        response.content_length(json_string.size());
        response.set(http::field::cache_control, "no-cache");

        return response;
    }

    Response MakeInValidTimeControlResponse(http::status status, unsigned http_version) {
        StringResponse response(status, http_version);
        std::string json_string;
        boost::json::object obj;

        response.set(http::field::content_type, ContentType::JSON);
        if (status == http::status::bad_request) {
            obj["code"] = "invalidArgument";
            obj["message"] = "Failed to parse tick request JSON";
        } else if (status == http::status::method_not_allowed) {
            response.set(http::field::allow, "POST");
            obj["code"] = "invalidMethod";
            obj["message"] = "Invalid method";
        }
        json_string = boost::json::serialize(obj);
        response.body() = json_string;
        response.content_length(json_string.size());
        response.set(http::field::cache_control, "no-cache");
        return response;
    }

    Response MakeInvalidInputPointResponse(http::status status, unsigned http_version) {
        StringResponse response(status, http_version);
        std::string json_string;
        boost::json::object obj;

        response.set(http::field::content_type, ContentType::JSON);

        obj["code"] = "badRequest";
        obj["message"] = "Invalid endpoint";

        json_string = boost::json::serialize(obj);
        response.body() = json_string;
        response.content_length(json_string.size());
        response.set(http::field::cache_control, "no-cache");
        return response;
    }

    std::vector<std::string> split(std::string &str, char delim) {
        size_t start;
        size_t end = 0;
        std::vector<std::string> out;
    
        while ((start = str.find_first_not_of(delim, end)) != std::string::npos) {
            end = str.find(delim, start);
            out.push_back(str.substr(start, end - start));
        }
        return out;
    }

    bool is_hex_digit(char c) {
        return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
    }

    std::string DecodeURI(const std::string& target) {

        std::string out_str;
        bool start_check_hex = false;
        bool got_first_hex = false;

        std::string hex_str;
        for(const auto& ch : target) {
            if(ch == '%' ) {
                if(start_check_hex) {
                    out_str += hex_str;
                    hex_str.clear();
                }
                hex_str += ch;
                start_check_hex = true;
                continue;
            } else if(start_check_hex && (hex_str.size() < 2)) {
                hex_str += ch;
                continue;
            } else if(start_check_hex){
                try{
                    hex_str += ch;
                    auto decimal = std::stoi(hex_str.substr(1), nullptr, 16);
                    char dec_ch = static_cast<char>(decimal);
                    out_str += dec_ch;
                    start_check_hex = false;
                    hex_str.clear();
                    continue;
                }catch (...) {
                    start_check_hex = false;
                    out_str += hex_str;
                    hex_str.clear();
                    continue;
                }  
            } else if (ch == '+') {
                out_str += ' ';
                continue;
            }
            out_str += ch; 
        }
        return out_str;
    }

    // Возвращает true, если каталог p содержится внутри base_path.
    bool IsSubPath(fs::path path, fs::path base) {
        // Проверяем, что все компоненты base содержатся внутри path
        for (auto b = base.begin(), p = path.begin(); b != base.end(); ++b, ++p) {
            if (p == path.end() || *p != *b) {
                return false;
            }
        }
        return true;
    }

    Response ApiHandler::Join(const StringRequest& req) const {
        if(req.method() == http::verb::post) {
            std::string body = req.body();
            boost::system::error_code ec;
            json::value value = json::parse(body, ec);
            if(ec) {
                return MakeInValidJoinResponse(http::status::bad_request, req.version(), true);
            }
            try{
                std::string user_name = std::string(value.as_object().at("userName").as_string().c_str());
                util::Tagged<std::string, model::Map> map_id(std::string(value.as_object().at("mapId").as_string().c_str()));
                
                auto join_result = app_.JoinGameUseCase(user_name, map_id);
                return MakeValidJoinResponse(http::status::ok, req.version(), join_result);
            } catch( const app::JoinGameError& ec) {
                auto reason = ec.What();
                if(reason == app::JoinGameError::JoinGameErrorReason::INVALID_MAP) {
                    return MakeInValidJoinResponse(http::status::not_found, req.version());
                } else if (reason == app::JoinGameError::JoinGameErrorReason::INVALID_NAME) {
                    return MakeInValidJoinResponse(http::status::bad_request, req.version());
                }
            } catch ( const std::out_of_range& exp ){
                // Если возникла ошибка при получении свойств json возвращаем 400 Bad request
                return MakeInValidJoinResponse(http::status::bad_request, req.version(), true);
            }
        }
        return MakeInValidJoinResponse(http::status::method_not_allowed, req.version());
    }

    Response ApiHandler::ListPlayers(const StringRequest& req) const {
        auto req_method = req.method();
        if(req_method == http::verb::get || req_method == http::verb::head) {
            if (req.find(http::field::authorization) != req.end()) {
                app::Token token(std::string(""));
                if(!app::IsValidAuthorizationField(req.at(http::field::authorization), token)) {
                    return MakeInValidPlayerListResponse(http::status::unauthorized, req.version(), true);
                }
                const auto session_players = app_.ListPlayersUseCase(token);
                if(session_players.empty()) {
                    // Если отсутствует игрок с заданным токеном
                    return MakeInValidPlayerListResponse(http::status::unauthorized, req.version());
                }
                return MakeValidPlayerListResponse(req_method, http::status::ok, req.version(), session_players);
            }
            return MakeInValidPlayerListResponse(http::status::unauthorized, req.version(), true);
        }
        return MakeInValidPlayerListResponse(http::status::method_not_allowed, req.version());
    }

    Response ApiHandler::GetGameState(const StringRequest& req) const {
        auto req_method = req.method();
        if(req_method == http::verb::get || req_method == http::verb::head) {
            if (req.find(http::field::authorization) != req.end()) {   
                try{
                    //const auto dogs = app_.GetGameStateUseCase(req.at(http::field::authorization));
                    const auto session_ptr = app_.GetGameStateUseCase(req.at(http::field::authorization));
                    return MakeValidGetGameStateResponse(req_method, http::status::ok, req.version(), session_ptr);
                } catch (const app::GetGameStateError& ec) {
                    auto reason = ec.What();
                    if(reason == app::GetGameStateError::GetGameStateErrorReason::INVALID_AUTH_FIELD) {
                        return MakeInValidGetGameStateResponse(http::status::unauthorized, req.version(), true);
                    } else if (reason == app::GetGameStateError::GetGameStateErrorReason::INVALID_TOKEN) {
                        return MakeInValidGetGameStateResponse(http::status::unauthorized, req.version());
                    }
                } 
            }
            return MakeInValidGetGameStateResponse(http::status::unauthorized, req.version(), true);
        }
        return MakeInValidGetGameStateResponse(http::status::method_not_allowed, req.version());
    }
    
    Response ApiHandler::MovePlayers(const StringRequest& req) const {
        auto req_method = req.method();
        if(req_method != http::verb::post) {
            return MakeInValidMovePlayerResponse(http::status::method_not_allowed, req.version());
        }
        if (req.find(http::field::content_type) != req.end() && (req.at(http::field::content_type) == ContentType::JSON)) {
            if(req.find(http::field::authorization) != req.end()) {
                std::string body = req.body();
                boost::system::error_code ec;
                json::value value = json::parse(body, ec);
                if(ec) {
                    return MakeInValidMovePlayerResponse(http::status::bad_request, req.version());
                }
                try{
                    std::string move_direction = std::string(value.as_object().at("move").as_string().c_str());
                    app_.MovePlayersUseCase(req.at(http::field::authorization), move_direction);
                    return MakeValidMovePlayerResponse(http::status::ok, req.version());
                } catch( const app::MovePlayersError& ec) {
                    auto reason = ec.What();
                    if(reason == app::MovePlayersError::MovePlayersErrorReason::INVALID_AUTH_FIELD) {
                        return MakeInValidGetGameStateResponse(http::status::unauthorized, req.version(), true);
                    } else if (reason == app::MovePlayersError::MovePlayersErrorReason::INVALID_TOKEN) {
                        return MakeInValidGetGameStateResponse(http::status::unauthorized, req.version());
                    } else if (reason == app::MovePlayersError::MovePlayersErrorReason::INVALID_MOVE_ARG) {
                        return MakeInValidMovePlayerResponse(http::status::bad_request, req.version());
                    }
                } catch ( const std::out_of_range& exp ){
                    // Если возникла ошибка при получении свойств json возвращаем 400 Bad request
                    MakeInValidMovePlayerResponse(http::status::bad_request, req.version());
                }
            }   
            return MakeInValidGetGameStateResponse(http::status::unauthorized, req.version(), true);
        }
        return MakeInValidMovePlayerResponse(http::status::bad_request, req.version(), true);  
    }

    Response ApiHandler::TickTime(const StringRequest& req) const {
        if(app_.IsGameAuto()) {
            MakeInvalidInputPointResponse(http::status::bad_request, req.version());
        }
        auto req_method = req.method();
        if(req_method != http::verb::post) {
            return MakeInValidTimeControlResponse(http::status::method_not_allowed, req.version());
        }
        if (req.find(http::field::content_type) != req.end() && (req.at(http::field::content_type) == ContentType::JSON)) {
            std::string body = req.body();
            boost::system::error_code ec;
            json::value value = json::parse(body, ec);
            if(ec) {
                return MakeInValidTimeControlResponse(http::status::bad_request, req.version());
            }
            try{
                double time_Delta = value.as_object().at("timeDelta").as_int64();
                if(time_Delta < 0){
                    throw std::invalid_argument({"Invalid_arg"});
                }
                app_.TickTimeUseCase(time_Delta);
                return MakeValidMovePlayerResponse(http::status::ok, req.version());
            } catch ( const std::out_of_range& exp ){
                // Если возникла ошибка при получении свойства timeDelta возвращаем 400 Bad request
                return MakeInValidTimeControlResponse(http::status::bad_request, req.version());
            } catch ( const std::invalid_argument& exp) {
                // Если возникла ошибка при получении валидного значения timeDelta возвращаем 400 Bad request
                return MakeInValidTimeControlResponse(http::status::bad_request, req.version());
            }
        }
        return MakeInValidMovePlayerResponse(http::status::bad_request, req.version(), true);  
    }

    Response ApiHandler::GetMapsInfo(const StringRequest& req) const {
        std::string trg =  static_cast<std::string>(req.target());
        if(req.method() == http::verb::get || req.method() == http::verb::head) {
            // Сцерий вывода информации о игровых картах
            std::vector<std::string> targets_items = split(trg, '/');
            if(trg == "/api/v1/maps"s) {
                // Возвращаем список карт
                const auto& maps = app_.ListMapsUseCase();
                return MakeValidMapsResponse(req.method(), http::status::ok, req.version(), maps);
            } else if ((trg.find("/api/v1/maps"s) != std::string::npos) && targets_items.size() == 4) {
                // Возвращаем информацию по карте
                auto map_ptr = app_.GetMapUseCase(targets_items.at(3));
                if(map_ptr != nullptr) {
                    return MakeValidMapsResponse(req.method(), http::status::ok, req.version(), {}, map_ptr, app_.GetExtraData().GetData());
                }
                return MakeInValidMapsResponse(http::status::not_found, req.version());
            }
            return MakeInValidMapsResponse(http::status::bad_request, req.version());
        }
        //return MakeInvalidStringResponse(http::status::method_not_allowed, req.version());
        return MakeInValidMapsResponse(http::status::method_not_allowed, req.version());
    }

    std::optional<std::vector<std::pair<std::string, std::string>>> ExtractUrlParams(const std::string& url) {
        std::vector<std::pair<std::string, std::string>> params;
        // Поиск начала параметров 
        size_t start = url.find("?");
        if (start == std::string::npos) {
            return std::nullopt; // Если параметров нет, возвращаем пустой вектор
        }
        // Извлечение части строки с параметрами
        std::string query = url.substr(start + 1);

        // Разделение параметров по разделителю '&'
        for (auto it = query.begin(); it != query.end(); ) {
            // Ищем следующий разделитель или конец строки
            auto end = std::find(it, query.end(), '&');

            // Извлекаем параметр
            std::string param = std::string(it, end);
            it = end;
            if (it != query.end()) {
                ++it; // Переходим к следующему символу
            }

            // Разделение параметра на ключ и значение
            size_t eqPos = param.find('=');
            if (eqPos != std::string::npos) {
                std::string key = param.substr(0, eqPos);
                std::string value = param.substr(eqPos + 1);
                params.push_back(std::make_pair(key, value));
            } else {
                return std::nullopt;
            }
        }
        return params;
    }

    Response ApiHandler::ListRetirePlayers(const StringRequest& req) const {
        std::string trg =  static_cast<std::string>(req.target());
        if(req.method() == http::verb::get) {
            std::string decoded_req = DecodeURI(trg);
            auto params = ExtractUrlParams(decoded_req);
            if(params && ((*params).size() == 2)){
                int maxItems = std::stoi((*params).at(1).second);
                if(maxItems > 100) {
                    return MakeInValidListRetirePlayersResponse(http::status::bad_request, req.version());
                }
            }
            auto retire_plyers_info = app_.GetRetirePlayersUseCase(*params);
            return MakeValidListRetirePlayersResponse(http::status::ok, req.version(), retire_plyers_info);
        }
        return MakeInValidListRetirePlayersResponse(http::status::method_not_allowed, req.version());
    }

    Response ApiHandler::HandleApiRequest(const StringRequest& req) const {
        std::string trg =  static_cast<std::string>(req.target());
        if(trg == "/api/v1/game/join"s) {
            return Join(req);
        } else if(trg == "/api/v1/game/players"s) {
            return ListPlayers(req);
        } else if(trg == "/api/v1/game/state"s) {
            return GetGameState(req);  
        } else if(trg == "/api/v1/game/player/action"s) {
            return MovePlayers(req);
        } else if (trg == "/api/v1/game/tick"s) {
            return TickTime(req);
        } else if (trg.find("/api/v1/maps") != std::string::npos) {
            return GetMapsInfo(req);
        } else if (trg.find("/api/v1/game/records"s) != std::string::npos) {
            return ListRetirePlayers(req);
        }
        return MakeInvalidInputPointResponse(http::status::bad_request, req.version());
    }

    Response RequestHandler::HandleFileRequest(const StringRequest& req) const {
        const auto file_response = [&req, this](http::status status, const fs::path path = ""s) {
            if(status == http::status::ok) {
                return MakeValidFileResponse(status, req.version(), path);
            }
            return MakeInValidFileResponse(status, req.version());
        };
        std::string trg =  static_cast<std::string>(req.target());
        if(req.method() == http::verb::get ) {
            fs::path decoded_target = DecodeURI(trg).substr(1);
            if(decoded_target != ""s) {                
                decoded_target = fs::weakly_canonical(root_dir_path_/decoded_target);
            } else {
                decoded_target = root_dir_path_;
            }
            if (IsSubPath(decoded_target, root_dir_path_)) {
                if(fs::is_directory(decoded_target)) {
                    decoded_target += "/index.html"s;
                }
                if (fs::exists(decoded_target)) {
                    return file_response(http::status::ok, decoded_target);
                } else {
                    return file_response(http::status::not_found);
                }
            }
            return file_response(http::status::bad_request);
        }
        return MakeInvalidStringResponse(http::status::method_not_allowed, req.version());
    }

}  // namespace http_handler

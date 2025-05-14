#include "logging.h"
#include  <variant>

namespace server_log {

    void MyFormatter(logging::record_view const& rec, logging::formatting_ostream& strm) {

        json::object output;

        auto ts = rec[timestamp];
        output["timestamp"] = to_iso_extended_string(*ts);

        // Получаем дополнительные данные
        if(rec[additional_data].get_ptr()) {
            output["data"] = (*(rec[additional_data])).as_object();
        }

        // Получаем сообщение
        if(rec[logging::expressions::smessage].get_ptr()) {
            output["message"] = *(rec[logging::expressions::smessage]);
        }

        strm << boost::json::serialize(output) << std::endl;
    }

    unsigned GetResponseResult(const Response& response) {
        if (std::holds_alternative<StringResponse>(response)) {
            return std::get<StringResponse>(response).result_int(); 
        } else if (std::holds_alternative<FileResponse>(response)) {
            return std::get<FileResponse>(response).result_int(); 
        }
        return 0;
    }

    std::string_view GetResponseContentType(const Response& response) {
        try{
            if (std::holds_alternative<StringResponse>(response)) {
                return std::get<StringResponse>(response).at(boost::beast::http::field::content_type); 
            } else if (std::holds_alternative<FileResponse>(response)) {
                return std::get<FileResponse>(response).at(boost::beast::http::field::content_type); 
            }

        } catch(const std::out_of_range& ex) {
            // В случае отсутствия поля content_type в ответе сервера
        }
        return {};
    }
}

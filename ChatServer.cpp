#include <iostream>
#include <uwebsockets/App.h>
#include <nlohmann/json.hpp>
#include <map>
using json = nlohmann::json;

const std::string PUBLIC = "public_all";
const std::string COMMAND = "command";
const std::string PRIVATE_MSG = "private_msg";
const std::string MESSAGE = "message";
const std::string USER_ID = "user_id";
const std::string USER_ID_FROM = "user_id_from";
const std::string PUBLIC_MSG = "public_msg";
const std::string SET_NAME = "set_name";
const std::string NAME = "name";
const std::string STATUS = "status";
const std::string ONLINE = "online";

struct UserData {
    std::int32_t user_id;
    std::string name;
};

std::map<int, UserData*> all_users;

typedef uWS::WebSocket<false, true, UserData> UWebS;

//Возвращает JSON со статусом пользователя
std::string status(UserData* data, bool online) {
    json response;
    response[COMMAND] = STATUS;
    response[NAME] = data->name;
    response[USER_ID] = data->user_id;
    response[ONLINE] = online;
    return response.dump();
}

void processPrivateMessage(UWebS *ws, json parsed, std::int16_t user_id) {
    int user_id_to = parsed[USER_ID];
    std::string user_msg = parsed[MESSAGE];

    json response;
    response[COMMAND] = PRIVATE_MSG;
    response[USER_ID_FROM] = user_id;
    response[MESSAGE] = user_msg;

    ws->publish("userN" + std::to_string(user_id_to), response.dump());
}

void processPublicMessage(UWebS* ws, json parsed, std::int16_t user_id) {
    std::string user_msg = parsed[MESSAGE];

    json response;
    response[COMMAND] = PUBLIC_MSG;
    response[USER_ID_FROM] = user_id;
    response[MESSAGE] = user_msg;

    ws->publish("public_all", response.dump());
}

void processSetName(UWebS* ws, json parsed, UserData* data) {
    data->name = parsed[NAME];
}

void processMessage(UWebS *WS ,std::string_view message) {
    UserData* data = WS->getUserData();

    std::cout << "Message from user ID: " << data->user_id << " message: " << message << std::endl;
    auto parsed = json::parse(message);

    if (parsed[COMMAND] == PRIVATE_MSG) {
        processPrivateMessage(WS, parsed, data->user_id);
    }

    if (parsed[COMMAND] == PUBLIC_MSG) {
        processPublicMessage(WS, parsed, data->user_id);
    }

    if (parsed[COMMAND] == SET_NAME) {
        processSetName(WS, parsed, data);
        WS->publish(PUBLIC, status(data, true));
    }
}


int main()
{
    // 1. Задать настройки сервера
    // 2. Прилумать структуру данных ассоциированную с пользователем
    
    std::int32_t latest_id = 10;
    
    uWS::App().ws<UserData>("/*", {
        .idleTimeout = 1000, // сколько секунд требуется чтобы отключить человека при бездействии
        .open = [&latest_id](auto* ws) {
            UserData *data = ws->getUserData();
            // назначаем id
            data->user_id = latest_id++;
            data->name = "unnamed";

            std::cout << "New user connected ID: " << data->user_id << std::endl;
            // присоеденение к приватному каналу пользователя
            ws->subscribe("userN" + std::to_string(data->user_id));
            //Уведомление что человек подключился к публичному чату и он онлайн
            ws->publish(PUBLIC, status(data, true));
            //Присоединение к общему чату
            ws->subscribe(PUBLIC);

            //при подключении сообщаем о всех участниках чата
            for (auto entry : all_users) {
                ws->send(status(entry.second, true), uWS::OpCode::TEXT); // отправление информации конкретному пользователю а не каналу
            }

            all_users[data->user_id] = data; // добавление пользователя в итератор all_users
        },

        .message = [](auto* ws, std::string_view message, uWS::OpCode) {
            processMessage(ws, message);
        },

        // string_view тип данных похожий на string
        .close = [](auto *ws, std::int64_t code, std::string_view message) {
            // Получаем данные пользователя
            UserData* data = ws->getUserData();

            
            std::cout << "User disconnected ID: " << data->user_id << std::endl;
            all_users.erase(data->user_id);
        },
        // []() {} - лямбда функция
        }).listen(9001, [](auto*) {  // listen номер порта который прослушивает сервер
            std::cout << "Server Started successfully" << std::endl;
            }).run();
}


//TODO:
// 1. Добавить поддержку имен
// 2. пользователи должны знать статус друг друга
// 3. Онлайн
// 4. Оффлайн
// 5. Смена имени
// 6. Узнатьт сколько и кто на сервере
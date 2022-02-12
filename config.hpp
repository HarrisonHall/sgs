/**
 * config.hpp
 */

#include <cstdint>
#include <functional>
#include <string>
#include <map>

#include "json.hpp"

using nlohmann::json;


bool debug = true;
uint16_t port = 3000;
uint64_t max_players = 256;
uint64_t max_lobbies = 16;
uint64_t max_players_per_lobby = 16;

uint16_t player_timeout = 12;  // Seconds until player is forcefully disconnected

std::map<std::string, std::function<json (const json)>> game_processing = {
	{"increment", [](const json message) {
		json return_message = message;
		json data = message.value("data", json::object());
		int value = data.value("value", 0);
		data["value"] = value;
		return_message["data"] = data;
		return return_message;
	}}
};

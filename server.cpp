/**
 * server.cpp
 */

#include <string>
#include <deque>
#include <map>
#include <iostream>

#include "uWebSockets/src/App.h"
#include "json.hpp"

#include "config.hpp"

using nlohmann::json;
struct GameSession;
struct PlayerDetails;

struct GameSession {
	std::string lobby_name;
	std::string game_name;
	std::deque<PlayerDetails *> players;
	json initialization_data;

	size_t num_players();
	bool remove_player(PlayerDetails *player);
};

struct PlayerDetails {
	uint64_t id;
	std::string game_session_name;
};

std::map<std::string, GameSession> games;


const std::string just_connected_message = json({
		{"type", "connected"},
		{"data", {}}
	}).dump();

int main() {
	uWS::App app = uWS::App();  // Websocket app
	
	app.ws<PlayerDetails *>("/game_server", {
			//.compression = uWS::SHARED_COMPRESSOR,
			//.maxPayloadLength = 16 * 1024,
			.idleTimeout = player_timeout,
			
			.open = [=](auto *ws) {
				ws->send(just_connected_message);
				std::cout << "New connection" << std::endl;
			},
			.message = [](auto *ws, std::string_view message, uWS::OpCode opCode) {
        ws->send(message, opCode);
			},
			.close = [](auto *ws, int code, std::string_view message) {
				std::cout << "Player left" << std::endl;
				// TODO - cleanup
			}
		});
	
	app.listen(port, [](auto *listen_socket) {
		if (listen_socket) {
			std::cout << "Listening" << std::endl;
		}
	});
	app.run();
	std::cout << "Failed to listen" << std::endl;
	// TODO spawn thread for cleaning up games
	// TODO loop searching for new connections
}

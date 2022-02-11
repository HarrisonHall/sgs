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
struct LobbySession;
struct PlayerDetails;
std::map<std::string, LobbySession*> lobbies;


struct LobbySession {
	std::string lobby_name;
	std::string game_name;
	std::deque<PlayerDetails *> players;
	json initialization_data;

	LobbySession(PlayerDetails *leader, std::string lobby_name, std::string game_name)
		: game_name(game_name), lobby_name(lobby_name) {
		this->players.push_back(leader);
	}

	PlayerDetails *get_leader() {
		if (this->players.size() == 0) return nullptr;
		return this->players[0];
	}
	size_t num_players() {
		return this->players.size();
	}
	bool is_full() {
		return (this->num_players() >= max_players_per_lobby);
	}
	bool has_player(PlayerDetails *player) {
		auto search = std::find(this->players.begin(), this->players.end(), player);
		return (search != this->players.end());
	}
	void add_player(PlayerDetails *player) {
		this->players.push_back(player);
	}
	bool remove_player(PlayerDetails *player) {
		auto search = std::find(this->players.begin(), this->players.end(), player);
		if (search != this->players.end()) {
			this->players.erase(search);
			return true;
		}
		return false;
	}
};

struct PlayerDetails {
	static uint64_t last_id;
	uint64_t id = 0;
	std::string lobby_session_name;
	uWS::WebSocket<false, true, PlayerDetails> *socket_connection;
	
	bool in_valid_lobby() {
		if (this->lobby_session_name == "") return false;
		auto search = lobbies.find(lobby_session_name);
		return (search != lobbies.end());
	}
	
	LobbySession *get_lobby() {
		if (!this->in_valid_lobby()) return nullptr;
		return lobbies.find(lobby_session_name)->second;
	}
	
	bool is_leader() {
		if (this->in_valid_lobby()) {
			if (this->get_lobby()->players[0] == this) {
				return true;
			}
		}
		return false;
	}
};
uint64_t PlayerDetails::last_id = 0;
uint64_t total_players = 0;


const json EMPTY_JSON = json::object();
const json CONNECTED = {
	{"type", "connected"},
	{"data", EMPTY_JSON}
};
const std::string CONNECTED_MESSAGE = CONNECTED.dump();
const json SUCCESS = {
	{"type", "success"},
	{"data", EMPTY_JSON}
};
const std::string SUCCESS_MESSAGE = SUCCESS.dump();
const json ERROR = {
	{"type", "error"},
	{"data", EMPTY_JSON}
};
const std::string ERROR_MESSAGE = ERROR.dump();

int main() {
	uWS::App app = uWS::App();  // Websocket app
	
	app.ws<PlayerDetails>("/game_server", {
		// General settings
		.idleTimeout = player_timeout,
			
		// Connection started - initialization
		.open = [=](auto *ws) {
			auto *player_info = reinterpret_cast<PlayerDetails *>(ws->getUserData());
			
			player_info->id = ++PlayerDetails::last_id;
			player_info->socket_connection = ws;

			std::cout << "--Joined: " << player_info->id << std::endl;
			total_players++;

			ws->send(CONNECTED_MESSAGE);
		},
		
		// Generic message received
		.message = [](auto *ws, std::string_view _message, uWS::OpCode opCode) {
			auto *current_player = reinterpret_cast<PlayerDetails *>(ws->getUserData());

			auto message = json::parse(_message, nullptr, false, true);
			std::string lobby_name = message.value("lobby", "");
			std::string game_name = message.value("game", "");
			std::string message_type = message.value("type", "error");

			if (message_type == "initialization_data" && current_player->is_leader()) {
				
			}

			if (message_type == "error" || message_type != "data") {
				// Ignore for now
			}

			if (game_processing.find(game_name) != game_processing.end()) {
				message = game_processing[game_name](message);
			}
			
			if (current_player->in_valid_lobby()) {
				if (current_player->is_leader()) {
					// Send to everyone
					std::cout << "--Forwarding to everyone" << std::endl;
					for (auto *player : current_player->get_lobby()->players) {
						auto dumped_message = message.dump();
						if (player != current_player) {
							player->socket_connection->send(dumped_message);
						}
					}
				} else {
					// Send to leader
					std::cout << "--Forwarding to leader" << std::endl;
					auto *leader = current_player->get_lobby()->get_leader();
					if (leader) {
						leader->socket_connection->send(message.dump());
					}
				}
			} else {
				// Modify lobby
				auto search = lobbies.find(lobby_name);

				if (lobby_name == "") {
					// Invalid lobby
					ws->send(ERROR_MESSAGE);
				} else if (search == lobbies.end()) {
					// Create lobby if doesn't exists
					json creation_success = SUCCESS;
					std::cout << "Creating lobby" << std::endl;
					
					LobbySession *new_lobby = new LobbySession(current_player, lobby_name, game_name);
					lobbies[lobby_name] = new_lobby;
					current_player->lobby_session_name = lobby_name;
					creation_success["data"]["is_leader"] = true;
					ws->send(creation_success.dump());
				} else {
					// Add to lobby if not full
					std::cout << "Adding to lobby" << std::endl;
					auto *lobby = search->second;
					json joining_success = SUCCESS;
					
					if (lobby->is_full()) {
						ws->send(ERROR_MESSAGE);
					} else {
						lobby->add_player(current_player);
						current_player->lobby_session_name = lobby_name;
						joining_success["data"]["is_leader"] = false;
						ws->send(joining_success.dump());
					}
				}
			}
		},
		
		// Connection ended - destruction
		.close = [](auto *ws, int code, std::string_view message) {
			auto *current_player = reinterpret_cast<PlayerDetails *>(ws->getUserData());
			
			auto *lobby = current_player->get_lobby();
			lobby->remove_player(current_player);
			if (lobby->num_players() == 0) {
				lobbies.erase(lobby->lobby_name);
				std::cout << "Deleting lobby " << lobby->lobby_name << std::endl;
				delete lobby;
			}

			total_players--;
			
			std::cout << "--Left: " << current_player->id << std::endl;
		}
		
	});  // Set up websocket

	// Server status
	app.get("/status", [](auto *res, auto *req) {
		json status = {
			{"players", total_players},
			{"lobbies", lobbies.size()},
			{"next_player_id", PlayerDetails::last_id + 1}
		};
		res->end(status.dump());
	});

	// Current lobbies
	app.get("/lobbies", [](auto *res, auto *req) {
		json lobby_info = {
			{"lobbies", EMPTY_JSON}
		};
		for (const auto l : lobbies) {
			lobby_info["lobbies"][l.first] = l.second->num_players();
		}
		
		res->end(lobby_info.dump());
	});
	
	app.listen(port, [](auto *listen_socket) {
		if (listen_socket) {
			std::cout << "Listening" << std::endl;
		}
	});
	
	app.run();
	
	std::cout << "Failed to listen" << std::endl;
	// TODO spawn thread for cleaning up games?
}

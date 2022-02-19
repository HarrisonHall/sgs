// server.cpp
// ==========
// Run simple game server.
// Configuration done in config.hpp.


#pragma GCC diagnostic ignored "-Wunused-value"  // Selectively ignore assert warning

#include <cassert>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "uWebSockets/src/App.h"
#include "json.hpp"

#include "config.hpp"


using nlohmann::json;

struct LobbySession;  // Lobby information
struct PlayerDetails;  // Player/connection information


struct LobbySession {
	std::string lobby_name;  // Name of lobby. Equivalent to corresponding key in lobbies.
	std::string game_name;  // Name of lobby game. Must match for player to join lobby. 
	std::vector<PlayerDetails *> players;  // All players in the game. The first player is the lobby leader.
	json initialization_data = json::object();  // Data sent to new players to recreate current game state.
	static std::map<std::string, LobbySession*> sessions;  // All LobbySession::sessions by name

	LobbySession(PlayerDetails *leader, const std::string &lobby_name, const std::string &game_name)
		: game_name(game_name), lobby_name(lobby_name), players{leader} {}

	// Get lobby leader
	PlayerDetails *get_leader() const {
		assert(("Players is not empty (lobby should have been deinitialized)", this->players.size() > 0));
		if (this->players.size() == 0) return nullptr;
		return this->players[0];
	}

	// Number of players
	size_t num_players() const {
		return this->players.size();
	}

	// True if lobby is full
	bool is_full() const {
		return (this->num_players() >= config::max_players_per_lobby);
	}

	// Check for player in lobby
	bool has_player(PlayerDetails *player) const {
		auto search = std::find(this->players.begin(), this->players.end(), player);
		return (search != this->players.end());
	}

	// Add player to lobby
	void add_player(PlayerDetails *player) {
		assert(("Lobby should not be overfilled", this->num_players() + 1 <= config::max_players));
		this->players.push_back(player);
	}

	// Remove player from lobby
	bool remove_player(PlayerDetails *player) {
		auto search = std::find(this->players.begin(), this->players.end(), player);
		if (search != this->players.end()) {
			this->players.erase(search);
			return true;
		}
		return false;
	}
};
std::map<std::string, LobbySession*> LobbySession::sessions;

struct PlayerDetails {
	static uint64_t last_id;  // Last id given to a player (increments for each new connection)
	static uint64_t num_concurrent_players;  // Total number of concurrent players
	uint64_t id = 0;  // Id of current player
	LobbySession *lobby;
	uWS::WebSocket<false, true, PlayerDetails> *socket_connection;

	// True if player in valid lobby
	bool in_valid_lobby() {
		return (lobby != nullptr);
	}

	// True if player is leader of lobby
	bool is_leader() {
		if (this->in_valid_lobby()) {
			if (this->lobby->players[0] == this) {
				return true;
			}
		}
		return false;
	}
};
uint64_t PlayerDetails::last_id = 0;
uint64_t PlayerDetails::num_concurrent_players = 0;


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
const json DATA = {
	{"type", "data"},
	{"data", EMPTY_JSON}
};
const std::string DATA_MESSAGE = DATA.dump();

int main() {
	uWS::App app = uWS::App();  // Websocket app

	// Set up websocket endpoint for players
	app.ws<PlayerDetails>("/game_server", {
		// General settings
		.idleTimeout = config::player_timeout,
			
		// Connection started - initialization
		.open = [=](auto *ws) {
			auto *player_info = reinterpret_cast<PlayerDetails *>(ws->getUserData());

			if (PlayerDetails::num_concurrent_players >= config::max_players) {
				ws->close();
				return;
			}
			
			player_info->id = ++PlayerDetails::last_id;
			player_info->socket_connection = ws;

			printf("--Joined: [%llx]\n", player_info->id);
			PlayerDetails::num_concurrent_players++;

			ws->send(CONNECTED_MESSAGE);
		},
		
		// Generic message received
		.message = [](auto *ws, std::string_view _message, uWS::OpCode opCode) {
			auto *current_player = reinterpret_cast<PlayerDetails *>(ws->getUserData());

			auto message = json::parse(_message, nullptr, false, true);
			std::string lobby_name = message.value("lobby", "");
			std::string game_name = message.value("game", "");
			std::string message_type = message.value("type", "error");

			if (message_type == "initialization_data" && current_player->is_leader() && current_player->in_valid_lobby()) {
				current_player->lobby->initialization_data = message.value("data", EMPTY_JSON);
				return;
			}

			if (message_type == "error" || message_type != "data") {
				return;  // Ignore for now
			}

			// Process packets for certain games
			if (config::game_processing.find(game_name) != config::game_processing.end()) {
				message = config::game_processing[game_name](message);
			}
			
			if (current_player->in_valid_lobby()) {
				auto dumped_message = message.dump();
				
				if (current_player->is_leader()) {
					// Send to everyone
					for (auto *player : current_player->lobby->players) {
						if (player != current_player) {
							player->socket_connection->send(dumped_message);
						}
					}
				} else {
					// Send to leader
					auto *leader = current_player->lobby->get_leader();
					if (leader) {
						leader->socket_connection->send(dumped_message);
					}
				}
			} else {
				// Modify lobby
				auto search = LobbySession::sessions.find(lobby_name);

				if (lobby_name == "") {
					// Invalid lobby
					ws->send(ERROR_MESSAGE);
				} else if (search == LobbySession::sessions.end()) {
					// Create lobby if doesn't exist
					json creation_success = SUCCESS;
					
					printf("--Creating lobby: %s [%llx]\n", lobby_name.c_str(), current_player->id);
					
					LobbySession *new_lobby = new LobbySession(current_player, lobby_name, game_name);
					LobbySession::sessions[lobby_name] = new_lobby;
					current_player->lobby = new_lobby;
					creation_success["data"]["is_leader"] = true;
					creation_success["data"]["player_id"] = current_player->id;
					creation_success["lobby"] = lobby_name;
					ws->send(creation_success.dump());
				} else {
					// Add to lobby if not full and game matches
					auto *lobby = search->second;
					json joining_success = SUCCESS;

					printf("--Joining lobby: %s [%llx]\n", lobby_name.c_str(), current_player->id);

					if (lobby->game_name != game_name) {
						ws->send(ERROR_MESSAGE);
					} else if (lobby->is_full()) {
						ws->send(ERROR_MESSAGE);
					} else {
						lobby->add_player(current_player);
						current_player->lobby = lobby;
						joining_success["data"]["is_leader"] = false;
						joining_success["data"]["player_id"] = current_player->id;
						joining_success["lobby"] = lobby_name;
						ws->send(joining_success.dump());

						// Send initialization data
						auto data_message = DATA;
						data_message["data"] = lobby->initialization_data;
						data_message["lobby"] = lobby_name;
						ws->send(data_message.dump());
					}
				}
			}
		},
		
		// Connection ended - destruction
		.close = [](auto *ws, int code, std::string_view message) {
			auto *current_player = reinterpret_cast<PlayerDetails *>(ws->getUserData());
			
			auto *lobby = current_player->lobby;
			bool was_leader = current_player->is_leader();
			lobby->remove_player(current_player);
			if (lobby->num_players() == 0) {
				LobbySession::sessions.erase(lobby->lobby_name);
				printf("--Deleting lobby: %s\n", lobby->lobby_name.c_str());
				delete lobby;
			} else if (was_leader) {
				auto new_leader_message = SUCCESS;
				new_leader_message["data"]["is_leader"] = true;
				new_leader_message["lobby"] = lobby->lobby_name;
				lobby->players[0]->socket_connection->send(new_leader_message.dump());
			}

			PlayerDetails::num_concurrent_players--;
			
			printf("--Disconnected: [%llx]\n", current_player->id);
		}
		
	});  // Set up websocket

	// Set up server status endpoint
	app.get("/status", [](auto *res, auto *req) {
		json status = {
			{"num_players", PlayerDetails::num_concurrent_players},
			{"num_lobbies", LobbySession::sessions.size()},
			{"next_player_id", PlayerDetails::last_id + 1}
		};
		res->end(status.dump());
	});

	// Set up lobby information endpoint
	app.get("/lobbies", [](auto *res, auto *req) {
		json lobby_info = {
			{"lobbies", EMPTY_JSON}
		};
		for (const auto l : LobbySession::sessions) {
			lobby_info["lobbies"][l.first] = {
				{"num_players", l.second->num_players()},
				{"game", l.second->game_name}
			};
		}
		
		res->end(lobby_info.dump());
	});

	// Listen on configured port
	app.listen(config::port, [](auto *listen_socket) {
		if (listen_socket) {
			printf("!Running on port: %hu\n", config::port);
		}
	});
	
	app.run();  // Start server

	// This is only executed if server failed to bind
	printf("!Failed to run on port: %hu\n", config::port);
}

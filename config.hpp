/**
 * config.hpp
 */

#include <cstdint>
#include <functional>
#include <string>
#include <map>

uint16_t port = 3000;
uint64_t max_players = 256;
uint64_t max_lobbies = 16;
uint64_t max_players_per_lobby = 16;

uint16_t lobby_timeout = 120;
uint16_t player_timeout = 12;  // Seconds until player is forcefully disconnected

std::map<std::string, int> game_processing;

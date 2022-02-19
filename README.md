# Simple Game Server
A (very) simple and lightweight game server for managing lobbies.

Vanilla compilation provides a standalone executable that runs a server on port 3000.
Clients use websockets to communicate with the server at the `/game_server` endpoint.
You can check generic status information via a get request to the `/status` endpoint.

A simple usage would have a client try to create a lobby by specifying the lobby and
game names. After receiving a success message, the client will become a lobby leader.
Future lobby members will have all of their traffic forwarded to the leader, and all
leader traffic will be forwarded to each member. The leader can also send initialization
information which informs new lobby members how to initialize their game. A lobby is
closed when all members leave. Members can also find available lobbies through the
`/lobbies` get endpoint.

Configuration is done in the config header *before* compilation.
You can add custom processing functions to the configuration, but the server is designed
for games where a lobby leader (the first person to join a lobby) manages data validation
on the client side.

## Building && Running
```
# Compile uWebSockets
git submodule update --init --recursive
cd uWebSockets
make
cd ..

# Build sgs
make

# Run
./sgs
```

## Future work
- Generalize makefile
- Standardize logging
- Create some interface for managing the server after it already launched
- Consider adding security (SSL)
- Manage multiple servers for use in something like kubernetes
- Robust exception handling so clients can't crash server
- Loop/track player IDs
- Add documentation about json messages

## FAQ
- There are no launch flags, all configuration is done before compilation
- All information is serialized in json

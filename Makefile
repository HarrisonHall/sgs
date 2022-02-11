
default: sgs


sgs: server.cpp config.hpp
	g++ server.cpp uWebSockets/uSockets/uSockets.a -I uWebSockets/uSockets/src -lz -o sgs --std=c++17 -Ofast


clean:
	rm -f sgs

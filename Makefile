
default: sgs


sgs: server.cpp config.hpp
	g++ server.cpp uWebSockets/uSockets/uSockets.a -I uWebSockets/uSockets/src -lz -o sgs

clean:
	rm -f sgs

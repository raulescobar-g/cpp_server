# makefile

all: server client

common.o: common.h common.cpp
	g++ -g -w -std=c++11 -c common.cpp 

Histogram.o: Histogram.h Histogram.cpp
	g++ -g -w -std=c++11 -c Histogram.cpp

FIFOreqchannel.o: FIFOreqchannel.h FIFOreqchannel.cpp
	g++ -g -w -std=c++11 -c FIFOreqchannel.cpp 

TCPRequestChannel.o: TCPRequestChannel.h TCPRequestChannel.cpp
	g++ -g -w -std=c++11 -c TCPRequestChannel.h TCPRequestChannel.cpp

client: client.cpp Histogram.o FIFOreqchannel.o common.o TCPRequestChannel.o
	g++ -g -w -std=c++11 -fsanitize=address -fno-omit-frame-pointer -o client client.cpp Histogram.o FIFOreqchannel.o common.o TCPRequestChannel.o -lpthread 

server: server.cpp  FIFOreqchannel.o common.o TCPRequestChannel.o
	g++ -g -w -std=c++11 -fsanitize=address -fno-omit-frame-pointer -o server server.cpp FIFOreqchannel.o common.o TCPRequestChannel.o -lpthread 

clean:
	rm -rf *.o fifo* server client 

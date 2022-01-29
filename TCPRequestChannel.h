#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <thread>
#include "common.h"
using namespace std;


class TCPRequestChannel{
    private:
	 /* Since a TCP socket is full-duplex, we need only one.*/
	    int sockfd;

    public:
        /* If the host name is an empty string, set up the channel for
        the server side. If the name is non-empty, the constructor
        works for the client side.*/

        TCPRequestChannel (const string host_name, const string port_no);

        /* This is used by the server to create a channel out of a newly accepted client socket. Note that an accepted client socket is ready for communication */
        TCPRequestChannel(int _sockfd);
        /* destructor */
        ~TCPRequestChannel();

        int cread(void* msgbuf, int buflen);

        int cwrite(void* msgbuf, int msglen);

        /* this is for adding the socket to the epoll watch list */
        int getfd();
};


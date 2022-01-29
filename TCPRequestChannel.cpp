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
#include "TCPRequestChannel.h"
using namespace std;



TCPRequestChannel::TCPRequestChannel (const string host_name, const string port_no) {
    struct addrinfo hints, *res, *serv;
    memset(&hints, 0, sizeof(hints));

    if (host_name == "") {
        int new_fd;  // listen on sock_fd, new connection on new_fd
        struct sockaddr_storage their_addr; // connector's address information
        socklen_t sin_size;
        char s[INET6_ADDRSTRLEN];
        int rv;

    
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE; // use my IP

        if ((rv = getaddrinfo(NULL, port_no.c_str(), &hints, &serv)) != 0) {
            cerr  << "getaddrinfo: " << gai_strerror(rv) << endl;
            exit(1);
        }
        if ((sockfd = socket(serv->ai_family, serv->ai_socktype, serv->ai_protocol)) == -1) {
            perror("server: socket");
            exit(1);
        }
        if (::bind(sockfd, serv->ai_addr, serv->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            exit(1);
        }

        freeaddrinfo(serv); // all done with this structure

        if (listen(sockfd, 20) == -1) {
            perror("sever: listen");
            exit(1);
        }
    }
    else {

        // first, load up address structs with getaddrinfo():
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        int status;
        //getaddrinfo("www.example.com", "3490", &hints, &res);
        if ((status = getaddrinfo (host_name.c_str(), port_no.c_str(), &hints, &res)) != 0) {
            cerr << "getaddrinfo: " << gai_strerror(status) << endl;
            exit(1);
        }

        // make a socket:
        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sockfd < 0){
            perror ("Cannot create socket");	
            exit(1);
        }

        // connect!
        if (connect(sockfd, res->ai_addr, res->ai_addrlen)<0){
            perror ("Cannot Connect");
            exit(1);
        }
        //
        //cout << "Connected " << endl;
        // now it is time to free the memory dynamically allocated onto the "res" pointer by the getaddrinfo function
        freeaddrinfo (res);
    }
}

/* This is used by the server to create a channel out of a newly accepted client socket. Note that an accepted client socket is ready for communication */
TCPRequestChannel::TCPRequestChannel(int _sockfd){
    sockfd = _sockfd;
}
/* destructor */
TCPRequestChannel::~TCPRequestChannel() {close(sockfd);}

int TCPRequestChannel::cread(void* msgbuf, int buflen) {return recv(sockfd, msgbuf, buflen, 0);}

int TCPRequestChannel::cwrite(void* msgbuf, int msglen) {return send(sockfd, msgbuf, msglen, 0);}

/* this is for adding the socket to the epoll watch list */
int TCPRequestChannel::getfd() {return sockfd;}

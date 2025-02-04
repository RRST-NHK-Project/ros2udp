#ifndef UDP_HPP
#define UDP_HPP

#include <vector>
#include <iostream>
#include <sstream>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

class UDP {
public:
    UDP(const std::string& ip_address, int port);
    void send(std::vector<int>& data);

private:
    int udp_socket;
    struct sockaddr_in src_addr, dst_addr;
};

#endif // UDP_HPP

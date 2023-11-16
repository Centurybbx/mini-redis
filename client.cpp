#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <string>
#include <stdio.h>

#include "util.h"

const size_t k_max_msg = 4096;

int32_t send_req(int fd, const char* text) {
    uint32_t len = strlen(text);
    if (len > k_max_msg) {
        return -1;
    }

    char wbuf[4 + k_max_msg];
    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4], text, len);
    return util::write_all(fd, wbuf, 4 + len);
}

int32_t read_res(int fd) {
    char rbuf[4 + k_max_msg + 1];
    errno = 0;
    int32_t err = util::read_full(fd, rbuf, 4);
    if (err) {
        if (errno == 0) {
            util::error("EOF");
        } else {
            util::error("read() error");
        }
        return err;
    }

    uint32_t len = 0;
    memcpy(&len, rbuf, 4);
    if (len > k_max_msg) {
        util::error("too long");
        return -1;
    }

    // relpy body
    err = util::read_full(fd, &rbuf[4], len);
    if (err) {
        util::error("read() error");
        return err;
    }

    // do something
    rbuf[4 + len] = '\0';
    printf("server says: %s\n", &rbuf[4]);
    // std::cout << "server says: " << rbuf[4] << std::endl;
    return 0;
}

int main() {
    int cfd;
    cfd = socket(AF_INET, SOCK_STREAM, 0);
    if (cfd < 0) {
        util::abort("client socket()");
    }

    sockaddr_in server_address = {};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(9966);
    server_address.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);    // 127.0.0.1

    int rv = connect(cfd, reinterpret_cast<sockaddr*>(&server_address), sizeof(server_address));
    if (rv) {
        util::abort("client abort");
    }
    
    // multiple pipelined requests
    std::vector<std::string> query_list = {"hello1", "hello2", "hello3"};
    for (size_t i = 0; i < query_list.size(); ++i) {
        int32_t err = send_req(cfd, query_list[i].c_str());
        if (err) {
            close(cfd);
            return 0;
        }
    }

    for(size_t i = 0; i < query_list.size(); ++i) {
        int32_t err = read_res(cfd);
        if (err) {
            close(cfd);
            return 0;
        }
    }

    close(cfd);
    return 0;
}
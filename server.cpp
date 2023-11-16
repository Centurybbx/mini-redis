#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <poll.h>
#include <vector>
#include <fcntl.h>
#include <string>
#include <stdlib.h>
#include <stdio.h>

#include "util.h"

const size_t k_max_msg = 4096;

enum {
  STATE_REQ = 0,  // reading request
  STATE_RES = 1,  // sending request
  STATE_END = 2,  // mark the connection for deletion
};

struct Conn {
  int fd = -1;
  uint32_t state = 0;
  // buffer for reading
  size_t rbuf_size = 0;
  uint8_t rbuf[4 + k_max_msg];
  // buffer for writing
  size_t wbuf_size = 0;
  size_t wbuf_sent = 0;
  uint8_t wbuf[4 + k_max_msg];
};

void state_res(Conn* conn);
void state_req(Conn* conn);

void fd_set_nb(int fd) {
  errno = 0;
  int flags = fcntl(fd, F_GETFL, 0);
  if (errno) {
    util::abort("fcntl error");
    return;
  }

  flags |= O_NONBLOCK;

  errno = 0;
  fcntl(fd, F_SETFL, flags);
  if (errno) {
    util::abort("fcntl error");
  }
}

void conn_put(std::vector<Conn *>& fd2conn, struct Conn* conn) {
  if (fd2conn.size() <= conn->fd) {
    fd2conn.resize(conn->fd + 1);
  }
  fd2conn[conn->fd] = conn;
}

int32_t accept_new_conn(std::vector<Conn *>& fd2conn, int fd) {
  sockaddr_in client_addr = {};
  socklen_t socklen = sizeof(client_addr);
  int connfd = accept(fd, reinterpret_cast<sockaddr*>(&client_addr), &socklen);
  if (connfd < 0) {
    util::error("accept() error");
    return -1;
  }

  fd_set_nb(connfd);

  Conn* conn = reinterpret_cast<Conn *>(malloc(sizeof(Conn)));
  if (!conn) {
    close(connfd);
    return -1;
  }
  conn->fd = connfd;
  conn->state = STATE_REQ;
  conn->rbuf_size = 0;
  conn->wbuf_size = 0;
  conn->wbuf_sent = 0;
  conn_put(fd2conn, conn);
  return 0;
}

bool try_one_request(Conn* conn) {
  // try to parse a request from the buffer
  if (conn->rbuf_size < 4) {
    // not enough data in the buffer. Will retry in the next iteration.
    return false;
  }

  uint32_t len = 0;
  memcpy(&len, &conn->rbuf[0], 4);
  if (len > k_max_msg) {
    util::error("too long");
    conn->state = STATE_END;
    return false;
  }

  if (4 + len > conn->rbuf_size) {
    // not enough data in the buffer. Will retry in the next iteration.
    return false;
  }

  printf("client says: %.*s\n", len, &conn->rbuf[4]);
  // std::cout << "client says: " << conn->rbuf << std::endl;

  memcpy(&conn->wbuf[0], &len, 4);
  memcpy(&conn->wbuf[4], &conn->rbuf[4], len);
  conn->wbuf_size = 4 + len;

  // remove request from the buffer
  size_t remain = conn->rbuf_size - 4 - len;
  if (remain) {
    memmove(conn->rbuf, &conn->rbuf[4 + len], remain);
  }
  conn->rbuf_size = remain;

  // change state
  conn->state = STATE_RES;
  state_res(conn);

  // continue the outer loop if the request was fully processed
  return (conn->state == STATE_REQ);
}

bool try_fill_buffer(Conn* conn) {
  // try to fill the buffer
  assert(conn->rbuf_size < sizeof(conn->rbuf));
  ssize_t rv = 0;
  do {
    size_t cap = sizeof(conn->rbuf) - conn->rbuf_size;
    rv = read(conn->fd, &conn->rbuf[conn->rbuf_size], cap);
  } while (rv < 0 && errno == EINTR);
  if (rv < 0 && errno == EAGAIN) {
    // got EAGAIN, stop
    return false;
  }

  if (rv < 0) {
    util::error("read() error");
    conn->state = STATE_END;
    return false;
  }
  if (rv == 0) {
    if (conn->rbuf_size > 0) {
      util::error("unpected EOF");
    } else {
      util::error("EOF");
    }
    conn->state = STATE_END;
    return false;
  }

  conn->rbuf_size += rv;
  assert(conn->rbuf_size <= sizeof(conn->rbuf));

  // try to process request one by one.
  while (try_one_request(conn)) {}
  return (conn->state == STATE_REQ);
}

void state_req(Conn* conn) {
  while (try_fill_buffer(conn)) {}
}

bool try_flush_buffer(Conn* conn) {
  ssize_t rv = 0;
  do {
    size_t remain = conn->wbuf_size - conn->wbuf_sent;
    rv = write(conn->fd, &conn->wbuf[conn->wbuf_sent], remain);
  } while (rv < 0 && errno == EINTR);
  if (rv < 0 && errno == EAGAIN) {
    // got EAGAIN, stop.
    return false;
  }

  if (rv < 0) {
    util::error("write() error");
    conn->state = STATE_END;
    return false;
  }
  conn->wbuf_sent += rv;
  assert(conn->wbuf_sent <= conn->wbuf_size);
  if (conn->wbuf_sent == conn->wbuf_size) {
    // response was fully sent, change state back
    conn->state = STATE_REQ;
    conn->wbuf_sent = 0;
    conn->wbuf_size = 0;
    return false;
  }

  // still got some data in wbuf, could tey to write again
  return true;
}

void state_res(Conn* conn) {
  while (try_flush_buffer(conn)) {}
}

void connection_io(Conn* conn) {
  if (conn->state == STATE_REQ) {
    state_req(conn);
  } else if (conn->state == STATE_RES) {
    state_res(conn);
  } else {
    assert(0);  // not expected
  }
}

int main() {
  int sfd;
  sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd < 0) {
    util::abort("server socket()");
  }

  // set common opts for socket server
  int val = 1;
  setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

  // bind
  sockaddr_in server_address = {};
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(9966);
  server_address.sin_addr.s_addr = ntohl(0);
  int rv = bind(sfd, reinterpret_cast<const sockaddr*>(&server_address), sizeof(server_address));
  if (rv) {
    util::abort("server bind()");
  }

  // listen
  rv = listen(sfd, SOMAXCONN);
  if (rv) {
    util::abort("server listen()");
  }

  std::cout << "Server started..." << std::endl;

  // TODO: using real map to replace
  std::vector<Conn*> fd2conn;

  fd_set_nb(sfd);

  std::vector<pollfd> poll_args;
  while (true) {
    poll_args.clear();
    // listening fd is put in the first position
    pollfd pfd = {sfd, POLLIN, 0};
    poll_args.push_back(pfd);
    // for connection fds
    for (auto* conn : fd2conn) {
      if (!conn) {
        continue;
      }
      pollfd pfd = {};
      pfd.fd = conn->fd;
      pfd.events = (conn->state == STATE_REQ) ? POLLIN : POLLOUT;
      pfd.events = pfd.events | POLLERR;
      poll_args.push_back(pfd);
    }

    // poll for active fds
    int rv = poll(poll_args.data(), poll_args.size(), 1000);
    if (rv < 0) {
      util::abort("poll");
    }

    // process active connections
    for (size_t i = 1; i < poll_args.size(); ++i) {
      if (poll_args[i].revents) {
        auto* conn = fd2conn[poll_args[i].fd];
        connection_io(conn);
        if (conn->state == STATE_END) {
          // client closed normally, or something bad happened.
          // destroy the connection
          fd2conn[conn->fd] = nullptr;
          close(conn->fd);
          free(conn);
        }
      }
    }
    
    // try to accept a new connection if the listening fd is active
    if (poll_args[0].revents) {
      accept_new_conn(fd2conn, sfd);
    }

  }

  return 0;
}

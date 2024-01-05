#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <functional>
#include <list>
#include <map>
#include <set>
#include <string>
#include "internal.h"

namespace miniredis {
namespace reactor {

class WriteBuf {
 public:
  bool Empty() const { return queue_.empty(); }

  bool DoWrite(int fd) {
    while (!Empty()) {
      auto const& s = queue_.front();
      const char* p = s.data() + head_;
      size_t len = s.size() - head_;

      while (len > 0) {
        ssize_t ret = write(fd, p, len);
        if (ret == -1) {
          if (errno == EAGAIN) {
            return true;
          }
          Log(Sprintf("write failed: %m"));
          return false;
        }
        head_ += (size_t)ret;
        p += ret;
        len -= (size_t)ret;
      }

      queue_.pop_front();
    }
    return true;
  }

  void AddData(const char* buf, size_t len) {
    if (queue_.empty() || queue_.back().size() + len > 4096) {
      queue_.emplace_back(buf, len);
    } else {
      queue_.back().append(buf, len);
    }
  }

 private:
  std::list<std::string> queue_;
  size_t head_;  // pointer to the first string
};

static const int kEpollEventCountMax = 1024;
static thread_local int ep_fd_ = -1;
// fd to callback function
static thread_local std::map<int, std::function<void(int)>> read_handlers;
static thread_local std::set<int> ready_read_fds_;
static thread_local std::map<int, WriteBuf> write_bufs;

void Init() {
  ep_fd_ = epoll_create1(0);
  if (ep_fd_ == -1) {
    Die(Sprintf("create epoll failed: %m"));
  }
}

void CloseFd(int fd);

bool RegisterPrepare(int fd) {
  // check if already registered
  if (read_handlers.find(fd) != read_handlers.end()) {
    return false;
  }

  // set unblocking
  int flags = 1;
  if (ioctl(fd, FIONBIO, &flags) == -1) {
    return false;
  }

  struct epoll_event ev;
  ev.events = EPOLLIN | EPOLLOUT | EPOLLET;  // et mode
  ev.data.fd = fd;
  if (epoll_ctl(ep_fd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
    return false;
  }

  return true;
}

bool RegisterListener(int fd, std::function<void(int)> on_new_conn) {
  if (!RegisterPrepare(fd)) {
    return false;
  }

  read_handlers[fd] = [on_new_conn](int listen_fd) {
    for (;;) {
      int conn_fd = accept(listen_fd, nullptr, nullptr);
      if (conn_fd == -1) {
        if (errno != EAGAIN) {
          Log(Sprintf("accept failed: %m"));
          CloseFd(listen_fd);
        }
        return;
      }

      int enable = 1;
      setsockopt(conn_fd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable));

      on_new_conn(conn_fd);
    }
  };

  return true;
}

bool RegisterEventFd(int fd, std::function<void(uint64_t)> on_ev) {
  if (!RegisterPrepare(fd)) {
    return false;
  }

  read_handlers[fd] = [on_ev](int ev_fd) {
    for (;;) {
      uint64_t count;
      if (read(ev_fd, &count, sizeof(count)) == -1) {
        if (errno != EAGAIN) {
          Log(Sprintf("read evfd failed: %m"));
          CloseFd(ev_fd);
        }
        return;
      }
      on_ev(count);
    }
  };

  return true;
}

bool RegisterConn(
    int fd, std::function<void(int, const char*, size_t)> on_data_received) {
  if (!RegisterPrepare(fd)) {
    return false;
  }

  read_handlers[fd] = [on_data_received](int conn_fd) {
    for (;;) {
      char buf[4096];
      ssize_t ret = read(conn_fd, buf, sizeof(buf));
      if (ret == -1) {
        if (errno != EAGAIN) {
          Log(Sprintf("read from conn failed: %m"));
          CloseFd(conn_fd);
        }
        return;
      }

      on_data_received(conn_fd, buf, (size_t)ret);
      if (ret == 0) {
        CloseFd(conn_fd);
        return;
      }

      ready_read_fds_.insert(conn_fd);
      return;
    }
  };

  return true;
}

void WriteConn(int fd, const char* buf, size_t len) {
  WriteBuf* wb = nullptr;
  auto iter = write_bufs.find(fd);
  if (iter == write_bufs.end()) {
    // no need to write into wb, so write directly
    while (len > 0) {
      ssize_t ret = write(fd, buf, len);
      if (ret == -1) {
        if (errno != EAGAIN) {
          Log(Sprintf("write failed: %m"));
          CloseFd(fd);
          return;
        }
        // stile get data to write, new buf
        wb = &write_bufs[fd];
        break;
      }
      buf += ret;
      len -= (size_t)ret;
    }

    if (len == 0) {
      // success
      return;
    }
  } else {
    wb = &iter->second;
  }
  wb->AddData(buf, len);
}

void CloseFd(int fd) {
  read_handlers.erase(fd);
  ready_read_fds_.erase(fd);
  write_bufs.erase(fd);
  close(fd);
}

void Start() {
  for (;;) {
    struct epoll_event evs[kEpollEventCountMax];
    int ev_count = epoll_wait(ep_fd_, evs, kEpollEventCountMax,
                              ready_read_fds_.empty() ? 100 : 0);
    if (ev_count == -1) {
      if (errno != EINTR) {
        Die(Sprintf("epoll wait failed: %m"));
      }
      ev_count = 0;
    }

    if (ev_count == 0 && !ready_read_fds_.empty()) {
      std::set<int> rrfs(std::move(ready_read_fds_));
      ready_read_fds_.clear();
      for (auto fd : rrfs) {
        auto h = read_handlers[fd];
        h(fd);
      }
    }

    for (int i = 0; i < ev_count; ++i) {
      const struct epoll_event& ev = evs[i];
      int fd = ev.data.fd;

      if (ev.events & (EPOLLIN | EPOLLERR | EPOLLHUP)) {
        // read event
        auto h = read_handlers[fd];
        h(fd);
      } else if (ev.events & (EPOLLOUT | EPOLLERR | EPOLLHUP)) {
        // write event
        auto iter = write_bufs.find(fd);
        if (iter != write_bufs.end()) {
          auto& wb = iter->second;
          if (!wb.DoWrite(fd)) {
            CloseFd(fd);
          } else if (wb.Empty()) {
            write_bufs.erase(iter);
          }
        }
      }
    }
  }
}

}  // namespace reactor

}  // namespace miniredis

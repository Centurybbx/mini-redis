#include "internal.h"

namespace miniredis {

void OnConnDataReceived(ReqParser::Ptr rp, int conn_fd, const char* data, size_t len) {
  if (len == 0) {
    return;
  }

  rp->Feed(data, len);
  for (;;) {
    std::vector<std::string> args;
    if (!rp->PopCmd(args)) {
      Log("bad redis req");
      reactor::CloseConn(conn_fd);
      return;
    }
    if (args.empty()) {
      return;
    }

    std::string resp;
    ProcCmd(args, resp);

    reactor::WriteConn(conn_fd, resp.data(), resp.size());
  }

}

class Worker {
 public:
  Worker(size_t idx) : idx_(idx) {
    new_conn_notify_fd_ = eventfd(0, 0);
    if (new_conn_notify_fd_ == -1) {
      Die(Sprintf("create eventfd failed: %m"));
    }

    std::thread(&Worker::ThreadMain, this).detach();
  }

  void DeliverConn(int conn_fd) {
    {
      std::lock_guard<std::mutex> lg(new_conn_queue_lock_);
      new_conns_.emplace_back(conn_fd);
    }
    uint64_t count = 1;
    if (write(new_conn_notify_fd_, &count, sizeof(count)) == -1) {
      Die(Sprintf("write to new_conn_notify_fd failed: %m"));
    }
  }

 private:
  size_t idx_;
  std::mutex new_conn_queue_lock_;
  std::vector<int> new_conns_;
  int new_conn_notify_fd_;

  void ThreadMain() {
    reactor::Init();

    if (!reactor::RegisterEventFd(new_conn_notify_fd_, [this](uint64_t) {
          std::vector<int> conns;
          {
            std::lock_guard<std::mutex> lg(new_conn_queue_lock_);
            conns = std::move(new_conns_);
            new_conns_.clear();
          }
          for (auto conn_fd : conns) {
            if (!reactor::RegisterConn(conn_fd, [rp = ReqParser::New()](int cfd, const char* data, size_t len) {
              OnConnDataReceived(rp, cfd, data, len);
            })) {
              Log("reg new conn failed");
              close(conn_fd);
            }
          }
        })) {
      Die("reg new conn_notify_fd_ to reactor failed");
    }

    reactor::Start();

    Die("bug");
  }
};

static std::vector<Worker*> workers;

void InitWorkers(size_t worker_count) {
  workers.resize(worker_count);
  for (size_t i = 0; i < worker_count; ++i) {
    workers[i] = new Worker(i);
  }
}

void DeliverConnToWorker(int conn_fd) {
  static thread_local size_t next_idx = 0;
  workers[next_idx % workers.size()]->DeliverConn(conn_fd);
  ++next_idx;
}

}  // namespace miniredis
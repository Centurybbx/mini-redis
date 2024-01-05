#include <signal.h>
#include <stddef.h>
#include <string.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace miniredis {

void InitWorkers(size_t worker_count);
void DeliverConnToWorker(int conn_fd);

int64_t NowMS();
void Die(const std::string& s);
std::string Sprintf(const char* format, ...);
void Log(const std::string& msg);
void StartSvr();

namespace reactor {
void Init();

bool RegisterListener(int fd, std::function<void(int)> on_new_conn);

bool RegisterEventFd(int fd, std::function<void(uint64_t)> on_ev);

bool RegisterConn(
    int fd, std::function<void(int, const char*, size_t)> on_data_received);

void WriteConn(int fd, const char* buf, size_t len);

void Start();

}  // namespace reactor

}  // namespace miniredis

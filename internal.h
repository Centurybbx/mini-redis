#include <signal.h>
#include <stddef.h>
#include <string.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <functional>
#include <mutex>
#include <memory>
#include <list>
#include <map>
#include <string>
#include <thread>
#include <vector>

namespace miniredis {

void InitWorkers(size_t worker_count);
void DeliverConnToWorker(int conn_fd);

int64_t NowMS();
void Die(const std::string& s);
std::string Sprintf(const char* format, ...);
bool ParseInt64(const std::string& s, int64_t& v);
void Log(const std::string& msg);
void StartSvr();

namespace reactor {
void Init();

bool RegisterListener(int fd, std::function<void(int)> on_new_conn);
bool RegisterEventFd(int fd, std::function<void(uint64_t)> on_ev);
bool RegisterConn(
    int fd, std::function<void(int, const char*, size_t)> on_data_received);

void WriteConn(int fd, const char* buf, size_t len);
void CloseConn(int fd);

void Start();

}  // namespace reactor

class ReqParser {
public:
    using Ptr = std::shared_ptr<ReqParser>;

    virtual void Feed(const char* data, size_t len) = 0;
    virtual bool PopCmd(std::vector<std::string>& args) = 0;

    static Ptr New();
};

class RObj {
public:
    using Ptr = std::shared_ptr<RObj>;

    virtual ~RObj() = 0;
};

void ProcCmd(const std::vector<std::string>& args, std::string& rsp);
void ProcCmdGet(const std::vector<std::string>& args, std::string& rsp);
void ProcCmdSet(const std::vector<std::string>& args, std::string& rsp);

namespace db {
    RObj::Ptr GetRObj(const std::string& k);
    void SetRObj(const std::string& k, RObj::Ptr robj);
}

}  // namespace miniredis

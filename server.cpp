
#include <netinet/in.h>
#include <sys/socket.h>

#include "internal.h"

namespace miniredis {

const uint16_t kPort = 6996;

void StartSvr() {
  int svr_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (svr_fd_ < 0) {
    Die(Sprintf("create socket failed: %m"));
  }

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(kPort);
  addr.sin_addr.s_addr = INADDR_ANY;

  int reuseaddr_on = 1;
  if (setsockopt(svr_fd_, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_on,
                 sizeof(reuseaddr_on)) == -1) {
    Die(Sprintf("setsockopt failed: %m"));
  }

  if (bind(svr_fd_, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
    Die(Sprintf("bind failed: %m"));
  }

  if (listen(svr_fd_, 1024) < 0) {
    Die(Sprintf("listen failed: %m"));
  }

  reactor::Init();

  if (!reactor::RegisterListener(
          svr_fd_, [](int conn_fd) { DeliverConnToWorker(conn_fd); })) {
    Die("reg listener to reactor failed");
  }

  reactor::Start();
}
}  // namespace miniredis

#pragma once

#include <unistd.h>
#include <cassert>
#include <string>
#include <iostream>
#include <cstdlib>

namespace util {
    int32_t read_full(int fd, char* buf, size_t n) {
        while (n > 0) {
            ssize_t rv = read(fd, buf, n);
            if (rv <= 0) {
                return -1;
            }
            assert(rv <= n);
            n -= rv;
            buf += rv;
        }
        return 0;
    }

    int32_t write_all(int fd, const char* buf, size_t n) {
        while (n > 0) {
            ssize_t rv = write(fd, buf, n);
            if (rv <= 0) {
                return -1;
            }
            assert(rv <= n);
            n -= rv;
            buf += rv;
        }
        return 0;
    }

    void error(std::string msg) {
        std::cerr << msg << std::endl;
    }

    void abort(std::string msg) {
        int err = errno;
        std::cerr << "err: " << errno << ", error msg: " << msg << std::endl;
        std::abort();
    }
}

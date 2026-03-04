#pragma once

#include <vector>
#include <string>
#include <string_view>
#include <mutex>
#include <thread>

class Logger {
public:
    Logger(size_t rows = 25, size_t stride = 128);
    ~Logger();

    void start();
    void stop();
    void addLog(const char* msg);
    std::string_view getBufferView() const;

private:
    std::string buffer_;
    size_t rows_, stride_;
    std::mutex mutex_;
    std::thread pipe_thread_;
    int pipefds[2];
    int original_stderr_fd = -1;
    bool running_ = false;
};

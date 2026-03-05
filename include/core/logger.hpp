#pragma once

#include <deque>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>

class Logger {
public:
    explicit Logger(size_t max_lines = 25);
    ~Logger();

    void start();
    void stop();

    std::deque<std::string> getLines();

    std::string_view getBufferView() const {
    return buffer_;
}

private:
    void pipeLoop();

    size_t max_lines_;
    std::deque<std::string> lines_;

    std::mutex mutex_;
    std::thread pipe_thread_;
    std::atomic<bool> running_{false};

    int pipefds_[2];
    int original_stderr_fd_ = -1;

    std::string buffer_;
    void rebuildBuffer();
};
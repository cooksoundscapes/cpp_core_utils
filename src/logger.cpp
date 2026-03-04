#include "core/logger.hpp"
#include <unistd.h>
#include <cstring>
#include <algorithm>
#include <cstdio>

Logger::Logger(size_t rows, size_t stride) 
    : rows_(rows), stride_(stride) {
    buffer_.resize(rows_ * stride_, ' ');
    for (size_t i = 1; i <= rows_; ++i) {
        buffer_[i * stride_ - 1] = '\n';
    }
}

Logger::~Logger() {
    stop();
}

void Logger::start() {
    if (running_) return;
    if (pipe(pipefds) == -1) return;

    original_stderr_fd = dup(STDERR_FILENO);
    dup2(pipefds[1], STDERR_FILENO);
    setvbuf(stderr, nullptr, _IONBF, 0);

    running_ = true;
    pipe_thread_ = std::thread([this]() {
        int fd = pipefds[0];
        char c;
        std::string current_line;
        current_line.reserve(stride_);

        while (read(fd, &c, 1) > 0) {
            if (c == '\n') {
                addLog(current_line.c_str());
                current_line.clear();
            } else {
                current_line += c;
                if (current_line.size() >= stride_ - 2) {
                    addLog(current_line.c_str());
                    current_line.clear();
                }
            }
        }
    });
}

void Logger::stop() {
    if (!running_) return;
    running_ = false;

    if (original_stderr_fd != -1) {
        dup2(original_stderr_fd, STDERR_FILENO);
        close(original_stderr_fd);
        original_stderr_fd = -1;
    }

    close(pipefds[1]); 
    if (pipe_thread_.joinable()) pipe_thread_.join();
    close(pipefds[0]);
}

void Logger::addLog(const char* msg) {
    if (!msg || std::strlen(msg) == 0) return;
    std::lock_guard<std::mutex> lock(mutex_);

    char* base_ptr = buffer_.data();
    std::memmove(base_ptr, base_ptr + stride_, (rows_ - 1) * stride_);

    char* last_line_ptr = base_ptr + (rows_ - 1) * stride_;
    std::memset(last_line_ptr, ' ', stride_ - 1);
    
    size_t len = std::min(std::strlen(msg), stride_ - 2);
    std::memcpy(last_line_ptr, msg, len);
}

std::string_view Logger::getBufferView() const {
    return buffer_;
}
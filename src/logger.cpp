#include "core/logger.hpp"

#include <unistd.h>
#include <cstdio>
#include <cstring>

Logger::Logger(size_t max_lines)
    : max_lines_(max_lines)
{
}

Logger::~Logger() {
    stop();
}

void Logger::start() {
    if (running_)
        return;

    pipe(pipefds_);

    original_stderr_fd_ = dup(STDERR_FILENO);
    dup2(pipefds_[1], STDERR_FILENO);

    setvbuf(stderr, nullptr, _IONBF, 0);

    running_ = true;
    pipe_thread_ = std::thread(&Logger::pipeLoop, this);
}

void Logger::stop() {
    if (!running_)
        return;

    running_ = false;

    dup2(original_stderr_fd_, STDERR_FILENO);
    close(original_stderr_fd_);

    close(pipefds_[1]);

    if (pipe_thread_.joinable())
        pipe_thread_.join();

    close(pipefds_[0]);
}

void Logger::pipeLoop() {
    char buffer[256];

    std::string partial;

    while (running_) {
        ssize_t n = read(pipefds_[0], buffer, sizeof(buffer));

        if (n <= 0)
            continue;

        partial.append(buffer, n);

        size_t pos;
        while ((pos = partial.find('\n')) != std::string::npos) {
            std::string line = partial.substr(0, pos);
            partial.erase(0, pos + 1);

            std::lock_guard<std::mutex> lock(mutex_);

            lines_.push_back(line);

            if (lines_.size() > max_lines_)
                lines_.pop_front();

            rebuildBuffer();
        }
    }
}

std::deque<std::string> Logger::getLines() {
    std::lock_guard<std::mutex> lock(mutex_);
    return lines_;
}

void Logger::rebuildBuffer() {
    buffer_.clear();

    for (auto& l : lines_) {
        buffer_ += l;
        buffer_ += '\n';
    }
}
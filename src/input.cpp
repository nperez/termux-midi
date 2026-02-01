#include "input.h"
#include "synth.h"
#include <cstdio>
#include <cstring>
#include <sstream>
#include <poll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <errno.h>

InputHandler::InputHandler(Synthesizer& synth)
    : synth_(synth) {
}

InputHandler::~InputHandler() {
    stop();
}

void InputHandler::stop() {
    running_.store(false);

    if (inputThread_.joinable()) {
        inputThread_.join();
    }

    cleanup();
}

void InputHandler::cleanup() {
    if (socketFd_ >= 0) {
        close(socketFd_);
        socketFd_ = -1;
    }

    if (!socketPath_.empty()) {
        unlink(socketPath_.c_str());
        socketPath_.clear();
    }
}

void InputHandler::startStdin(QuitCallback onQuit) {
    if (running_.load()) {
        return;
    }

    quitCallback_ = std::move(onQuit);
    running_.store(true);
    inputThread_ = std::thread(&InputHandler::stdinLoop, this);
}

bool InputHandler::startSocket(const std::string& path, QuitCallback onQuit) {
    if (running_.load()) {
        return false;
    }

    // Create Unix domain socket
    socketFd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socketFd_ < 0) {
        std::fprintf(stderr, "Failed to create socket: %s\n", strerror(errno));
        return false;
    }

    // Remove existing socket file
    unlink(path.c_str());

    // Bind socket
    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(socketFd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::fprintf(stderr, "Failed to bind socket: %s\n", strerror(errno));
        close(socketFd_);
        socketFd_ = -1;
        return false;
    }

    // Listen for connections
    if (listen(socketFd_, 5) < 0) {
        std::fprintf(stderr, "Failed to listen on socket: %s\n", strerror(errno));
        close(socketFd_);
        socketFd_ = -1;
        return false;
    }

    socketPath_ = path;
    quitCallback_ = std::move(onQuit);
    running_.store(true);
    inputThread_ = std::thread(&InputHandler::socketLoop, this);

    std::printf("Listening on socket: %s\n", path.c_str());
    return true;
}

void InputHandler::stdinLoop() {
    char buffer[1024];

    // Set stdin to non-blocking for poll
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    while (running_.load()) {
        struct pollfd pfd;
        pfd.fd = STDIN_FILENO;
        pfd.events = POLLIN;

        int ret = poll(&pfd, 1, 100);  // 100ms timeout
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (ret > 0 && (pfd.revents & POLLIN)) {
            if (std::fgets(buffer, sizeof(buffer), stdin)) {
                // Remove trailing newline
                size_t len = std::strlen(buffer);
                if (len > 0 && buffer[len - 1] == '\n') {
                    buffer[len - 1] = '\0';
                }

                if (!processCommand(buffer)) {
                    break;
                }
            } else if (std::feof(stdin)) {
                break;
            }
        }
    }

    // Restore blocking mode
    fcntl(STDIN_FILENO, F_SETFL, flags);

    running_.store(false);
    if (quitCallback_) {
        quitCallback_();
    }
}

void InputHandler::socketLoop() {
    while (running_.load()) {
        struct pollfd pfd;
        pfd.fd = socketFd_;
        pfd.events = POLLIN;

        int ret = poll(&pfd, 1, 100);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (ret > 0 && (pfd.revents & POLLIN)) {
            int clientFd = accept(socketFd_, nullptr, nullptr);
            if (clientFd < 0) {
                continue;
            }

            std::printf("Client connected\n");

            // Handle client
            char buffer[1024];
            FILE* clientFile = fdopen(clientFd, "r");
            if (clientFile) {
                while (running_.load() && std::fgets(buffer, sizeof(buffer), clientFile)) {
                    size_t len = std::strlen(buffer);
                    if (len > 0 && buffer[len - 1] == '\n') {
                        buffer[len - 1] = '\0';
                    }

                    if (!processCommand(buffer)) {
                        break;
                    }
                }
                std::fclose(clientFile);
            } else {
                close(clientFd);
            }

            std::printf("Client disconnected\n");
        }
    }

    running_.store(false);
    if (quitCallback_) {
        quitCallback_();
    }
}

bool InputHandler::processCommand(const std::string& line) {
    if (line.empty()) {
        return true;
    }

    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    if (cmd == "quit" || cmd == "exit") {
        return false;
    }
    else if (cmd == "noteon") {
        int channel, note, velocity;
        if (iss >> channel >> note >> velocity) {
            synth_.noteOn(channel, note, velocity / 127.0f);
        } else {
            std::fprintf(stderr, "Usage: noteon <channel> <note> <velocity>\n");
        }
    }
    else if (cmd == "noteoff") {
        int channel, note;
        if (iss >> channel >> note) {
            synth_.noteOff(channel, note);
        } else {
            std::fprintf(stderr, "Usage: noteoff <channel> <note>\n");
        }
    }
    else if (cmd == "cc") {
        int channel, controller, value;
        if (iss >> channel >> controller >> value) {
            synth_.controlChange(channel, controller, value);
        } else {
            std::fprintf(stderr, "Usage: cc <channel> <controller> <value>\n");
        }
    }
    else if (cmd == "pc") {
        int channel, program;
        if (iss >> channel >> program) {
            synth_.programChange(channel, program);
        } else {
            std::fprintf(stderr, "Usage: pc <channel> <program>\n");
        }
    }
    else if (cmd == "pitch") {
        int channel, value;
        if (iss >> channel >> value) {
            synth_.pitchBend(channel, value);
        } else {
            std::fprintf(stderr, "Usage: pitch <channel> <value>\n");
        }
    }
    else if (cmd == "panic") {
        synth_.allNotesOff();
    }
    else if (cmd == "sleep") {
        // Sleep command for scripting (in seconds)
        double seconds;
        if (iss >> seconds) {
            usleep(static_cast<useconds_t>(seconds * 1000000));
        }
    }
    else {
        std::fprintf(stderr, "Unknown command: %s\n", cmd.c_str());
    }

    return true;
}

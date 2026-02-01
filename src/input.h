#ifndef INPUT_H
#define INPUT_H

#include <string>
#include <atomic>
#include <thread>
#include <functional>

class Synthesizer;

class InputHandler {
public:
    // Callback for quit command
    using QuitCallback = std::function<void()>;

    InputHandler(Synthesizer& synth);
    ~InputHandler();

    // Start reading from stdin
    void startStdin(QuitCallback onQuit = nullptr);

    // Start listening on Unix socket
    bool startSocket(const std::string& path, QuitCallback onQuit = nullptr);

    // Stop input handling
    void stop();

    // Check if running
    bool isRunning() const { return running_.load(); }

    // Process a single command line (returns false on quit)
    bool processCommand(const std::string& line);

private:
    Synthesizer& synth_;
    std::atomic<bool> running_{false};
    std::thread inputThread_;
    QuitCallback quitCallback_;
    int socketFd_ = -1;
    std::string socketPath_;

    void stdinLoop();
    void socketLoop();
    void cleanup();
};

#endif // INPUT_H

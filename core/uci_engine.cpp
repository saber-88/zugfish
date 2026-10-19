#include "uci_engine.hpp"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace zugfish {

std::string Score::to_string() const {
    if (is_mate) {
        if (std::abs(value) == 100000 || value == 0) {
            return (value >= 0) ? "#" : "-#";
        }
        return (value > 0 ? "#" : "-#") + std::to_string(std::abs(value));
    }
    std::ostringstream ss;
    if (value >= 0) ss << "+";
    ss << std::fixed << std::setprecision(2) << (value / 100.0);
    return ss.str();
}

Score Score::to_white_perspective(bool white_to_move) const {
    Score s = *this;
    if (s.is_mate && s.value == 0) {
        // If the position has mate 0, the side whose turn it is has been checkmated.
        // If White was to move, White was checkmated (-100000).
        // If Black was to move, Black was checkmated (+100000).
        s.value = white_to_move ? -100000 : 100000;
        return s;
    }
    if (!white_to_move) {
        s.value = -s.value;
    }
    return s;
}

double Score::win_probability(bool white_to_move) const {
    Score s = to_white_perspective(white_to_move);
    if (s.is_mate) {
        return s.value > 0 ? 1.0 : 0.0;
    }
    // Standard logistic curve used by Lichess
    return 1.0 / (1.0 + std::exp(-0.00368208 * s.value));
}

UciEngine::UciEngine(const std::string& engine_path)
    : engine_path_(engine_path) {
    start();
}

UciEngine::~UciEngine() {
    stop();
}

UciEngine::UciEngine(UciEngine&& other) noexcept
    : engine_path_(std::move(other.engine_path_))
#ifdef _WIN32
    , process_handle_(other.process_handle_)
    , write_handle_(other.write_handle_)
    , read_handle_(other.read_handle_)
#else
    , pid_(other.pid_)
    , write_fd_(other.write_fd_)
    , read_fd_(other.read_fd_)
    , read_file_(other.read_file_)
#endif
{
#ifdef _WIN32
    other.process_handle_ = NULL;
    other.write_handle_ = NULL;
    other.read_handle_ = NULL;
#else
    other.pid_ = -1;
    other.write_fd_ = -1;
    other.read_fd_ = -1;
    other.read_file_ = nullptr;
#endif
}

UciEngine& UciEngine::operator=(UciEngine&& other) noexcept {
    if (this != &other) {
        stop();
        engine_path_ = std::move(other.engine_path_);
#ifdef _WIN32
        process_handle_ = other.process_handle_;
        write_handle_ = other.write_handle_;
        read_handle_ = other.read_handle_;

        other.process_handle_ = NULL;
        other.write_handle_ = NULL;
        other.read_handle_ = NULL;
#else
        pid_ = other.pid_;
        write_fd_ = other.write_fd_;
        read_fd_ = other.read_fd_;
        read_file_ = other.read_file_;

        other.pid_ = -1;
        other.write_fd_ = -1;
        other.read_fd_ = -1;
        other.read_file_ = nullptr;
#endif
    }
    return *this;
}

bool UciEngine::is_alive() const {
#ifdef _WIN32
    if (!process_handle_) return false;
    DWORD exit_code = 0;
    if (GetExitCodeProcess(process_handle_, &exit_code)) {
        return exit_code == STILL_ACTIVE;
    }
    return false;
#else
    if (pid_ <= 0) return false;
    int status = 0;
    pid_t result = waitpid(pid_, &status, WNOHANG);
    return result == 0;
#endif
}

void UciEngine::start() {
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE child_stdin_read = NULL;
    HANDLE child_stdin_write = NULL;
    HANDLE child_stdout_read = NULL;
    HANDLE child_stdout_write = NULL;

    if (!CreatePipe(&child_stdin_read, &child_stdin_write, &sa, 0)) {
        throw std::runtime_error("Failed to create stdin pipe for UCI engine");
    }
    SetHandleInformation(child_stdin_write, HANDLE_FLAG_INHERIT, 0);

    if (!CreatePipe(&child_stdout_read, &child_stdout_write, &sa, 0)) {
        CloseHandle(child_stdin_read);
        CloseHandle(child_stdin_write);
        throw std::runtime_error("Failed to create stdout pipe for UCI engine");
    }
    SetHandleInformation(child_stdout_read, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdInput = child_stdin_read;
    si.hStdOutput = child_stdout_write;
    si.hStdError = NULL;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    std::string cmd = engine_path_;
    BOOL success = CreateProcessA(
        NULL,
        cmd.data(),
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &si,
        &pi
    );

    CloseHandle(child_stdin_read);
    CloseHandle(child_stdout_write);

    if (!success) {
        CloseHandle(child_stdin_write);
        CloseHandle(child_stdout_read);
        throw std::runtime_error("Failed to start UCI engine process: " + engine_path_);
    }

    CloseHandle(pi.hThread);
    process_handle_ = pi.hProcess;
    write_handle_ = child_stdin_write;
    read_handle_ = child_stdout_read;
#else
    int parent_to_child[2];
    int child_to_parent[2];

    if (pipe(parent_to_child) != 0 || pipe(child_to_parent) != 0) {
        throw std::runtime_error("Failed to create pipes for UCI engine");
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(parent_to_child[0]); close(parent_to_child[1]);
        close(child_to_parent[0]); close(child_to_parent[1]);
        throw std::runtime_error("Failed to fork process for UCI engine");
    }

    if (pid == 0) {
        // In child process
        dup2(parent_to_child[0], STDIN_FILENO);
        dup2(child_to_parent[1], STDOUT_FILENO);

        close(parent_to_child[0]);
        close(parent_to_child[1]);
        close(child_to_parent[0]);
        close(child_to_parent[1]);

        // Suppress stderr to keep terminal clean
        int dev_null = open("/dev/null", O_WRONLY);
        if (dev_null != -1) {
            dup2(dev_null, STDERR_FILENO);
            close(dev_null);
        }

        execlp(engine_path_.c_str(), engine_path_.c_str(), nullptr);
        _exit(127);
    }

    // In parent process
    close(parent_to_child[0]);
    close(child_to_parent[1]);

    pid_ = pid;
    write_fd_ = parent_to_child[1];
    read_fd_ = child_to_parent[0];
    read_file_ = fdopen(read_fd_, "r");
    if (!read_file_) {
        stop();
        throw std::runtime_error("Failed to open file stream for UCI engine output");
    }
#endif

    // Handshake
    send_command("uci");
    while (is_alive()) {
        std::string line = read_line();
        if (line == "uciok") {
            break;
        }
    }
    wait_ready();
}

void UciEngine::stop() {
#ifdef _WIN32
    if (process_handle_) {
        if (is_alive()) {
            send_command("quit");
            WaitForSingleObject(process_handle_, 100);
        }
        if (is_alive()) {
            TerminateProcess(process_handle_, 0);
            WaitForSingleObject(process_handle_, 500);
        }
        CloseHandle(process_handle_);
        process_handle_ = NULL;
    }
    if (write_handle_) {
        CloseHandle(write_handle_);
        write_handle_ = NULL;
    }
    if (read_handle_) {
        CloseHandle(read_handle_);
        read_handle_ = NULL;
    }
#else
    if (pid_ > 0) {
        if (is_alive()) {
            send_command("quit");
        }
        if (read_file_) {
            fclose(read_file_);
            read_file_ = nullptr;
            read_fd_ = -1;
        } else if (read_fd_ != -1) {
            close(read_fd_);
            read_fd_ = -1;
        }
        if (write_fd_ != -1) {
            close(write_fd_);
            write_fd_ = -1;
        }

        int status = 0;
        int ret = waitpid(pid_, &status, WNOHANG);
        if (ret == 0) {
            usleep(100000); // 100ms
            ret = waitpid(pid_, &status, WNOHANG);
            if (ret == 0) {
                kill(pid_, SIGKILL);
                waitpid(pid_, &status, 0);
            }
        }
        pid_ = -1;
    }
#endif
}

void UciEngine::send_command(const std::string& command) {
#ifdef _WIN32
    if (!write_handle_) return;
    std::string line = command + "\n";
    DWORD written = 0;
    WriteFile(write_handle_, line.data(), static_cast<DWORD>(line.size()), &written, NULL);
#else
    if (write_fd_ < 0) return;
    std::string line = command + "\n";
    ssize_t written = write(write_fd_, line.data(), line.size());
    (void)written;
#endif
}

std::string UciEngine::read_line() {
#ifdef _WIN32
    if (!read_handle_) return "";
    std::string line;
    char c = 0;
    DWORD bytes_read = 0;
    while (ReadFile(read_handle_, &c, 1, &bytes_read, NULL) && bytes_read > 0) {
        if (c == '\n') break;
        if (c != '\r') {
            line.push_back(c);
        }
    }
    return line;
#else
    if (!read_file_) return "";
    char* line_buf = nullptr;
    size_t len = 0;
    ssize_t read_bytes = getline(&line_buf, &len, read_file_);
    if (read_bytes == -1) {
        free(line_buf);
        return "";
    }
    std::string line(line_buf);
    free(line_buf);

    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
        line.pop_back();
    }
    return line;
#endif
}

void UciEngine::wait_ready() {
    send_command("isready");
    while (is_alive()) {
        std::string line = read_line();
        if (line == "readyok") {
            break;
        }
    }
}

void UciEngine::new_game() {
    send_command("ucinewgame");
    wait_ready();
}

void UciEngine::set_position_fen(const std::string& fen) {
    send_command("position fen " + fen);
}

EngineEvaluation UciEngine::analyze_depth(int depth) {
    send_command("go depth " + std::to_string(depth));
    return parse_search_output();
}

EngineEvaluation UciEngine::analyze_movetime(int ms) {
    send_command("go movetime " + std::to_string(ms));
    return parse_search_output();
}

EngineEvaluation UciEngine::parse_search_output() {
    EngineEvaluation eval;
    while (is_alive()) {
        std::string line = read_line();
        if (line.rfind("info ", 0) == 0) {
            std::istringstream iss(line);
            std::string token;
            iss >> token; // skip "info"
            while (iss >> token) {
                if (token == "depth") {
                    iss >> eval.depth;
                } else if (token == "seldepth") {
                    iss >> eval.seldepth;
                } else if (token == "score") {
                    std::string type;
                    iss >> type;
                    if (type == "cp") {
                        eval.score.is_mate = false;
                        iss >> eval.score.value;
                    } else if (type == "mate") {
                        eval.score.is_mate = true;
                        iss >> eval.score.value;
                    }
                } else if (token == "nodes") {
                    iss >> eval.nodes;
                } else if (token == "nps") {
                    iss >> eval.nps;
                } else if (token == "time") {
                    iss >> eval.time_ms;
                } else if (token == "pv") {
                    eval.pv.clear();
                    std::string pv_move;
                    while (iss >> pv_move) {
                        eval.pv.push_back(pv_move);
                    }
                }
            }
        } else if (line.rfind("bestmove ", 0) == 0) {
            std::istringstream iss(line);
            std::string token;
            iss >> token; // skip "bestmove"
            iss >> eval.best_move;
            if (iss >> token && token == "ponder") {
                iss >> eval.ponder_move;
            }
            break;
        }
    }
    return eval;
}

} // namespace zugfish

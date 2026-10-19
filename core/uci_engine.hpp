#pragma once

#include <cstdint>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/types.h>
#endif

namespace zugfish {

struct Score {
    bool is_mate = false;
    int value = 0; // centipawns if !is_mate; moves to mate if is_mate (e.g. +1, -3)

    // Formatted string: "+0.35", "-1.20", "#1", "-#3"
    std::string to_string() const;

    // Normalized from White's perspective (+ = White advantage, - = Black advantage)
    Score to_white_perspective(bool white_to_move) const;

    // Approximate winning probability [0.0, 1.0] from White's perspective
    double win_probability(bool white_to_move) const;
};

struct EngineEvaluation {
    int depth = 0;
    int seldepth = 0;
    Score score;
    std::string best_move;
    std::string ponder_move;
    std::vector<std::string> pv;
    uint64_t nodes = 0;
    uint64_t nps = 0;
    uint64_t time_ms = 0;
};

class UciEngine {
public:
#ifdef _WIN32
    explicit UciEngine(const std::string& engine_path = "stockfish.exe");
#else
    explicit UciEngine(const std::string& engine_path = "/usr/sbin/stockfish");
#endif
    ~UciEngine();

    // Non-copyable
    UciEngine(const UciEngine&) = delete;
    UciEngine& operator=(const UciEngine&) = delete;

    // Movable
    UciEngine(UciEngine&& other) noexcept;
    UciEngine& operator=(UciEngine&& other) noexcept;

    bool is_alive() const;

    void send_command(const std::string& command);
    std::string read_line();

    void wait_ready();
    void new_game();
    void set_position_fen(const std::string& fen);

    // Synchronous evaluation
    EngineEvaluation analyze_depth(int depth);
    EngineEvaluation analyze_movetime(int ms);

private:
    void start();
    void stop();
    EngineEvaluation parse_search_output();

    std::string engine_path_;
#ifdef _WIN32
    HANDLE process_handle_ = NULL;
    HANDLE write_handle_ = NULL;
    HANDLE read_handle_ = NULL;
#else
    pid_t pid_ = -1;
    int write_fd_ = -1;
    int read_fd_ = -1;
    FILE* read_file_ = nullptr;
#endif
};

} // namespace zugfish

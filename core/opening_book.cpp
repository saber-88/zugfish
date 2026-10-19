#include "opening_book.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace zugfish {

namespace fs = std::filesystem;

OpeningBook& OpeningBook::instance() {
    static OpeningBook book;
    return book;
}

OpeningBook::OpeningBook()
    : root_(std::make_unique<Node>()) {
    root_->opening = {"A00", "Undetermined Opening"};
    load_openings();
}

void OpeningBook::add_line(const std::string& eco, const std::string& name, const std::vector<std::string>& uci_moves) {
    if (uci_moves.empty()) return;
    Node* current = root_.get();
    for (const auto& uci : uci_moves) {
        auto it = current->children.find(uci);
        if (it == current->children.end()) {
            auto new_node = std::make_unique<Node>();
            new_node->uci = uci;
            new_node->opening = {eco, name};
            Node* raw_ptr = new_node.get();
            current->children[uci] = std::move(new_node);
            current = raw_ptr;
        } else {
            current = it->second.get();
            // Update with more specific variation name
            current->opening = {eco, name};
        }
    }
    eco_count_++;
}

bool OpeningBook::is_book_move(const chess::Board& board, const std::vector<std::string>& uci_history, const std::string& next_uci) const {
    // 1. Check grandmaster tournament Polyglot book
    if (polyglot_.is_loaded()) {
        if (polyglot_.is_book_move(board, next_uci)) {
            return true;
        }
    }

    // 2. Check exhaustive ECO trie
    const Node* current = root_.get();
    for (const auto& uci : uci_history) {
        auto it = current->children.find(uci);
        if (it == current->children.end()) {
            return false;
        }
        current = it->second.get();
    }
    return current->children.find(next_uci) != current->children.end();
}

bool OpeningBook::is_book_move(const std::vector<std::string>& uci_history, const std::string& next_uci) const {
    // Reconstruct board if board was not passed
    chess::Board board;
    for (const auto& u : uci_history) {
        try {
            chess::Move m = chess::uci::uciToMove(board, u);
            board.makeMove(m);
        } catch (...) {
            break;
        }
    }
    return is_book_move(board, uci_history, next_uci);
}

OpeningInfo OpeningBook::identify_opening(const std::vector<std::string>& uci_history) const {
    const Node* current = root_.get();
    OpeningInfo best_match = {"A00", "Custom Opening"};
    for (const auto& uci : uci_history) {
        auto it = current->children.find(uci);
        if (it == current->children.end()) {
            break;
        }
        current = it->second.get();
        if (!current->opening.eco.empty() && current->opening.eco != "A00") {
            best_match = current->opening;
        } else if (best_match.eco == "A00" && !current->opening.name.empty()) {
            best_match = current->opening;
        }
    }
    return best_match;
}

static std::string find_file(const std::string& filename) {
    const std::vector<std::string> candidates = {
        "data/" + filename,
        "../data/" + filename,
        "../../data/" + filename,
        "/home/karmveer/.coding/zugfish/data/" + filename,
        "/usr/local/share/zugfish/" + filename,
        "/usr/share/zugfish/" + filename
    };
    for (const auto& path : candidates) {
        if (fs::exists(path)) {
            return path;
        }
    }
    return "";
}

void OpeningBook::load_openings() {
    // 1. e4 Openings
    add_line("B00", "King's Pawn Opening", {"e2e4"});

    // 1. e4 e5
    add_line("C20", "King's Pawn Game: Open Game", {"e2e4", "e7e5"});
    add_line("C40", "King's Knight Opening", {"e2e4", "e7e5", "g1f3"});
    add_line("C42", "Petrov's Defense", {"e2e4", "e7e5", "g1f3", "g8f6"});
    add_line("C41", "Philidor Defense", {"e2e4", "e7e5", "g1f3", "d7d6"});
    add_line("C41", "Philidor Defense: Exchange Variation", {"e2e4", "e7e5", "g1f3", "d7d6", "d2d4", "b8d7"});
    add_line("C41", "Philidor Defense: Morphy Variation", {"e2e4", "e7e5", "g1f3", "d7d6", "d2d4", "c8g4"});
    add_line("C44", "Scotch Game", {"e2e4", "e7e5", "g1f3", "b8c6", "d2d4"});
    add_line("C45", "Scotch Game: Classical Variation", {"e2e4", "e7e5", "g1f3", "b8c6", "d2d4", "e5d4", "f3d4", "f8c5"});
    add_line("C45", "Scotch Game: Schmidt Variation", {"e2e4", "e7e5", "g1f3", "b8c6", "d2d4", "e5d4", "f3d4", "g8f6"});
    add_line("C46", "Three Knights Opening", {"e2e4", "e7e5", "g1f3", "b8c6", "b1c3"});
    add_line("C47", "Four Knights Game", {"e2e4", "e7e5", "g1f3", "b8c6", "b1c3", "g8f6"});
    add_line("C48", "Four Knights Game: Spanish Variation", {"e2e4", "e7e5", "g1f3", "b8c6", "b1c3", "g8f6", "f1b5"});
    add_line("C49", "Four Knights Game: Double Ruy Lopez", {"e2e4", "e7e5", "g1f3", "b8c6", "b1c3", "g8f6", "f1b5", "f8b4"});

    // Italian Game
    add_line("C50", "Italian Game", {"e2e4", "e7e5", "g1f3", "b8c6", "f1c4"});
    add_line("C50", "Italian Game: Hungarian Defense", {"e2e4", "e7e5", "g1f3", "b8c6", "f1c4", "f8e7"});
    add_line("C53", "Italian Game: Giuoco Piano", {"e2e4", "e7e5", "g1f3", "b8c6", "f1c4", "f8c5", "c2c3"});
    add_line("C54", "Italian Game: Giuoco Pianissimo", {"e2e4", "e7e5", "g1f3", "b8c6", "f1c4", "f8c5", "d2d3"});
    add_line("C51", "Evans Gambit", {"e2e4", "e7e5", "g1f3", "b8c6", "f1c4", "f8c5", "b2b4"});
    add_line("C55", "Two Knights Defense", {"e2e4", "e7e5", "g1f3", "b8c6", "f1c4", "g8f6"});
    add_line("C57", "Two Knights Defense: Fried Liver Attack", {"e2e4", "e7e5", "g1f3", "b8c6", "f1c4", "g8f6", "f3g5", "d7d5", "e4d5", "f6d5", "g5f7"});

    // Ruy Lopez
    add_line("C60", "Ruy Lopez", {"e2e4", "e7e5", "g1f3", "b8c6", "f1b5"});
    add_line("C65", "Ruy Lopez: Berlin Defense", {"e2e4", "e7e5", "g1f3", "b8c6", "f1b5", "g8f6"});
    add_line("C68", "Ruy Lopez: Exchange Variation", {"e2e4", "e7e5", "g1f3", "b8c6", "f1b5", "a7a6", "b5c6"});
    add_line("C70", "Ruy Lopez: Morphy Defense", {"e2e4", "e7e5", "g1f3", "b8c6", "f1b5", "a7a6", "f1a4"});
    add_line("C78", "Ruy Lopez: Closed", {"e2e4", "e7e5", "g1f3", "b8c6", "f1b5", "a7a6", "f1a4", "g8f6", "e1g1", "f8e7"});
    add_line("C88", "Ruy Lopez: Marshall Attack", {"e2e4", "e7e5", "g1f3", "b8c6", "f1b5", "a7a6", "f1a4", "g8f6", "e1g1", "f8e7", "f1e1", "b7b5", "a4b3", "e1g1", "c2c3", "d7d5"});

    // King's Gambit & Vienna
    add_line("C30", "King's Gambit", {"e2e4", "e7e5", "f2f4"});
    add_line("C33", "King's Gambit Accepted", {"e2e4", "e7e5", "f2f4", "e5f4"});
    add_line("C31", "King's Gambit Declined: Falkbeer Countergambit", {"e2e4", "e7e5", "f2f4", "d7d5"});
    add_line("C23", "Bishop's Opening", {"e2e4", "e7e5", "f1c4"});
    add_line("C25", "Vienna Game", {"e2e4", "e7e5", "b1c3"});

    // Sicilian Defense
    add_line("B20", "Sicilian Defense", {"e2e4", "c7c5"});
    add_line("B21", "Sicilian Defense: Smith-Morra Gambit", {"e2e4", "c7c5", "d2d4", "c5d4", "c2c3"});
    add_line("B22", "Sicilian Defense: Alapin Variation", {"e2e4", "c7c5", "c2c3"});
    add_line("B23", "Sicilian Defense: Closed", {"e2e4", "c7c5", "b1c3"});
    add_line("B27", "Sicilian Defense: Open", {"e2e4", "c7c5", "g1f3"});
    add_line("B30", "Sicilian Defense: Old Sicilian", {"e2e4", "c7c5", "g1f3", "b8c6"});
    add_line("B33", "Sicilian Defense: Sveshnikov Variation", {"e2e4", "c7c5", "g1f3", "b8c6", "d2d4", "c5d4", "f3d4", "g8f6", "b1c3", "e7e5"});
    add_line("B40", "Sicilian Defense: French Variation", {"e2e4", "c7c5", "g1f3", "e7e6"});
    add_line("B41", "Sicilian Defense: Kan Variation", {"e2e4", "c7c5", "g1f3", "e7e6", "d2d4", "c5d4", "f3d4", "a7a6"});
    add_line("B50", "Sicilian Defense: Modern Variations", {"e2e4", "c7c5", "g1f3", "d7d6"});
    add_line("B51", "Sicilian Defense: Moscow Variation", {"e2e4", "c7c5", "g1f3", "d7d6", "f1b5+"});
    add_line("B70", "Sicilian Defense: Dragon Variation", {"e2e4", "c7c5", "g1f3", "d7d6", "d2d4", "c5d4", "f3d4", "g8f6", "b1c3", "g7g6"});
    add_line("B75", "Sicilian Defense: Yugoslav Attack", {"e2e4", "c7c5", "g1f3", "d7d6", "d2d4", "c5d4", "f3d4", "g8f6", "b1c3", "g7g6", "c1e3", "f8g7", "f2f3"});
    add_line("B80", "Sicilian Defense: Scheveningen Variation", {"e2e4", "c7c5", "g1f3", "d7d6", "d2d4", "c5d4", "f3d4", "g8f6", "b1c3", "e7e6"});
    add_line("B90", "Sicilian Defense: Najdorf Variation", {"e2e4", "c7c5", "g1f3", "d7d6", "d2d4", "c5d4", "f3d4", "g8f6", "b1c3", "a7a6"});

    // French Defense
    add_line("C00", "French Defense", {"e2e4", "e7e6"});
    add_line("C01", "French Defense: Exchange Variation", {"e2e4", "e7e6", "d2d4", "d7d5", "e4d5", "e6d5"});
    add_line("C02", "French Defense: Advance Variation", {"e2e4", "e7e6", "d2d4", "d7d5", "e4e5"});
    add_line("C03", "French Defense: Tarrasch Variation", {"e2e4", "e7e6", "d2d4", "d7d5", "b1d2"});
    add_line("C10", "French Defense: Classical", {"e2e4", "e7e6", "d2d4", "d7d5", "b1c3", "g8f6"});
    add_line("C15", "French Defense: Winawer Variation", {"e2e4", "e7e6", "d2d4", "d7d5", "b1c3", "f1b4"});

    // Caro-Kann Defense
    add_line("B10", "Caro-Kann Defense", {"e2e4", "c7c6"});
    add_line("B12", "Caro-Kann Defense: Advance Variation", {"e2e4", "c7c6", "d2d4", "d7d5", "e4e5"});
    add_line("B13", "Caro-Kann Defense: Exchange Variation", {"e2e4", "c7c6", "d2d4", "d7d5", "e4d5", "c6d5"});
    add_line("B14", "Caro-Kann Defense: Panov Attack", {"e2e4", "c7c6", "d2d4", "d7d5", "e4d5", "c6d5", "c2c4"});
    add_line("B15", "Caro-Kann Defense: Classical Variation", {"e2e4", "c7c6", "d2d4", "d7d5", "b1c3", "d5e4", "c3e4"});

    // Other 1. e4
    add_line("B01", "Scandinavian Defense", {"e2e4", "d7d5"});
    add_line("B01", "Scandinavian Defense: Main Line", {"e2e4", "d7d5", "e4d5", "d8d5", "b1c3", "d5a5"});
    add_line("B02", "Alekhine's Defense", {"e2e4", "g8f6"});
    add_line("B06", "Modern Defense", {"e2e4", "g7g6"});
    add_line("B07", "Pirc Defense", {"e2e4", "d7d6", "d2d4", "g8f6", "b1c3", "g7g6"});

    // 1. d4 Openings
    add_line("A40", "Queen's Pawn Game", {"d2d4"});
    add_line("D00", "Queen's Pawn Game: Closed", {"d2d4", "d7d5"});
    add_line("D00", "Queen's Pawn Game: London System", {"d2d4", "d7d5", "c1f4"});
    add_line("D00", "Queen's Pawn Game: London System", {"d2d4", "d7d5", "g1f3", "g8f6", "c1f4"});
    add_line("D00", "Queen's Pawn Game: London System", {"d2d4", "g8f6", "c1f4"});
    add_line("D00", "Queen's Pawn Game: London System", {"d2d4", "g8f6", "g1f3", "d7d5", "c1f4"});
    add_line("D00", "Colle System", {"d2d4", "d7d5", "g1f3", "g8f6", "e2e3"});
    add_line("D01", "Richter-Veresov Attack", {"d2d4", "d7d5", "b1c3", "g8f6", "c1g5"});

    // Queen's Gambit
    add_line("D06", "Queen's Gambit", {"d2d4", "d7d5", "c2c4"});
    add_line("D20", "Queen's Gambit Accepted", {"d2d4", "d7d5", "c2c4", "d5c4"});
    add_line("D30", "Queen's Gambit Declined", {"d2d4", "d7d5", "c2c4", "e7e6"});
    add_line("D35", "Queen's Gambit Declined: Exchange Variation", {"d2d4", "d7d5", "c2c4", "e7e6", "b1c3", "g8f6", "c4d5", "e6d5"});
    add_line("D37", "Queen's Gambit Declined: Classical Variation", {"d2d4", "d7d5", "c2c4", "e7e6", "b1c3", "g8f6", "g1f3", "f8e7", "c1f4"});
    add_line("D10", "Slav Defense", {"d2d4", "d7d5", "c2c4", "c7c6"});
    add_line("D11", "Slav Defense: Modern Line", {"d2d4", "d7d5", "c2c4", "c7c6", "g1f3", "g8f6"});
    add_line("D43", "Semi-Slav Defense", {"d2d4", "d7d5", "c2c4", "c7c6", "g1f3", "g8f6", "b1c3", "e7e6"});

    // Indian Defenses
    add_line("A45", "Indian Defense", {"d2d4", "g8f6"});
    add_line("A46", "Torre Attack", {"d2d4", "g8f6", "g1f3", "e7e6", "c1g5"});
    add_line("A48", "King's Indian: East Indian Defense", {"d2d4", "g8f6", "g1f3", "g7g6"});
    add_line("E00", "Catalan Opening", {"d2d4", "g8f6", "c2c4", "e7e6", "g2g3"});
    add_line("E12", "Queen's Indian Defense", {"d2d4", "g8f6", "c2c4", "e7e6", "g1f3", "b7b6"});
    add_line("E20", "Nimzo-Indian Defense", {"d2d4", "g8f6", "c2c4", "e7e6", "b1c3", "f8b4"});
    add_line("E32", "Nimzo-Indian Defense: Classical Variation", {"d2d4", "g8f6", "c2c4", "e7e6", "b1c3", "f8b4", "d1c2"});
    add_line("E60", "King's Indian Defense", {"d2d4", "g8f6", "c2c4", "g7g6"});
    add_line("E61", "King's Indian Defense: Main Line", {"d2d4", "g8f6", "c2c4", "g7g6", "b1c3", "f8g7", "e2e4", "d7d6"});
    add_line("D70", "Grünfeld Defense", {"d2d4", "g8f6", "c2c4", "g7g6", "b1c3", "d7d5"});
    add_line("A80", "Dutch Defense", {"d2d4", "f7f5"});
    add_line("A60", "Benoni Defense", {"d2d4", "g8f6", "c2c4", "c7c5", "d4d5"});
    add_line("A57", "Benko Gambit", {"d2d4", "g8f6", "c2c4", "c7c5", "d4d5", "b7b5"});

    // Flank Openings
    add_line("A10", "English Opening", {"c2c4"});
    add_line("A20", "English Opening: King's English Variation", {"c2c4", "e7e5"});
    add_line("A30", "English Opening: Symmetrical Variation", {"c2c4", "c7c5"});
    add_line("A04", "Réti Opening", {"g1f3"});
    add_line("A09", "Réti Opening: King's Indian Attack", {"g1f3", "d7d5", "c2c4"});
    add_line("A02", "Bird's Opening", {"f2f4"});
    add_line("A01", "Nimzo-Larsen Attack", {"b2b3"});
}

} // namespace zugfish

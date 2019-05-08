#include <vector>
#include <iostream>
#include <sstream>
#include <random>
#include <string>
#include <unordered_map>

unsigned char random_char() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 255);
    return static_cast<unsigned char>(dis(gen));
}

// this is too slow
std::string generate_hex(const unsigned int len) {
    std::stringstream ss;
    for (unsigned int i = 0; i < len; i++) {
        const auto rc = random_char();
        std::stringstream hexstream;
        hexstream << std::hex << int(rc);
        auto hex = hexstream.str();
        ss << (hex.length() < 2 ? '0' + hex: hex);
    }
    return ss.str();
}

std::string generate_event_id(int rank, int step, size_t idx) {
    return std::to_string(rank) + "_" + std::to_string(step) + "_" + std::to_string(idx);
}


#include <vector>
#include <iostream>
#include <sstream>
#include <random>
#include <string>
#include <unordered_map>
#include <regex>
#include <chimbuko/core/ad/utils.hpp>
#include <chimbuko/core/verbose.hpp>
#include <chimbuko/core/util/string.hpp>

namespace chimbuko{

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

std::string generate_event_id(int rank, int step, long idx) {
    return std::to_string(rank) + ":" + std::to_string(step) + ":" + std::to_string(idx);
}

std::string generate_event_id(int rank, int step, long idx, unsigned long eid) {
    return std::to_string(rank) + "-" + std::to_string(eid) + ":" + std::to_string(step) + ":" + std::to_string(idx);
    // return generate_event_id(rank, step, idx) + "_" + std::to_string(eid);
}

std::string getHPserverIP(const std::string &base_ip, const int hpserver_nthr, const int rank){
  if(hpserver_nthr <= 0) throw std::runtime_error("getHPserverIP input hpserver_nthr cannot be <1");
  std::regex r(R"((.*)\:(\d+)\s*$)");
  std::smatch m;
  if(!std::regex_match(base_ip, m, r)) throw std::runtime_error("getHPserverIP Could not parse base IP address " + base_ip);
  std::string server = m[1];
  int base_port = strToAny<int>(m[2]);
  
  int port_offset = rank % hpserver_nthr; //assign round-robin to ranks

  int port = base_port + port_offset;
  std::string new_ip = server + ":" + anyToStr(port);
  return new_ip;
}

}

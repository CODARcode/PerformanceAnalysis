#pragma once

namespace chimbuko{
  /**
   * @brief Return a random character
   */
  unsigned char random_char();

  /*
   * @brief Generate the hexadecimal representation of an integer
   *
   * WARNING: this is slow, apparently
   */
  std::string generate_hex(const unsigned int len);
  
  /*
   * @brief Generate an event_id string of form ${rank}:${step}:${idx}
   */
  std::string generate_event_id(int rank, int step, size_t idx);

  /*
   * @brief Generate an event_id string of form ${rank}-${eid}:${step}:${idx}
   */
  std::string generate_event_id(int rank, int step, size_t idx, unsigned long eid);


  /*
   * @brief Get the IP of the hierarchical pserver endpoint that this rank should connect to. Ports are offset round-robin by the MPI rank of the process
   * @param base_ip The base IP of the server
   * @param hpserver_nthr The number of endpoints/threads the HPserver has
   * @param rank MPI rank of the client
   */
  std::string getHPserverIP(const std::string &base_ip, const int hpserver_nthr, const int rank);
};


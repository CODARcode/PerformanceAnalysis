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
};


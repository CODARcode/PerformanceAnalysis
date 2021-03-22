#pragma once

#include<functional>
#include<boost/functional/hash.hpp>

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


  /**
   * @brief A class representing a unique descriptor for an event
   */
  struct eventID{
    int rank; /**< Process rank of event */
    int step; /**< IO step in which event occurred */
    long idx; /**< Index of event within the IO step */

    eventID(): rank(0),step(0),idx(0){}
    eventID(const int rank, const int step, const long idx): rank(rank), step(step), idx(idx){}

    /**
     * @brief Return an event representing the special "root" event
     */
    inline static eventID root(){ return eventID(-1,-1,-1); }

    /**
     * @brief Is this event the special root event?
     */
    inline bool isRoot() const{ return rank == -1 && step == -1 && idx == -1; }

    inline bool operator==(const eventID &r) const{ return r.rank == rank && r.step == step && r.idx == idx; }
    inline bool operator!=(const eventID &r) const{ return r.rank != rank || r.step != step || r.idx != idx; }

    /**
     * @brief Return a string representing the event of the form "${rank}:${step}:${idx}" or "root" for the special root event
     */
    inline std::string toString() const{ return isRoot() ? "root" : generate_event_id(rank, step, idx); }
  };

  /*
   * @brief Get the IP of the hierarchical pserver endpoint that this rank should connect to. Ports are offset round-robin by the MPI rank of the process
   * @param base_ip The base IP of the server
   * @param hpserver_nthr The number of endpoints/threads the HPserver has
   * @param rank MPI rank of the client
   */
  std::string getHPserverIP(const std::string &base_ip, const int hpserver_nthr, const int rank);
};

namespace std{
  /**
   * @brief Specialize std::hash for eventID type
   */
  template <>
  struct hash<chimbuko::eventID>
  {
    inline std::size_t operator()(const chimbuko::eventID& k) const{
      std::size_t hash = 0;
      boost::hash_combine(hash, k.rank);
      boost::hash_combine(hash, k.step);
      boost::hash_combine(hash, k.idx);
      return hash;
    }
  };
};

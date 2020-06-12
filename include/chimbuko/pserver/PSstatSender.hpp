#pragma once

#include <thread>
#include <atomic>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <vector>

namespace chimbuko{

  /**
   * @brief Base class for wrappers around objects/object pointers that return JSON objects that are sent to the parameter server
   *
   * The JSON objects are collected into a single object whose members are tagged according to the "tag" provided by the wrapper
   * Nothing will be sent if the resulting JSON object is empty
   */
  struct PSstatSenderPayloadBase{
    /**
     * @brief Add the JSON object payload to 'into' as a new member with an appropriate tag (user should ensure no duplicate tags!)
     */
    virtual void add_json(nlohmann::json &into) const = 0;

    /**
     * @brief Whether to request a callback to process the response (optional)
     * @param packet The string packet returned by the previous call to get_json()
     * @param returned The string returned in response
     */
    virtual bool do_fetch() const{ return false; }
    /**
     * @brief If a callback is requested, this function is called after it is returned
     */
    virtual void process_callback(const std::string &packet, const std::string &returned) const{}

    virtual ~PSstatSenderPayloadBase(){}
  };


  
  /**
   * @brief A class that periodically sends aggregate statistics to the visualization module via curl using a background thread
   */
  class PSstatSender{
  public:
    

    /**
     * @brief Constructpr
     * @param send_freq The frequency (in milliseconds) at which sends are performed to the viz module
     */
    PSstatSender(size_t send_freq = 1000);
    ~PSstatSender();
    
    /**
     * @brief Change the frequency (in milliseconds) at which sends are performed to the viz module. Must be set prior to calling run_stat_sender.
     */
    void set_send_freq(const size_t freq){ m_send_freq = freq; }

    /**
     * @brief Start sending global anomaly stats to the visualization module (curl)
     * 
     * @param url The URL of the visualization module
     */
    void run_stat_sender(std::string url);

    /**
     * @brief Stop sending global anomaly stats to the visualization module (curl)
     */
    void stop_stat_sender(int wait_msec=0);

    /**
     * @brief Add a payload. Takes ownership of pointer, which is freed
     */
    void add_payload(PSstatSenderPayloadBase *payload){ m_payloads.push_back(payload); }
    
    /**
     * @brief If an exception is caught in the thread loop, the thread will stop issuing sends and set this bool to true
     */
    bool bad() const{ return m_bad == true; }

  private:
    size_t m_send_freq; /**< Number of seconds between sends to viz*/
    // thread workder to periodically send anomaly statistics to web server, if it is available.
    std::thread     * m_stat_sender; 
    std::atomic_bool  m_stop_sender;

    std::atomic_bool  m_bad; /**< If an exception is caught in the thread loop, the thread will stop issuing sends and set this bool to true */
    std::vector<PSstatSenderPayloadBase*> m_payloads; /**< Vector of payload wrappers defining the sets of data sent to the parameter server */
  };




};

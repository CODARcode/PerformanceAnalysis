#pragma once
#include <chimbuko_config.h>
#include <string>
#include <chrono>
#include <cassert>

namespace chimbuko{

  /**
   * @brief Get the local date and time in format "YYYY/MM/DD HH:MM:SS"
   */
  std::string getDateTime();

  /**
   * @brief Get the local date and time in format YYYY.MM.DD-HH.MM.SS suitable for a filename extension
   */
  std::string getDateTimeFileExt();

  /**
   * @brief A timer / stopwatch class
   */
  class Timer{
    typedef std::chrono::high_resolution_clock Clock;
    typedef std::chrono::milliseconds MilliSec;
    typedef std::chrono::microseconds MicroSec;
    Clock::time_point m_start; /**< The start timepoint */
    Clock::duration m_add; /**< Durations between previous start/unpause and pause */
    bool m_running; /**< Is the timer running? */
  public:
    Timer(bool start_now = true);

    /**
     * @brief (Re)start the timer
     */
    void start();

    /**
     * @brief Pause the timer
     */
    void pause();

    /**
     * @brief Unpause the timer.
     *
     * This is the same as start but it does not zero the accumulated time from previous active periods
     */
    void unpause();

    /**
     * @brief Compute the elapsed time in microseconds since start/unpause plus accumulated time from previoud active periods
     */
    double elapsed_us() const;
    /**
     * @brief Compute the elapsed time in milliconds since start/unpause plus accumulated time from previoud active periods
     */
    double elapsed_ms() const;
  };



};

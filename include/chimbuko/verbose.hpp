#pragma once

#include<chimbuko/util/time.hpp>
#include<iostream>

namespace chimbuko {

  /**
   * @brief Global control of whether verbose logging is active (default false)
   */
  inline bool & enableVerboseLogging(){ static bool v = false; return v; }

  /**
   * @brief Macro for log output that appears when verbose logging is enabled
   *
   * Example usage:  verboseStream << "Hello world!" << std::endl; 
   */
#define verboseStream \
  if(!enableVerboseLogging()){} \
  else std::cout << '[' << getDateTime() << "] "


  /**
   * @brief Macro for log output that includes the date and time, intended for reporting progress on service components for which there is only one rank
   *
   * Example usage:  progressStream << "Hello world!" << std::endl; 
   */
#define progressStream \
  std::cout << '[' << getDateTime() << "] "



  /**
   * @brief Choose the rank designated as head rank upon which progress output will be reported; default 0
   */
  inline int & progressHeadRank(){ static int rank = 0; return rank; }


  /**
   * @brief Macro for log output that appears when either the rank is equal to the head rank or verbose logging is enabled
   * @param rank The rank of the current process
   * Example usage:  progressStream(rank) << "Hello world!" << std::endl; 
   */
#define headProgressStream(rank)	\
  if(rank != progressHeadRank() && !enableVerboseLogging()){}	\
  else std::cout << '[' << getDateTime() << ", rank " << rank << "] "

  
};

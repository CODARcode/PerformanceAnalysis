#pragma once

namespace chimbuko {

  /**
   * @brief Static class to control verbose output
   */
  class Verbose{
  private:
    /**
     * @brief Access static verbose static bool
     */
    inline static bool & vrb(){ static bool v=false; return v; }
  public:
    /**
     * @brief Set verbose flag
     * @param val The value
     */
    inline static void set_verbose(bool val){ vrb() = val; }

    /**
     * @brief Determine if verbose mode is activated
     * @return Bool indicating whether verbose mode is active
     */    
    inline static bool on(){ return vrb(); }
  };

  /**
   * @brief Macro enclosing a statement that is to only be printed if verbose mode is active
   */
#define VERBOSE(STATEMENT)			\
  if(Verbose::on()){ STATEMENT; }
  
};

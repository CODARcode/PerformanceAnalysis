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
   * @brief Macro enclosing a statement that is to only be performed if verbose mode is active
   */
#define VERBOSE(STATEMENT)			\
  if(Verbose::on()){ STATEMENT; }

  /**
   * @brief Macro enclosing a statement that is to only be performed if the rank is equal to some head rank or verbose mode is active
   */
#define PROGRESS(HEAD_RANK, RANK, STATEMENT)	\
  if(RANK == HEAD_RANK || Verbose::on() ){ STATEMENT; }
  
};

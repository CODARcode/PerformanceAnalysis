#pragma once
#include <climits>

/*! \file ADDefine.hpp
    \brief 
    
    Details.
*/


/*!
  \def IDX_P
  index of program id
*/

/*!
  \def IDX_R
  index of rank id
*/

/*!
  \def IDX_T
  index of thread id
*/

/*!
  \def IDX_E
  index of event (entry/exit/send/recv) id
*/

#define IDX_P 0    // program index
#define IDX_R 1    // rank index
#define IDX_T 2    // thread index
#define IDX_E 3    // event index (entry/exit/send/recv)

/*!
  \def FUNC_EVENT_DIM
  dimension of a function (timer) event vector
*/

/*!
  \def FUNC_IDX_F
  index of function (timer) id
*/

/*!
  \def FUNC_IDX_TS
  index of timestamp in function (timer) event
*/

#define FUNC_EVENT_DIM 6  // function event vector dimension
#define FUNC_IDX_F 4      // function index
#define FUNC_IDX_TS 5     // timestamp index

/*!
  \def COMM_EVENT_DIM
  dimension of a communication event vector
*/

/*!
  \def COMM_IDX_TAG
  index of communication tag
*/

/*!
  \def COMM_IDX_PARTNER
  index of communication partner
*/

/*!
  \def COMM_IDX_BYTES
  index of communication size (in bytes)
*/

/*!
  \def COMM_IDX_TS
  index of communication timestamp
*/

#define COMM_EVENT_DIM 8    // communication event vector dimension
#define COMM_IDX_TAG     4  // communication tag index
#define COMM_IDX_PARTNER 5  // partner
#define COMM_IDX_BYTES   6  // size in bytes
#define COMM_IDX_TS      7  // timestamp



/*!
  \def COUNTER_EVENT_DIM
  dimension of a counter event vector
*/

/*!
  \def COUNTER_IDX_ID
  index of counter idx
*/

/*!
  \def COUNTER_IDX_VALUE
  index of counter value
*/

/*!
  \def COUNTER_IDX_TS
  index of counter timestamp
*/


#define COUNTER_EVENT_DIM 6 //counter event vector dimension
#define COUNTER_IDX_ID 3   //counter index (maps to a counter name using metadata collected by parser)
#define COUNTER_IDX_VALUE 4 //value (integer)
#define COUNTER_IDX_TS 5    //timestamp


/*!
  \def MAX_RUNTIME
  maximum execution time of a function (or a timer)
*/

#define MAX_RUNTIME LONG_MAX;

/*!
  \def IO_VERSION
  IO version number (deprecated)
*/
#define IO_VERSION 1

namespace chimbuko {

/** Error kinds of the ADParser class
 */
enum class ParserError
{
    OK = 0,          /**< OK (no error) */
    NoFuncData = 1,  /**< Failed to fetch function data */
    NoCommData = 2,  /**< Failed to fetch communication data */
    NoCountData = 3  /**< Failed to fetch counter data */
};

/**
 * @brief Error kinds of the ADEvent class
 * 
 */
enum class EventError
{
    OK = 0,                 /**< OK (no error) */
    UnknownEvent = 1,       /**< unknown event error */
    UnknownFunc = 2,        /**< unknown function (timer) error */
    CallStackViolation = 3, /**< call stack violoation error */
    EmptyCallStack = 4      /**< empty call stack error (i.e. exit before entry ) */
};

/**
 * @brief Error kinds of the ADio class
 * 
 */
enum class IOError
{
    OK = 0,            /**< OK (no error) */
    OutIndexRange = 1  /**< Out of index range error */
};

/**
 * @brief I/O mode of the ADio class
 * 
 */
enum class IOMode
{
    Off = 0,        /**< no I/O */
    Offline = 1,    /**< offline mode, dump to files */
    Online = 2,     /**< online mode, stream data */
    Both = 3        /**< both, dump to files and stream it */
};

/**
 * @brief I/O open mode of the ADio class
 * 
 */
enum class IOOpenMode
{
    Read = 0,   /**< Read */
    Write = 1   /**< Write */
};

/**
 * @brief event type in performance trace data 
 * 
 */
enum class EventDataType {
    Unknown = 0,  /**< unknown */
    FUNC = 1,     /**< function (timer) */
    COMM = 2,     /**< communication */
    COUNT = 3     /**< counters */
};


/**
 * @brief Which end of a list/deque
 * 
 */
enum class ListEnd {
    Back = 0,
    Front = 1
};

 
  
} // end of AD namespace

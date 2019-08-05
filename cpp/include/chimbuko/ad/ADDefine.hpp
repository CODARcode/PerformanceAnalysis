#pragma once
#include <climits>

#define IDX_P 0    // program index
#define IDX_R 1    // rank index
#define IDX_T 2    // thread index
#define IDX_E 3    // event index (entry/exit/send/recv)

#define FUNC_EVENT_DIM 6  // function event vector dimension
#define FUNC_IDX_F 4      // function index 
#define FUNC_IDX_TS 5     // timestamp index

#define COMM_EVENT_DIM 8    // communication event vector dimension
#define COMM_IDX_TAG     4  // communication tag index
#define COMM_IDX_PARTNER 5  // partner
#define COMM_IDX_BYTES   6  // size in bytes
#define COMM_IDX_TS      7  // timestamp

#define MAX_RUNTIME ULONG_MAX

#define IO_VERSION 1

namespace chimbuko {

enum class ParserError
{
    OK = 0,
    NoFuncData = 1,
    NoCommData = 2
};

enum class EventError
{
    OK = 0,
    UnknownEvent = 1,
    UnknownFunc = 2,
    CallStackViolation = 3,
    EmptyCallStack = 4
};

enum class IOError
{
    OK = 0,
    OutIndexRange = 1
};

enum class IOMode
{
    Off = 0,
    Offline = 1,
    Online = 2,
    Both = 3
};

enum class IOOpenMode
{
    Read = 0,
    Write = 1
};

enum class EventDataType {
    Unknown = 0,
    FUNC = 1,
    COMM = 2,
    COUNT = 3
};

} // end of AD namespace

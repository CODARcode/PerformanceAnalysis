#pragma once
#include <climits>

#define IDX_P 0
#define IDX_R 1
#define IDX_T 2
#define IDX_E 3

#define FUNC_EVENT_DIM 6
#define FUNC_IDX_F 4
#define FUNC_IDX_TS 5

#define COMM_EVENT_DIM 8
#define COMM_IDX_TAG     4
#define COMM_IDX_PARTNER 5
#define COMM_IDX_BYTES   6 // size in bytes
#define COMM_IDX_TS      7

#define MAX_RUNTIME ULONG_MAX

#define IO_VERSION 1

namespace AD {

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
    CallStackViolation = 2
};

enum class IOError
{
    OK = 0
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

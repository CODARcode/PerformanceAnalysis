#pragma once
#include <vector>
#include <string>
#include <iostream>

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

enum class EventDataType {
    Unknown = 0,
    FUNC = 1,
    COMM = 2,
    COUNT = 3
};
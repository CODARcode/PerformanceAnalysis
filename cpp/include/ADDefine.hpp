#pragma once
#include <vector>
#include <string>
#include <iostream>

#define FUNC_EVENT_DIM 6
#define FUNC_IDX_P 0
#define FUNC_IDX_R 1
#define FUNC_IDX_T 2
#define FUNC_IDX_E 3
#define FUNC_IDX_F 4
#define FUNC_IDX_TS 5

#define COMM_EVENT_DIM 8

#define LEN_EVENT_ID 5

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

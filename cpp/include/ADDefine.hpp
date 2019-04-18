#pragma once

#define FUNC_EVENT_DIM 6
#define COMM_EVENT_DIM 8

enum class ParserError
{
    OK = 0,
    NoFuncData = 1,
    NoCommData = 2
};
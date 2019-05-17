#pragma once

#include "chimbuko/ad/ADParser.hpp"
#include "chimbuko/ad/ADEvent.hpp"
#include "chimbuko/ad/ADOutlier.hpp"
#include "chimbuko/ad/ADio.hpp"
#include <queue>

typedef std::priority_queue<chimbuko::Event_t, std::vector<chimbuko::Event_t>, std::greater<std::vector<chimbuko::Event_t>::value_type>> PQUEUE;

std::string generate_event_id(int rank, int step, size_t idx);
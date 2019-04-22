#pragma once

#include "ADParser.hpp"
#include "ADEvent.hpp"
#include "ADOutlier.hpp"

std::string generate_event_id(int rank, int step, size_t idx);
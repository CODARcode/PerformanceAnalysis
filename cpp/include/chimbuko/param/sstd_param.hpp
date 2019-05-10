#pragma once

#include "chimbuko/param.hpp"
#include "chimbuko/util/RunStats.hpp"
#include <unordered_map>

namespace chimbuko {

class SstdParam : public ParamInterface
{
public:
    SstdParam();
    ~SstdParam();
    void clear() override;

    ParamKind kind() const override { return ParamKind::SSTD; }

    std::string serialize() override;
    std::string update(const std::string parameters, bool flag=false) override;

    std::string serialize(std::unordered_map<unsigned long, RunStats>& runstats);
    void deserialize(std::unordered_map<unsigned long, RunStats>& runstats, std::string parameters) const;
    void update(std::unordered_map<unsigned long, RunStats>& runstats);
    void update(SstdParam& other) { update(other.m_runstats); }

    //RunStats operator [](unsigned long id) const { return m_runstats[id]; }
    RunStats& operator [](unsigned long id) { return m_runstats[id]; }



private:
    std::unordered_map<unsigned long, RunStats> m_runstats;
};

} // end of chimbuko namespace
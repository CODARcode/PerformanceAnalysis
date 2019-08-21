#pragma once

#include "chimbuko/param.hpp"
#include "chimbuko/util/RunStats.hpp"
#include <unordered_map>

namespace chimbuko {

class SstdParam : public ParamInterface
{
public:
    SstdParam();
    SstdParam(const std::vector<int>& n_ranks);
    ~SstdParam();
    void clear() override;

    //ParamKind kind() const override { return ParamKind::SSTD; }

    size_t size() const override { return m_runstats.size(); }

    std::string serialize() override;
    std::string update(const std::string& parameters, bool flag=false) override;
    void assign(const std::string& parameters) override;
    void show(std::ostream& os) const override;

    std::string serialize(std::unordered_map<unsigned long, RunStats>& runstats);
    void deserialize(
        const std::string& parameters,
        std::unordered_map<unsigned long, RunStats>& runstats);

    void update(std::unordered_map<unsigned long, RunStats>& runstats);
    void update(SstdParam& other) { update(other.m_runstats); }

    void assign(std::unordered_map<unsigned long, RunStats>& runstats);

    //RunStats operator [](unsigned long id) const { return m_runstats[id]; }
    RunStats& operator [](unsigned long id) { return m_runstats[id]; }

private:
    std::unordered_map<unsigned long, RunStats> m_runstats;
};

} // end of chimbuko namespace

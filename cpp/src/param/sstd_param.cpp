#include "chimbuko/param/sstd_param.hpp"

using namespace chimbuko;

SstdParam::SstdParam()
{
    clear();
}

SstdParam::~SstdParam()
{

}

std::string SstdParam::serialize()  
{
    std::lock_guard<std::mutex> _{m_mutex};
    return serialize(m_runstats);
}

std::string SstdParam::serialize(std::unordered_map<unsigned long, RunStats>& runstats) 
{
    std::stringstream ss(std::stringstream::out | std::stringstream::binary);
    size_t n;

    n = runstats.size();
    ss.write((const char*)&n, sizeof(size_t));
    for (auto& pair: runstats)
    {
        unsigned long& id = pair.first;
        RunStats& stat = pair.second;
        ss.write((const char*)&id, sizeof(unsigned long));
        stat.set_stream(true);
        ss << stat;
        stat.set_stream(false);
    }

    return ss.str();
}

void SstdParam::deserialize(std::unordered_map<unsigned long, RunStats>& runstats, std::string parameters) const
{
    std::stringstream ss(parameters, std::stringstream::in | std::stringstream::binary);

    size_t n;
    unsigned long id;
    RunStats stat;

    ss.read((char*)&n, sizeof(size_t));
    for (size_t i = 0; i < n; i++)
    {
        ss.read((char*)&id, sizeof(unsigned long));
        stat.set_stream(true);
        ss >> stat;
        stat.set_stream(false);
        runstats[id] = stat;
    }
}

std::string SstdParam::update(const std::string parameters, bool flag)
{
    std::unordered_map<unsigned long, RunStats> runstats;
    deserialize(runstats, std::string(parameters));
    update(runstats);
    if (flag)
    {
        return serialize(runstats);
    }
    return "";
}

void SstdParam::clear()
{
    m_runstats.clear();
}

void SstdParam::update(std::unordered_map<unsigned long, RunStats>& runstats)
{
    std::lock_guard<std::mutex> _(m_mutex);
    for (auto& pair: runstats) {
        m_runstats[pair.first] += pair.second;
        pair.second = m_runstats[pair.first];
    }    
}
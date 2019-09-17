#include "chimbuko/param/sstd_param.hpp"
#include <sstream>


using namespace chimbuko;

SstdParam::SstdParam()
{
    clear();
}

SstdParam::SstdParam(const std::vector<int>& n_ranks)
: ParamInterface(n_ranks)
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
        ss.write((const char*)&pair.first, sizeof(unsigned long));
        pair.second.set_stream(true);
        ss << pair.second;
        pair.second.set_stream(false);
    }
    return ss.str();
}

void SstdParam::deserialize(
    const std::string& parameters,
    std::unordered_map<unsigned long, RunStats>& runstats) 
{
    std::stringstream ss(parameters,
        std::stringstream::out | std::stringstream::in | std::stringstream::binary
    );

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

std::string SstdParam::update(const std::string& parameters, bool flag)
{
    std::unordered_map<unsigned long, RunStats> runstats;
    deserialize(parameters, runstats);
    update(runstats);
    return (flag) ? serialize(runstats): "";
}

void SstdParam::assign(std::unordered_map<unsigned long, RunStats>& runstats)
{
    std::lock_guard<std::mutex> _(m_mutex);
    for (auto& pair: runstats) {
        m_runstats[pair.first] = pair.second;
    }       
}

void SstdParam::assign(const std::string& parameters)
{
    std::unordered_map<unsigned long, RunStats> runstats;
    deserialize(parameters, runstats);
    assign(runstats);
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

void SstdParam::show(std::ostream& os) const 
{
    os << "\n"
       << "Parameters: " << m_runstats.size() << std::endl;

    for (auto stat: m_runstats)
    {
        os << "Function " << stat.first << ": "
           << "Mean: " << stat.second.mean() << ", "
           << "Std: " << stat.second.stddev() << std::endl;
    }

    os << std::endl;
}

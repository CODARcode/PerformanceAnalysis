#include "RunStats.hpp"

RunStats::RunStats() : m_s0(0.0), m_s1(0.0), m_s2(0.0)
{

}

RunStats::~RunStats() {

}

void RunStats::add(double x, double weight)
{
    m_s0 += weight;
    m_s1 += x;
    m_s2 += x*x;    
}

std::ostream& operator<<(std::ostream& os, const RunStats& rs)
{
    os << "s0: " << rs.m_s0 << ", s1: " << rs.m_s1 << ", s2: " << rs.m_s2 << std::endl;
    return os;
}

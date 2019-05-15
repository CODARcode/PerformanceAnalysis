#include "chimbuko/util/RunStats.hpp"
#include "math.h"

using namespace chimbuko;

RunStats::RunStats() : m_is_binary(false)
{
    clear();
}

RunStats::~RunStats() {

}

void RunStats::clear() {
    m_n = 0;
    m_M1 = m_M2 = m_M3 = m_M4 = 0.0;
}

void RunStats::push(double x)
{
    double delta, delta_n, delta_n2, term1;
    long long n1 = m_n;

    m_n++;
    delta = x - m_M1;
    delta_n = delta / m_n;
    delta_n2 = delta_n * delta_n;
    term1 = delta * delta_n * n1;

    m_M1 += delta_n;
    m_M4 += term1 * delta_n2 * (m_n * m_n - 3 * m_n + 3) + 6 * delta_n2 * m_M2 - 4 * delta_n * m_M3;
    m_M3 += term1 * delta_n * (m_n - 2) - 3 * delta_n * m_M2;
    m_M2 += term1;
}

long long RunStats::N() const
{
    return m_n;
}

double RunStats::mean() const 
{
    return m_M1;
}

double RunStats::var() const
{
    if (m_n < 2.0) return m_M2;
    return m_M2 / (m_n - 1.0);
}

double RunStats::std() const
{
    return sqrt( var() );
}

double RunStats::skewness() const
{
    return sqrt( double(m_n) ) * m_M3 / pow(m_M2, 1.5);
}

double RunStats::kurtosis() const
{
    return double(m_n) * m_M4 / (m_M2 * m_M2) - 3.0;
}

RunStats chimbuko::operator+(const RunStats a, const RunStats b)
{
    RunStats combined;

    combined.m_n = a.m_n + b.m_n;

    double delta  = b.m_M1 - a.m_M1;
    double delta2 = delta * delta;
    double delta3 = delta * delta2;
    double delta4 = delta2 * delta2;

    combined.m_M1 = (a.m_n * a.m_M1 + b.m_n * b.m_M1) / combined.m_n;
    combined.m_M2 = a.m_M2 + b.m_M2 + delta2 * a.m_n * b.m_n / combined.m_n;
    combined.m_M3 = a.m_M3 + b.m_M3 + delta3 * a.m_n * b.m_n * (a.m_n - b.m_n) / (combined.m_n * combined.m_n);
    combined.m_M3 += 3.0 * delta * (a.m_n * b.m_M2 - b.m_n * a.m_M2) / combined.m_n;
    combined.m_M4 = a.m_M4 + b.m_M4 + delta4 * a.m_n * b.m_n * (a.m_n * a.m_n - a.m_n * b.m_n + b.m_n * b.m_n) / (combined.m_n * combined.m_n * combined.m_n);

    return combined;
}

RunStats& RunStats::operator+=(const RunStats& rs)
{
    RunStats combined = *this + rs;
    *this = combined;
    return *this;
}

bool chimbuko::operator==(const RunStats& a, const RunStats& b)
{
    return a.m_n == b.m_n && a.m_M1 == b.m_M1 && a.m_M2 == b.m_M2 && a.m_M3 == b.m_M3 && a.m_M4 == b.m_M4;
}

std::ostream& chimbuko::operator<<(std::ostream& os, const RunStats& rs)
{
    if (rs.m_is_binary) {
        os.write((const char*)&rs.m_n, sizeof(long long));
        os.write((const char*)&rs.m_M1, sizeof(double));
        os.write((const char*)&rs.m_M2, sizeof(double));
        os.write((const char*)&rs.m_M3, sizeof(double));
        os.write((const char*)&rs.m_M4, sizeof(double));
    } else {
        os << "n: " << rs.N() << ", mean: " << rs.mean() << ", std: " << rs.std() 
        << ", skewness: " << rs.skewness() << ", kurtosis: " << rs.kurtosis() << std::endl;
    }
    return os;
}

std::istream& chimbuko::operator>>(std::istream& is, RunStats& rs)
{
    is.read((char*)&rs.m_n, sizeof(long long));
    is.read((char*)&rs.m_M1, sizeof(double));
    is.read((char*)&rs.m_M2, sizeof(double));
    is.read((char*)&rs.m_M3, sizeof(double));
    is.read((char*)&rs.m_M4, sizeof(double));
    return is;
}

double chimbuko::static_mean(const std::vector<double>& data)
{
    if (data.size() == 0) return 0.0;

    double sum = 0.0;
    for (const double d : data)
        sum += d;
    
    return sum / data.size();
}

double chimbuko::static_std(const std::vector<double>& data)
{
    if (data.size() == 0) return 0.0;
    double mean = static_mean(data);

    double var = 0.0;
    for (const double d : data)
        var += pow(d - mean, 2.0);
    
    var /= ((data.size() < 2) ? data.size(): (data.size() - 1));
    return sqrt( var );
}
#include "chimbuko/util/RunStats.hpp"
#include <chimbuko/util/serialize.hpp>
#include <cmath>
#include <limits>
#include <sstream>

using namespace chimbuko;

RunStats::RunStats(bool do_accumulate)
: m_do_accumulate(do_accumulate)
{
    clear();
}

RunStats::~RunStats() {

}

void RunStats::clear() {
  m_count = m_eta = m_rho = m_tau = m_phi = m_acc = 0.0;
  m_min = std::numeric_limits<double>::max();
  m_max = std::numeric_limits<double>::min();
}

void RunStats::push(double x)
{
    double delta, delta_n, delta_n2, term;

    if (m_count == 0.0)
    {
        m_min = x;
        m_max = x;
    }
    else
    {
        m_min = std::min(m_min, x);
        m_max = std::max(m_max, x);
    }

    if (m_do_accumulate)
    {
        m_acc += x;
    }

    delta = x - m_eta;
    delta_n = delta / (m_count + 1.0);
    delta_n2 = delta_n * delta_n;
    term = delta * delta_n * m_count;

    m_count += 1.0;
    m_eta += delta_n;
    m_phi += (
        term * delta_n2 * (m_count * m_count - 3.0 * m_count + 3.0)
        + 6.0 * delta_n2 * m_rho
        - 4.0 * delta_n * m_tau
    );
    m_tau += (
        term * delta_n * (m_count - 2.0)
        - 3.0 * delta_n * m_rho
    );
    m_rho += term;
}

double RunStats::count() const {
    return m_count;
}

double RunStats::minimum() const {
    return m_min;
}

double RunStats::maximum() const {
    return m_max;
}

double RunStats::accumulate() const {
    return m_acc;
}

double RunStats::mean() const {
    return m_eta;
}

double RunStats::variance(double ddof) const {
    if (m_count - ddof <= 0.0)
        return 0.0;
    return m_rho / (m_count - ddof);
}

double RunStats::stddev(double ddof) const {
    return std::sqrt(std::abs(variance(ddof)));
}

double RunStats::skewness() const {
    if (std::abs(m_rho) < 0.0000001)
        return 0.0;
    return std::sqrt(m_count) * m_tau / std::pow(m_rho, 1.5);
}

double RunStats::kurtosis() const {
    if (std::abs(m_rho) < 0.0000001)
        return 0.0;
    return m_count * m_phi / (m_rho * m_rho) - 3.0;
}

RunStats chimbuko::operator+(const RunStats &a, const RunStats &b)
{
    double sum_count = a.m_count + b.m_count;
    if (sum_count == 0.0)
        return RunStats();

    double delta  = b.m_eta - a.m_eta;
    double delta2 = delta * delta;
    double delta3 = delta * delta2;
    double delta4 = delta2 * delta2;

    double sum_eta = (
        (a.m_count * a.m_eta + b.m_count * b.m_eta) / sum_count
    );

    double sum_rho = (
        a.m_rho + b.m_rho
        + delta2 * a.m_count * b.m_count / sum_count
    );

    double sum_tau = (
        a.m_tau + b.m_tau
        + delta3 * a.m_count * b.m_count * (a.m_count - b.m_count) / (sum_count * sum_count)
        + 3.0 * delta * (a.m_count * b.m_rho - b.m_count * a.m_rho) / sum_count
    );

    double sum_phi = (
        a.m_phi + b.m_phi
        + delta4 * a.m_count * b.m_count
        * (a.m_count * a.m_count - a.m_count * b.m_count + b.m_count * b.m_count)
        / (sum_count * sum_count * sum_count)
        + 6.0 * delta2 * (
            a.m_count * a.m_count * b.m_rho
            + b.m_count * b.m_count * a.m_rho) / (sum_count * sum_count)
        + 4.0 * delta * (a.m_count * b.m_tau - b.m_count * a.m_tau) / sum_count
    );

    double sum_min = std::min(a.m_min, b.m_min);
    double sum_max = std::max(a.m_max, b.m_max);

    double sum_acc = 0.0;
    bool do_accumulate = false;
    if (a.m_do_accumulate || b.m_do_accumulate)
    {
      double a_acc = a.m_do_accumulate ? a.m_acc : a.m_eta * a.m_count; //reconstruct sum
      double b_acc = b.m_do_accumulate ? b.m_acc : b.m_eta * b.m_count;
      sum_acc = a_acc + b_acc;
      //sum_acc = a.m_acc + b.m_acc;
      do_accumulate = true;
    }

    RunStats combined(do_accumulate);
    combined.m_count = sum_count;
    combined.m_eta = sum_eta;
    combined.m_rho = sum_rho;
    combined.m_tau = sum_tau;
    combined.m_phi = sum_phi;
    combined.m_min = sum_min;
    combined.m_max = sum_max;
    combined.m_acc = sum_acc;

    return combined;
}

RunStats& RunStats::operator+=(const RunStats& rs)
{
    RunStats combined = *this + rs;
    *this = combined;
    return *this;
}

nlohmann::json RunStats::get_json() const {
    return {
        {"count", count()},
        {"accumulate", accumulate()},
        {"minimum", minimum()},
        {"maximum", maximum()},
        {"mean", mean()},
        {"stddev", stddev()},
        {"skewness", skewness()},
        {"kurtosis", kurtosis()}
    };
}

std::string RunStats::serialize_cerealpb() const{
  return cereal_serialize(*this);
}

void RunStats::deserialize_cerealpb(const std::string &strstate){
  cereal_deserialize(*this, strstate);
}

std::string RunStats::net_serialize() const{
  return serialize_cerealpb();
}

void RunStats::net_deserialize(const std::string &s){
  deserialize_cerealpb(s);
}

RunStats::RunStatsValues RunStats::get_stat_values() const{
  RunStatsValues out;
  out.count = count();
  out.accumulate = accumulate();
  out.minimum = minimum();
  out.maximum = maximum();
  out.mean = mean();
  out.stddev = stddev();
  out.skewness = skewness();
  out.kurtosis = kurtosis();
  return out;
}

bool RunStats::equiv(const RunStats &b) const{
     return
       std::abs(m_count - b.m_count) < 1e-6 &&
       std::abs(m_eta - b.m_eta) < 1e-3 &&
       std::abs(m_rho - b.m_rho) < 1e-3 &&
       std::abs(m_tau - b.m_tau) < 1e-3 &&
       std::abs(m_min - b.m_min) < 1e-6 &&
       std::abs(m_max - b.m_max) < 1e-6 &&
       std::abs(m_acc - b.m_acc) < 1e-6;
}



bool chimbuko::operator==(const RunStats& a, const RunStats& b){
    return
        a.m_count == b.m_count &&
        a.m_eta == b.m_eta &&
        a.m_rho == b.m_rho &&
        a.m_tau == b.m_tau &&
        a.m_min == b.m_min &&
        a.m_max == b.m_max &&
        a.m_acc == b.m_acc;
}

bool chimbuko::operator!=(const RunStats& a, const RunStats& b){
  return !(a==b);
}

double chimbuko::static_mean(const std::vector<double>& data, double ddof)
{
    if (data.size() == 0) return 0.0;

    double sum = 0.0;
    for (const double d : data)
        sum += d;

    return sum / (data.size() - ddof);
}

double chimbuko::static_std(const std::vector<double>& data, double ddof)
{
    if (data.size() == 0 || data.size() - ddof <= 0.0) return 0.0;
    double mean = static_mean(data, ddof);

    double var = 0.0;
    for (const double d : data)
        var += std::pow(d - mean, 2.0);

    var /= data.size() - ddof;
    return std::sqrt(std::abs(var));
}

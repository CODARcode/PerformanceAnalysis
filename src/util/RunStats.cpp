#include "chimbuko/util/RunStats.hpp"
#include <chimbuko/util/serialize.hpp>
#include <cmath>
#include <limits>
#include <sstream>

using namespace chimbuko;


nlohmann::json RunStats::State::get_json() const {
    return {
      {"count", count},
        {"eta", eta},
	  {"rho", rho},
	    {"tau", tau},
	      {"phi", phi},
		{"min", min},
		  {"max", max},
		    {"acc", acc}
    };
}
void RunStats::State::set_json(const nlohmann::json& state){
  count = state["count"];
  eta = state["eta"];
  rho = state["rho"];
  tau = state["tau"];
  phi = state["phi"];
  min = state["min"];
  max = state["max"];
  acc = state["acc"];
}

std::string RunStats::State::serialize_cerealpb() const{
  return cereal_serialize(*this);
}

void RunStats::State::deserialize_cerealpb(const std::string &strstate){
  cereal_deserialize(*this, strstate);
}


RunStats::RunStats(bool do_accumulate)
: m_do_accumulate(do_accumulate)
{
    clear();
}

RunStats::~RunStats() {

}

void RunStats::clear() {
    m_state.clear();
}

void RunStats::set_state(const State& s){
  m_state = s;
}

nlohmann::json RunStats::get_json_state() const {
  return m_state.get_json();
}

std::string RunStats::get_strstate(){
  return m_state.get_json().dump();
}

void RunStats::set_json_state(const nlohmann::json& state){
  m_state.set_json(state);
}

void RunStats::set_strstate(const std::string& s){
  nlohmann::json state = nlohmann::json::parse(s);
  set_json_state(state);
}

void RunStats::push(double x)
{
    double delta, delta_n, delta_n2, term;

    if (m_state.count == 0.0)
    {
        m_state.min = x;
        m_state.max = x;
    }
    else
    {
        m_state.min = std::min(m_state.min, x);
        m_state.max = std::max(m_state.max, x);
    }

    if (m_do_accumulate)
    {
        m_state.acc += x;
    }

    delta = x - m_state.eta;
    delta_n = delta / (m_state.count + 1.0);
    delta_n2 = delta_n * delta_n;
    term = delta * delta_n * m_state.count;

    m_state.count += 1.0;
    m_state.eta += delta_n;
    m_state.phi += (
        term * delta_n2 * (m_state.count * m_state.count - 3.0 * m_state.count + 3.0)
        + 6.0 * delta_n2 * m_state.rho
        - 4.0 * delta_n * m_state.tau
    );
    m_state.tau += (
        term * delta_n * (m_state.count - 2.0)
        - 3.0 * delta_n * m_state.rho
    );
    m_state.rho += term;
}

double RunStats::count() const {
    return m_state.count;
}

double RunStats::minimum() const {
    return m_state.min;
}

double RunStats::maximum() const {
    return m_state.max;
}

double RunStats::accumulate() const {
    return m_state.acc;
}

double RunStats::mean() const {
    return m_state.eta;
}

double RunStats::variance(double ddof) const {
    if (m_state.count - ddof <= 0.0)
        return 0.0;
    return m_state.rho / (m_state.count - ddof);
}

double RunStats::stddev(double ddof) const {
    return std::sqrt(std::abs(variance(ddof)));
}

double RunStats::skewness() const {
    if (std::abs(m_state.rho) < 0.0000001)
        return 0.0;
    return std::sqrt(m_state.count) * m_state.tau / std::pow(m_state.rho, 1.5);
}

double RunStats::kurtosis() const {
    if (std::abs(m_state.rho) < 0.0000001)
        return 0.0;
    return m_state.count * m_state.phi / (m_state.rho * m_state.rho) - 3.0;
}

RunStats chimbuko::operator+(const RunStats a, const RunStats b)
{
    double sum_count = a.m_state.count + b.m_state.count;
    if (sum_count == 0.0)
        return RunStats();

    double delta  = a.m_state.eta - b.m_state.eta;
    double delta2 = delta * delta;
    double delta3 = delta * delta2;
    double delta4 = delta2 * delta2;

    double sum_eta = (
        (a.m_state.count * a.m_state.eta + b.m_state.count * b.m_state.eta) / sum_count
    );

    double sum_rho = (
        a.m_state.rho + b.m_state.rho
        + delta2 * a.m_state.count * b.m_state.count / sum_count
    );

    double sum_tau = (
        a.m_state.tau + b.m_state.tau
        + delta3 * a.m_state.count * b.m_state.count * (a.m_state.count - b.m_state.count) / (sum_count * sum_count)
        + 3.0 * delta * (a.m_state.count * b.m_state.rho - b.m_state.count * a.m_state.rho) / sum_count
    );

    double sum_phi = (
        a.m_state.phi + b.m_state.phi
        + delta4 * a.m_state.count * b.m_state.count
        * (a.m_state.count * a.m_state.count - a.m_state.count * b.m_state.count + b.m_state.count * b.m_state.count)
        / (sum_count * sum_count * sum_count)
        + 6.0 * delta2 * (
            a.m_state.count * a.m_state.count * b.m_state.rho
            + b.m_state.count * b.m_state.count * a.m_state.rho) / (sum_count * sum_count)
        + 4.0 * delta * (a.m_state.count * b.m_state.tau - b.m_state.count * a.m_state.tau) / sum_count
    );

    double sum_min = std::min(a.m_state.min, b.m_state.min);
    double sum_max = std::max(a.m_state.max, b.m_state.max);

    double sum_acc = 0.0;
    bool do_accumulate = false;
    if (a.m_do_accumulate || b.m_do_accumulate)
    {
        sum_acc = a.m_state.acc + b.m_state.acc;
        do_accumulate = true;
    }

    RunStats combined(do_accumulate);
    combined.set_state(RunStats::State(
        sum_count,
        sum_eta, sum_rho, sum_tau, sum_phi,
        sum_min, sum_max, sum_acc
    ));

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

std::string RunStats::net_serialize() const{
  return get_state().serialize_cerealpb();
}

void RunStats::net_deserialize(const std::string &s){
  State state;
  state.deserialize_cerealpb(s);
  set_state(state);
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

bool chimbuko::operator==(const RunStats& a, const RunStats& b)
{
    return
        std::abs(a.m_state.count - b.m_state.count) < 1e-6 &&
        std::abs(a.m_state.eta - b.m_state.eta) < 1e-3 &&
        std::abs(a.m_state.rho - b.m_state.rho) < 1e-3 &&
        std::abs(a.m_state.tau - b.m_state.tau) < 1e-3 &&
        std::abs(a.m_state.min - b.m_state.min) < 1e-6 &&
        std::abs(a.m_state.max - b.m_state.max) < 1e-6 &&
        std::abs(a.m_state.acc - b.m_state.acc) < 1e-6;
}

bool chimbuko::operator!=(const RunStats& a, const RunStats& b)
{
    return
        std::abs(a.m_state.count - b.m_state.count) > 1e-6 ||
        std::abs(a.m_state.eta - b.m_state.eta) > 1e-3 ||
        std::abs(a.m_state.rho - b.m_state.rho) > 1e-3 ||
        std::abs(a.m_state.tau - b.m_state.tau) > 1e-3 ||
        std::abs(a.m_state.min - b.m_state.min) > 1e-6 ||
        std::abs(a.m_state.max - b.m_state.max) > 1e-6 ||
        std::abs(a.m_state.acc - b.m_state.acc) > 1e-6;
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

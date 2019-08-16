#pragma once
#include <iostream>
#include <vector>
#include <limits>

namespace chimbuko {

class RunStats {
public:
    typedef struct State {
        double count;
        double eta;
        double rho;
        double tau;
        double phi;
        double min;
        double max;
        double acc;
        State()
        {
            clear();
        }
        State(
            double _count, 
            double _eta, double _rho, double _tau, double _phi, 
            double _min, double _max, double _acc)
        : count(_count), 
          eta(_eta), rho(_rho), tau(_tau), phi(_phi),
          min(_min), max(_max), acc(_acc)
        {

        }
        clear()
        {
            count = eta = rho = tau = phi = acc = 0.0;
            min = std::numeric_limits<double>::max();
            max = std::numeric_limits<double>::min();
        }
    } State;

public:
    RunStats(bool do_accumulate = false, bool is_binary = false);
    ~RunStats();

    void clear();
    
    State get_state() { return m_state; }
    void set_state(const State& s);
    static RunStats from_state(const State& s) {
        RunStats stats;
        stats.set_state(s);
        return stats;
    }
    RunStats copy() {
        return from_state(get_state());
    }

    void push(double x);

    double count() const;
    double minimum() const;
    double maximum() const;
    double accumulate() const;
    double mean() const;
    double variance(double ddof=1.0) const;
    double stddev(double ddof=1.0) const;
    double skewness() const;
    double kurtosis() const;
    
    void set_stream(bool is_binary) { m_is_binary = is_binary; }
    
    friend RunStats operator+(const RunStats a, const RunStats b);
    RunStats& operator+=(const RunStats& rs);

    friend bool operator==(const RunStats& a, const RunStats& b);
    friend bool operator!=(const RunStats& a, const RunStats& b);

    friend std::ostream& operator<<(std::ostream& os, const RunStats& rs);
    friend std::istream& operator>>(std::istream& os, RunStats& rs);

private:
    State m_state;
    bool m_is_binary;
    bool m_do_accumulate;
};

RunStats operator+(const RunStats a, const RunStats b);
bool operator==(const RunStats& a, const RunStats& b);
bool operator!=(const RunStats& a, const RunStats& b);

std::ostream& operator<<(std::ostream& os, const RunStats& rs);
std::istream& operator>>(std::istream& is, RunStats& rs);

double static_mean(const std::vector<double>& data, double ddof=1.0);
double static_std(const std::vector<double>& data, double ddof=1.0);

} // end of chimbuko namespace

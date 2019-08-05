#pragma once
#include <iostream>
#include <vector>

namespace chimbuko {

class RunStats {

public:
    RunStats();
    ~RunStats();

    void clear();
    void push(double x);

    void set_stream(bool is_binary) { m_is_binary = is_binary; }
    
    long long N() const;

    double mean() const;
    double var() const;
    double std() const;
    // still experimental, can output nan 
    double skewness() const;
    // still experimental, can output nan or inf
    double kurtosis() const;

    friend RunStats operator+(const RunStats a, const RunStats b);
    RunStats& operator+=(const RunStats& rs);

    friend bool operator==(const RunStats& a, const RunStats& b);

    friend std::ostream& operator<<(std::ostream& os, const RunStats& rs);
    friend std::istream& operator>>(std::istream& os, RunStats& rs);
private:
    long long m_n;
    double m_M1, m_M2, m_M3, m_M4;
    bool m_is_binary;
};

RunStats operator+(const RunStats a, const RunStats b);
bool operator==(const RunStats& a, const RunStats& b);

std::ostream& operator<<(std::ostream& os, const RunStats& rs);
std::istream& operator>>(std::istream& is, RunStats& rs);

double static_mean(const std::vector<double>& data);
double static_std(const std::vector<double>& data);

} // end of chimbuko namespace
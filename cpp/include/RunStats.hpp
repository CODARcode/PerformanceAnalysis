#pragma once
#include <iostream>
#include <vector>

class RunStats {

public:
    RunStats();
    ~RunStats();

    void clear();
    void push(double x);
    
    long long N() const;

    double mean() const;
    double var() const;
    double std() const;
    double skewness() const;
    double kurtosis() const;

    friend RunStats operator+(const RunStats a, const RunStats b);
    RunStats& operator+=(const RunStats& rs);

    friend std::ostream& operator<<(std::ostream& os, const RunStats& rs);

private:
    long long m_n;
    double m_M1, m_M2, m_M3, m_M4;
};

double static_mean(const std::vector<double>& data);
double static_std(const std::vector<double>& data);
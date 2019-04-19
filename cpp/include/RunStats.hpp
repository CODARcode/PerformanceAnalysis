#pragma once
#include <iostream>

class RunStats {

public:
    RunStats();
    ~RunStats();

    double get_s0() const { return m_s0; }
    double get_s1() const { return m_s1; }
    double get_s2() const { return m_s2; }

    // double mean();
    // double std();

    void add(double x, double weight=1.0);

    friend std::ostream& operator<<(std::ostream& os, const RunStats& rs);

private:
    double m_s0, m_s1, m_s2;

};
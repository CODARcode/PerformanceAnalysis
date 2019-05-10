#pragma once
#ifdef CHIMBUKO_USE_MPINET

#include "chimbuko/net.hpp"

#include <iostream>

namespace chimbuko {

class MPINet : public NetInterface {
public:
    MPINet();
    ~MPINet();

    void init(int* argc, char*** argv, int nt) override;
    void finalize() override;

    void run() override;

    std::string name() const override { return "MPINET"; }
    int thread_level() const { return m_thread_provided; }
    int size() const { return m_size; }

private:
    int m_thread_provided;
    int m_inited;
    int m_rank;
    int m_size;
};

} // end of chimbuko namespace
#endif // CHIMBUKO_USE_MPINET
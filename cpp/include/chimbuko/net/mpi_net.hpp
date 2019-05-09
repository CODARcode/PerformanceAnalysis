#pragma once
#ifdef CHIMBUKO_USE_MPINET

#include "chimbuko/net.hpp"

#include <iostream>

namespace chimbuko {

class ResourceManager
{
public:
    ResourceManager() : m_resource(0) 
    {}
    ~ResourceManager(){}

    void update(int value) {
        std::lock_guard<std::mutex> _{m_mutex};
        std::cout << "I am " << std::this_thread::get_id() << std::endl;
        std::cout << "Update: " << m_resource << " --> " << value << std::endl;
        m_resource = value;
    }

private:
    std::mutex m_mutex;
    int m_resource;
};

class MPINet : public NetInterface {
public:
    MPINet();
    ~MPINet();

    void init(int* argc, char*** argv, int nt) override;
    void finalize() override;

    void run() override;
    void update(int value) {
        m_resource.update(value);
    }

    std::string name() const override { return "MPINET"; }

private:
    int m_thread_provided;
    int m_inited;
    int m_rank;
    int m_size;

    ResourceManager m_resource;
};

} // end of chimbuko namespace
#endif // CHIMBUKO_USE_MPINET
#pragma once
#ifdef _USE_ZMQNET

#include "chimbuko/net.hpp"
#include <zmq.h>
#include <vector>
#include <thread>

namespace chimbuko {

class ZMQNet : public NetInterface {
public:
    ZMQNet();
    ~ZMQNet();

    void init(int* argc, char*** argv, int nt) override;
    void finalize() override;
    void run() override;
    void stop() override;
    std::string name() const override { return "ZMQNET"; }

    static int send(void* socket, const std::string& strmsg);
    static int recv(void* socket, std::string& strmsg);

protected:
    void init_thread_pool(int nt) override;

private:
    bool recvAndSend(void* skFrom, void* skTo);

private:
    void* m_context;
    long long m_n_requests;
    std::vector<std::thread> m_threads;
};

} // end of chimbuko namespace
#endif // CHIMBUKO_USE_ZMQNET
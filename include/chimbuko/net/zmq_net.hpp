#pragma once
#ifdef _USE_ZMQNET

#include "chimbuko/net.hpp"
#include "chimbuko/util/mtQueue.hpp"
#include <zmq.h>
#include <vector>
#include <thread>
#include <queue>
#include <string>

namespace chimbuko {

class ZMQNet : public NetInterface {
public:
    ZMQNet();
    ~ZMQNet();

    void init(int* argc, char*** argv, int nt) override;
    void init(int* argc, char*** argv, int nt, mtQueue<std::string> &qu);
    void finalize() override;
#ifdef _PERF_METRIC
    void run(std::string logdir="./") override;
#else
    void run(std::string addr) override;
#endif
    void stop() override;
    std::string name() const override { return "ZMQNET"; }

    static int send(void* socket, const std::string& strmsg);
    static int recv(void* socket, std::string& strmsg);

protected:
    void init_thread_pool(int nt) override;
    void init_thread_pool(int nt, mtQueue<std::string> &qu);

private:
    bool recvAndSend(void* skFrom, void* skTo);

private:
    void* m_context;
    long long m_n_requests;
    std::vector<std::thread> m_threads;
};

} // end of chimbuko namespace
#endif // CHIMBUKO_USE_ZMQNET

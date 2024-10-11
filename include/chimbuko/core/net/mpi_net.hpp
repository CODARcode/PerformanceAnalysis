#pragma once
#include <chimbuko_config.h>
#ifdef _USE_MPINET

#include "chimbuko/core/net.hpp"
#include <mpi.h>
#include <iostream>
#include <atomic>

namespace chimbuko {

/**
 * @brief MPI network (OpenMPI >= 4.0.1)
 * 
 */
class MPINet : public NetInterface {
public:
    /**
     * @brief Construct a new MPINet object
     * 
     */
    MPINet();

    /**
     * @brief Destroy the MPINet object
     * 
     */
    ~MPINet();

    /**
     * @brief Initialize MPI
     * 
     * @param argc command line argc
     * @param argv command line argv
     * @param nt the number of thread in a pool
     */
    void init(int* argc, char*** argv, int nt) override;

    /**
     * @brief Finalize MPI
     * 
     */
    void finalize() override;

    /**
     * @brief Run MPI network (i.e. keep receving message from clients, if any)
     * 
     */
    void run() override;

    /**
     * @brief Stop MPI network
     * 
     */
    void stop() override;

    /**
     * @brief return the name of MPI network
     * 
     * @return std::string the name of MPI network
     */
    std::string name() const override { return "MPINET"; }

    /**
     * @brief return thread level of MPI
     * 
     * @return int thread level of MPI
     */
    int thread_level() const { return m_thread_provided; }

    /**
     * @brief return size of MPI (world communicator) 
     * 
     * @return int size of MPI (world communicator)
     */
    int size() const { return m_size; }

    /**
     * @brief return the rank of this network
     * 
     * @return int the rank of this network
     */
    int rank() const { return m_rank; }

    static std::string recv(MPI_Comm& comm, int src, int tag, int count);  
    static void send(MPI_Comm& comm, const std::string& buf, int dest, int tag, int count);

protected:
    void init_thread_pool(int nt) override;


private:
    threadPool *     m_tpool; // thread pool

    int m_thread_provided;
    int m_inited;
    int m_rank;
    int m_size;

    std::atomic_bool m_stop;
};

} // end of chimbuko namespace
#endif // CHIMBUKO_USE_MPINET

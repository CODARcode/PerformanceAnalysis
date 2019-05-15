#include "chimbuko/ad/ADOutlier.hpp"
#include "chimbuko/param/sstd_param.hpp"
#ifdef _USE_MPINET
#include "chimbuko/net/mpi_net.hpp"
#endif
#include "chimbuko/message.hpp"

using namespace chimbuko;

/* ---------------------------------------------------------------------------
 * Implementation of ADOutlier class
 * --------------------------------------------------------------------------- */
ADOutlier::ADOutlier() 
: m_use_ps(false), m_execDataMap(nullptr), m_param(nullptr)
{
}

ADOutlier::~ADOutlier() {
    if (m_param) {
        delete m_param;
    }
}

void ADOutlier::connect_ps(int rank, int srank, std::string sname) {
#ifdef _USE_MPINET
    int rs;
    char port[MPI_MAX_PORT_NAME];

    rs = MPI_Lookup_name(sname.c_str(), MPI_INFO_NULL, port);
    if (rs != MPI_SUCCESS) return;

    rs = MPI_Comm_connect(port, MPI_INFO_NULL, 0, MPI_COMM_WORLD, &m_comm);
    if (rs != MPI_SUCCESS) return;

    m_rank = rank;
    m_srank = srank;
    m_use_ps = true;

    // test connection
    Message msg;
    msg.set_info(m_rank, m_srank, MessageType::REQ_ECHO, MessageKind::DEFAULT);
    msg.set_msg("Hello!");

    MPINet::send(m_comm, msg.data(), m_srank, MessageType::REQ_ECHO, msg.count());

    MPI_Status status;
    int count;
    MPI_Probe(m_srank, MessageType::REP_ECHO, m_comm, &status);
    MPI_Get_count(&status, MPI_BYTE, &count);

    msg.clear();
    msg.set_msg(
        MPINet::recv(m_comm, status.MPI_SOURCE, status.MPI_TAG, count), true
    );

    if (msg.data_buffer().compare("Hello!>I am MPINET!") != 0)
    {
        std::cerr << "Connect error to parameter server (MPINET)!\n";
        exit(1);
    }
    //std::cout << "rank: " << m_rank << ", " << msg.data_buffer() << std::endl;

    MPI_Barrier(MPI_COMM_WORLD);
#endif
}

void ADOutlier::disconnect_ps() {
    if (!m_use_ps) return;
#ifdef _USE_MPINET
    MPI_Barrier(MPI_COMM_WORLD);
    if (m_rank == 0)
    {
        Message msg;
        msg.set_info(m_rank, m_srank, MessageType::REQ_QUIT, MessageKind::CMD);
        msg.set_msg(MessageCmd::QUIT);
        MPINet::send(m_comm, msg.data(), m_srank, MessageType::REQ_QUIT, msg.count());
    }
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Comm_disconnect(&m_comm);
    m_use_ps = false;
#endif
}

void ADOutlier::sync_outliers(const std::unordered_map<unsigned long, unsigned long>& m)
{
    if (false && m_use_ps) {
        std::cout << "Update anomaly statistics with parameter server" << std::endl;
    }

    else {
        for (auto it : m) {
            m_outliers[it.first] += it.second;
        }
    }    
}

/* ---------------------------------------------------------------------------
 * Implementation of ADOutlierSSTD class
 * --------------------------------------------------------------------------- */
ADOutlierSSTD::ADOutlierSSTD() : ADOutlier(), m_sigma(6.0) {
    m_param = new SstdParam();
}

ADOutlierSSTD::~ADOutlierSSTD() {
}

void ADOutlierSSTD::sync_param(ParamInterface* param)
{
    SstdParam& g = *(SstdParam*)m_param;
    SstdParam& l = *(SstdParam*)param;

    if (!m_use_ps) {
        g.update(l);
    }
    else {
#ifdef _USE_MPINET
        Message msg;
        msg.set_info(m_rank, m_srank, MessageType::REQ_ADD, MessageKind::SSTD);
        msg.set_msg(l.serialize(), false);

        MPINet::send(m_comm, msg.data(), m_srank, MessageType::REQ_ADD, msg.count());

        //msg.show(std::cout);

        MPI_Status status;
        int count;
        MPI_Probe(m_srank, MessageType::REP_ADD, m_comm, &status);
        MPI_Get_count(&status, MPI_BYTE, &count);

        msg.clear();
        msg.set_msg(
            MPINet::recv(m_comm, status.MPI_SOURCE, status.MPI_TAG, count), true
        );
        
        //std::cout << "rank: " << m_rank << std::endl;
        //msg.show(std::cout);

        g.assign(msg.data_buffer());
#endif
    }
}

unsigned long ADOutlierSSTD::run() {
    if (m_execDataMap == nullptr) return 0;

    SstdParam param;
    for (auto it : *m_execDataMap) {        
        for (auto itt : it.second) {
            param[it.first].push(static_cast<double>(itt->get_runtime()));
        }
    }

    // update temp runstats (parameter server)
    // std::cout << "rank: " << m_rank << ", "
    //         << "before sync: " << param.size() << ", "
    //         <<  (*m_execDataMap).size() << std::endl;
    sync_param(&param);

    // run anomaly detection algorithm
    unsigned long n_outliers = 0;
    std::unordered_map<unsigned long, unsigned long> temp_outliers;
    for (auto it : *m_execDataMap) {
        const unsigned long func_id = it.first;
        const unsigned long n = compute_outliers(func_id, it.second);
        n_outliers += n;
        temp_outliers[func_id] = n;
    }

    // update # anomaly
    sync_outliers(temp_outliers);

    return n_outliers;
}

unsigned long ADOutlierSSTD::compute_outliers(
    const unsigned long func_id, std::vector<CallListIterator_t>& data) 
{
    SstdParam& param = *(SstdParam*)m_param;
    if (param[func_id].N() < 2) return 0;
    unsigned long n_outliers = 0;

    const double mean = param[func_id].mean();
    const double std = param[func_id].std();

    const double thr_hi = mean + m_sigma * std;
    const double thr_lo = mean - m_sigma * std;

    for (auto itt : data) {
        const double runtime = static_cast<double>(itt->get_runtime());
        int label = (thr_lo > runtime || thr_hi < runtime) ? -1: 1;
        if (label == -1) {
            n_outliers += 1;
            itt->set_label(label);
        }
    }

    return n_outliers;
}

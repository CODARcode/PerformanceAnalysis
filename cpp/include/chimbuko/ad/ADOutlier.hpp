#pragma once
#include "chimbuko/ad/ADEvent.hpp"
#include "chimbuko/ad/ExecData.hpp"
#include "chimbuko/util/RunStats.hpp"
#include "chimbuko/param.hpp"
#include <mpi.h>

namespace chimbuko {

class ADOutlier {

public:
    ADOutlier();
    virtual ~ADOutlier();

    void linkExecDataMap(const ExecDataMap_t* m) { m_execDataMap = m; }
    bool use_ps() const { return m_use_ps; }
    void connect_ps(int rank, int srank = 0, std::string sname="MPINET");
    void disconnect_ps();
    virtual unsigned long run() = 0;


protected:
    virtual unsigned long compute_outliers(
        const unsigned long func_id, std::vector<CallListIterator_t>& data) = 0;
    virtual void sync_param(ParamInterface* param) = 0;
    void sync_outliers(const std::unordered_map<unsigned long, unsigned long>& m);

protected:
    bool m_use_ps;
#ifdef _USE_MPINET
    int m_rank;   // ad rank
    int m_srank;  // server rank
    MPI_Comm m_comm;
#endif

    const ExecDataMap_t * m_execDataMap;
    ParamInterface * m_param;
    std::unordered_map<unsigned long, unsigned long> m_outliers;
};

class ADOutlierSSTD : public ADOutlier {

public:
    ADOutlierSSTD();
    ~ADOutlierSSTD();

    void set_sigma(double sigma) { m_sigma = sigma; }

    unsigned long run() override;

protected:
    unsigned long compute_outliers(
        const unsigned long func_id, std::vector<CallListIterator_t>& data) override;
    void sync_param(ParamInterface* param) override;

private:
    double m_sigma;
};

} // end of AD namespace

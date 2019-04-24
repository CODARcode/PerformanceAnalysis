#include "ADio.hpp"

using namespace AD;

/* ---------------------------------------------------------------------------
 * Implementation of ADio class
 * --------------------------------------------------------------------------- */
ADio::ADio(IOMode mode) 
: m_mode(mode), m_execWindow(5), m_dispatcher(nullptr) {
    // m_file.open("/home/sungsooha/Desktop/CODAR/PerformanceAnalysis/cpp/test.txt", std::ios::app);
    // if (!m_file.is_open()) {
    //     std::cerr << "Cannot open file: " << std::endl;
    //     exit(EXIT_FAILURE);
    // }
    // m_q = new DispatchQueue("ADio", 1);
}

ADio::~ADio() {
    if (m_dispatcher) delete m_dispatcher;
    if (m_fHead.is_open()) m_fHead.close();
    if (m_fData.is_open()) m_fData.close();
    if (m_fStat.is_open()) m_fStat.close();
}

void ADio::setHeader(std::unordered_map<std::string, unsigned int> info) {
    for (auto pair: info) {
        m_head.set(pair.first, pair.second);
    }
}

void ADio::setDispatcher(std::string name, size_t thread_cnt) {
    if (m_dispatcher == nullptr)
        m_dispatcher = new DispatchQueue(name, thread_cnt);
}

static void _open(std::fstream& f, std::string filename, IOOpenMode mode) {
    if (mode == IOOpenMode::Write)
        f.open(filename, std::ios_base::out | std::ios_base::binary);
    else
        f.open(filename, std::ios_base::in | std::ios_base::binary);

    if (!f.is_open()) {
        std::cerr << "Cannot open " << filename << std::endl;
        exit(EXIT_FAILURE);
    }
}

void ADio::open(std::string prefix, IOOpenMode mode) {
    _open(m_fHead, prefix + ".head.dat", mode);
    _open(m_fData, prefix + ".data.dat", mode);
    _open(m_fStat, prefix + ".stat.dat", mode);

    if (mode == IOOpenMode::Write) {
        m_fHead << m_head;
    } else {
        m_fHead >> m_head;
    }
}

IOError ADio::write_dispatch(CallListMap_p_t* m, long long step) {

    // std::fstream& fHead = m_fHead;
    // std::fstream& fData = m_fData;

    long long * pstep = new long long;
    pstep[0] = step;

    m_dispatcher->dispatch([this, m, pstep]{
        std::cout << "inside write dispatch step: " << *pstep << std::endl;
        long long seekpos = this->m_fHead.tellp();
        long long n_exec = 0;
        for (auto it_p: *m) {
            for (auto it_r: it_p.second) {
                for (auto it_t: it_r.second) {
                    CallList_t& cl = it_t.second;
                    for (auto it: cl) {
                        if (it.get_label() == 1) continue;
                        it.set_stream(true);
                        this->m_fData << it;
                        it.set_stream(false);
                        n_exec++;
                    } // exec list
                } // thread
            } // rank
        } // program
        this->m_head.inc_nframes();
        this->m_fHead << this->m_head;

        long long offset = (*pstep) * 3 * sizeof(long long);
        std::cout << "inside write dispatch offset: " << offset << std::endl;
        this->m_fHead.seekp(this->m_head.get_offset() + offset, std::ios_base::beg);
        this->m_fHead.write((const char*)& seekpos, sizeof(long long));
        this->m_fHead.write((const char*)& n_exec, sizeof(long long));
        this->m_fHead.write((const char*)& pstep, sizeof(long long));

        delete m;
        delete pstep;
    });

    return IOError::OK;
}

IOError ADio::write(CallListMap_p_t* m, long long step) {

    // switch (m_mode)
    // {
    // case IOMode::Offline: return _write(m);
    // case IOMode::Online:
    //     break;

    // case IOMode::Both:
    //     break;

    // case IOMode::Off: 
    // default:
    //     delete m;
    //     return IOError::OK;
    // }
    if (m_dispatcher)
    {
        std::cout << "dispatch: " << step << std::endl;
        return write_dispatch(m, step);
    }

    long long seekpos = m_fData.tellp();
    long long n_exec = 0;
    for (auto it_p: *m) {
        for (auto it_r: it_p.second) {
            for (auto it_t: it_r.second) {
                CallList_t& cl = it_t.second;
                for (auto it: cl) {
                    if (it.get_label() == 1) continue;
                    it.set_stream(true);
                    m_fData << it;
                    it.set_stream(false);
                    n_exec++;
                } // exec list
            } // thread
        } // rank
    } // program

    m_head.inc_nframes();
    m_fHead << m_head;

    long long offset = step * 3 * sizeof(long long);
    m_fHead.seekp(m_head.get_offset() + offset, std::ios_base::beg);
    m_fHead.write((const char*)& seekpos, sizeof(long long));
    m_fHead.write((const char*)& n_exec, sizeof(long long));
    m_fHead.write((const char*)& step, sizeof(long long));

    delete m;

    return IOError::OK;
}

void ADio::read(CallList_t& cl, long long idx) {
    FileHeader_t::SeekPos_t pos;
    for (auto it: m_head.get_datapos())
        std::cout << "pos: " << it.pos << ", nexec: " << it.n_exec << ", step: " << it.step << std::endl;


    if (m_head.get_datapos(pos, idx)) {
        std::cout << "pos: " << pos.pos << ", nexec: " << pos.n_exec << ", step: " << pos.step << std::endl;
        m_fData.seekg(pos.pos);
        for (long long i = 0; i < pos.n_exec; i++) {
            ExecData_t exec;
            exec.set_stream(true);
            m_fData >> exec;
            exec.set_stream(false);
            cl.push_back(exec);
        }
    }
    // for (auto d: m_head.get_datapos()) {
    //     std::cout << "pos: " << d.pos << ", nexec: " << d.n_exec << ", step: " << d.step << std::endl;
    // }
}

std::ostream& AD::operator<<(std::ostream& os, ADio& io) {
    io.m_head.set_stream(false);
    os << io.m_head;
    io.m_head.set_stream(true);
    return os;
}

/* ---------------------------------------------------------------------------
 * Implementation of FileHeader_t class
 * --------------------------------------------------------------------------- */
std::ostream& AD::operator<<(std::ostream& os, const FileHeader_t& head) {
    if (head.m_is_binary) {
        os.seekp(0, std::ios_base::beg);
        os.write((const char*)&head.m_version, sizeof(unsigned int));
        os.write((const char*)&head.m_rank, sizeof(unsigned int));
        os.write((const char*)&head.m_nframes, sizeof(unsigned int));
        os.write((const char*)&head.m_algorithm, sizeof(unsigned int));
        os.write((const char*)&head.m_winsize, sizeof(unsigned int));
        os.write((const char*)&head.m_nparam, sizeof(unsigned int));
        if (head.m_nparam) {
            for (const unsigned int& param: head.m_uparams) {
                os.write((const char*)&param, sizeof(unsigned int));
            }
            for (const double& param: head.m_dparams) {
                os.write((const char*)&param, sizeof(double));
            }
        }
    } else {
        os << "\nVersion    : " << head.m_version
           << "\nRank       : " << head.m_rank
           << "\nNum. Frames: " << head.m_nframes
           << "\nAlgorithm  : " << head.m_algorithm
           << "\nWindow size: " << head.m_winsize
           << "\nNum. Params: " << head.m_nparam;
        if (head.m_nparam) {
            os << "\nParameters (unsigned int): ";
            for (const unsigned int& param: head.m_uparams)
                os << param << " ";
            os << "\nParameters (double): ";
            for (const double& param: head.m_dparams)
                os << param << " ";
            os << "\n";
        }
    }
    return os;
}

std::istream& AD::operator>>(std::istream& is, FileHeader_t& head) {
    if (head.m_is_binary) {
        is.seekg(0, std::ios_base::beg);
        is.read((char*)&head.m_version, sizeof(unsigned int));
        is.read((char*)&head.m_rank, sizeof(unsigned int));
        is.read((char*)&head.m_nframes, sizeof(unsigned int));
        is.read((char*)&head.m_algorithm, sizeof(unsigned int));
        is.read((char*)&head.m_winsize, sizeof(unsigned int));
        is.read((char*)&head.m_nparam, sizeof(unsigned int));
        head.m_uparams.resize(2*head.m_nparam);
        head.m_dparams.resize(head.m_nparam);
        for (const unsigned int& param: head.m_uparams) {
            is.read((char*)&param, sizeof(unsigned int));
        }
        for (const double& param: head.m_dparams) {
            is.read((char*)&param, sizeof(double));
        }
        if (head.m_nframes) {
            FileHeader_t::SeekPos_t pos;
            for (unsigned int i = 0; i < head.m_nframes; i++) {
                is.read((char*)&pos.pos, sizeof(long long));
                is.read((char*)&pos.n_exec, sizeof(long long));
                is.read((char*)&pos.step, sizeof(long long));
                head.m_datapos.push_back(pos);
            }
        }
    } else {
        std::cerr << "Currently, only binary format is supported for header!" << std::endl;
        exit(EXIT_FAILURE);
    }
    return is;
}
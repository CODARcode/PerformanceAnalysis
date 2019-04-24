#include "ADio.hpp"

using namespace AD;

/* ---------------------------------------------------------------------------
 * Implementation of ADio class
 * --------------------------------------------------------------------------- */
ADio::ADio(IOMode mode) 
: m_mode(mode), m_execWindow(5), m_q(nullptr) {
    // m_file.open("/home/sungsooha/Desktop/CODAR/PerformanceAnalysis/cpp/test.txt", std::ios::app);
    // if (!m_file.is_open()) {
    //     std::cerr << "Cannot open file: " << std::endl;
    //     exit(EXIT_FAILURE);
    // }
    // m_q = new DispatchQueue("ADio", 1);
}

ADio::~ADio() {
    if (m_q) delete m_q;
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
    if (m_q == nullptr)
        m_q = new DispatchQueue(name, thread_cnt);
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

// IOError ADio::_write(CallListMap_p_t* m) {

//     return IOError::OK;
// }

IOError ADio::write(CallListMap_p_t* m, unsigned long step) {

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

    bool once = true, once2 = true;

    unsigned long seekpos = m_fData.tellp();
    unsigned long n_exec = 0;
    for (auto it_p: *m) {
        for (auto it_r: it_p.second) {
            for (auto it_t: it_r.second) {
                CallList_t& cl = it_t.second;
                // auto it_beg = cl.begin();
                // auto it_end = cl.end(); 
                for (auto it: cl) {
                    if (it.get_label() == 1) continue;
                    it.set_stream(true);
                    m_fData << it;
                    it.set_stream(false);
                    n_exec++;

                    if (n_exec == 1 || n_exec == 134 || (once && it.get_n_children() > 0) || (once2 && it.get_n_message() > 0)) {
                        std::cout << it << std::endl;
                        std::cout << "Children: ";
                        for (auto c: it.get_children())
                            std::cout << c << ", ";
                        std::cout << std::endl;

                        std::cout << "Message: " << std::endl;
                        for (auto m: it.get_message())
                            std::cout << m << std::endl;
                        std::cout << std::endl;

                        if (it.get_n_children() > 0)
                            once = false;

                        if (it.get_n_message() > 0)
                            once2 = false;
                    }
                } // exec list
            } // thread
        } // rank
    } // program

    m_head.inc_nframes();
    m_fHead << m_head;

    size_t offset = step * 3 * sizeof(unsigned long);
    m_fHead.seekp(m_head.get_offset() + offset, std::ios_base::beg);
    m_fHead.write((const char*)& seekpos, sizeof(unsigned long));
    m_fHead.write((const char*)& n_exec, sizeof(unsigned long));
    m_fHead.write((const char*)& step, sizeof(unsigned long));

    std::cout << "pos: " << seekpos << ", nexec: " << n_exec << ", step: " << step << std::endl;

    delete m;



    // std::ofstream& file = m_file;
    // m_q->dispatch([&file, m]{
    //     for (auto it_p : *m) {
    //         for (auto it_r : it_p.second) {
    //             for (auto it_t: it_r.second) {
    //                 CallList_t& cl = it_t.second;
    //                 for (auto it : cl) {
    //                     file << it << std::endl;
    //                 }
    //             }
    //         }
    //     }
    //     delete m;
    // });

    return IOError::OK;
}

void ADio::read(CallList_t& cl, unsigned long idx) {
    FileHeader_t::SeekPos_t pos;
    if (m_head.get_datapos(pos, idx)) {
        std::cout << "pos: " << pos.pos << ", nexec: " << pos.n_exec << ", step: " << pos.step << std::endl;
        m_fData.seekg(pos.pos);
        for (unsigned long i = 0; i < pos.n_exec; i++) {
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
                is.read((char*)&pos.pos, sizeof(unsigned long));
                is.read((char*)&pos.n_exec, sizeof(unsigned long));
                is.read((char*)&pos.step, sizeof(unsigned long));
                head.m_datapos.push_back(pos);
            }
        }
    } else {
        std::cerr << "Currently, only binary format is supported for header!" << std::endl;
        exit(EXIT_FAILURE);
    }
    return is;
}
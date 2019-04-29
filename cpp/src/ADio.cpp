#include "ADio.hpp"
#include <sstream>
#include <curl/curl.h>

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
    //_open(m_fStat, prefix + ".stat.dat", mode);

    if (mode == IOOpenMode::Write) {
        m_fHead << m_head;
    } else {
        m_fHead >> m_head;
    }
}

class WriteFunctor
{
public:
    WriteFunctor(
        std::fstream& fHead, std::fstream& fData,
        FileHeader_t& head, CallListMap_p_t* callList, long long step
    ) 
    : m_fHead(fHead), m_fData(fData), m_head(head), m_callList(callList), m_step(step)
    {

    }

    void operator()() {
        long long seekpos = m_fData.tellp();
        long long n_exec = 0;
        for (auto it_p: *m_callList) {
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

        long long offset = m_step * 3 * sizeof(long long);
        m_fHead.seekp(m_head.get_offset() + offset, std::ios_base::beg);
        m_fHead.write((const char*)& seekpos, sizeof(long long));
        m_fHead.write((const char*)& n_exec, sizeof(long long));
        m_fHead.write((const char*)& m_step, sizeof(long long));

        m_fHead.flush();
        m_fData.flush();

        delete m_callList;
    }

private:
    std::fstream& m_fHead;
    std::fstream& m_fData;
    FileHeader_t& m_head;
    CallListMap_p_t* m_callList;
    long long m_step;
};

static size_t _curl_writefunc(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    return size * nmemb;
}

IOError ADio::write(CallListMap_p_t* m, long long step) {
    // WriteFunctor wFunc(m_fHead, m_fData, m_head, m, step);
    // if (m_dispatcher)
    //     m_dispatcher->dispatch(wFunc);
    // else
    //     wFunc();

    std::stringstream oss(std::stringstream::out | std::stringstream::binary);
    m_head.inc_nframes();

    CURL* curl;
    CURLcode res;
    curl = curl_easy_init();
    if (curl) {
        oss << m_head;
        
        std::vector<CallListIterator_t> out_execData;
        long long n_exec = 0;
        bool once1 = false, once2 = false, once3 = false;
        for (auto& it_p: *m) {
            for (auto& it_r: it_p.second) {
                for (auto& it_t: it_r.second) {
                    CallList_t& cl = it_t.second;
                    for (auto it=cl.begin(); it!=cl.end(); it++) {
                        if (it->get_label() == 1) continue;

                        if (!once1) {
                            out_execData.push_back(it);
                            n_exec++;
                            once1 = true;
                        } else if (!once2 && it->get_n_children()) {
                            out_execData.push_back(it);
                            n_exec++;
                            once2 = true;
                        } else if (!once3 && it->get_n_message()) {
                            out_execData.push_back(it);
                            n_exec++;
                            once3 = true;
                        }
                    } // CallList_t
                } // thread
            } // rank
        } // program

        oss.write((const char*)&n_exec, sizeof(long long));
        for (CallListIterator_t& it: out_execData) {
            it->set_stream(true);
            oss << *it;
            it->set_stream(false);
            std::cout << *it << std::endl << std::endl;
        }
        delete m;


        struct curl_slist * headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/octet-stream");

        std::string data = oss.str();

        curl_easy_setopt(curl, CURLOPT_URL, "http://0.0.0.0:5500/post");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data.size());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &_curl_writefunc);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    return IOError::OK;
}

IOError ADio::read(CallList_t& cl, long long idx) {
    FileHeader_t::SeekPos_t pos;
    if (m_head.get_datapos(pos, idx)) {
        m_fData.seekg(pos.pos);
        for (long long i = 0; i < pos.n_exec; i++) {
            ExecData_t exec;
            exec.set_stream(true);
            m_fData >> exec;
            exec.set_stream(false);
            cl.push_back(exec);
        }
        return IOError::OK;
    }
    return IOError::OutIndexRange;
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
#include "chimbuko/ad/ADio.hpp"
#include <sstream>

using namespace chimbuko;

/* ---------------------------------------------------------------------------
 * Implementation of ADio class
 * --------------------------------------------------------------------------- */
ADio::ADio() 
: m_execWindow(5), m_dispatcher(nullptr), m_curl(nullptr) 
{

}

ADio::~ADio() {
    if (m_dispatcher) delete m_dispatcher;
    if (m_fHead.is_open()) m_fHead.close();
    if (m_fData.is_open()) m_fData.close();
    close_curl();
}

void ADio::open_curl() {
    curl_global_init(CURL_GLOBAL_ALL);
    m_curl = curl_easy_init();
    if (m_curl == nullptr) {
        std::cerr << "Failed to initialize curl easy handler\n";
        exit(EXIT_FAILURE);
    }
}

void ADio::close_curl() {
    if (m_curl) {
        curl_easy_cleanup(m_curl);
        curl_global_cleanup();
        m_curl = nullptr;
    }
}

void ADio::setHeader(std::unordered_map<std::string, unsigned int> info) {
    for (auto pair: info) {
        m_head.set(pair.first, pair.second);
        if (pair.first.compare("winsz") == 0)
            m_execWindow = pair.second;
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
private:
    static size_t _curl_writefunc(char *, size_t size, size_t nmemb, void *)
    {
        return size * nmemb;
    }

public:
    WriteFunctor(
        std::fstream& fHead, std::fstream& fData, CURL* curl,
        FileHeader_t& head, CallListMap_p_t* callList, long long step
    ) 
    : m_fHead(fHead), m_fData(fData), m_curl(curl), 
      m_head(head), m_callList(callList), m_step(step)
    {

    }

    void operator()() {
        unsigned int winsz = m_head.get_winsize();
        CallListIterator_t prev_n, next_n, beg, end;
        long long n_exec = 0;

        std::stringstream ss_data(std::stringstream::in | std::stringstream::out | std::stringstream::binary);


        for (auto it_p: *m_callList) {
            for (auto it_r: it_p.second) {
                for (auto it_t: it_r.second) {
                    CallList_t& cl = it_t.second;
                    beg = cl.begin();
                    end = cl.end();
                    for (auto it=cl.begin(); it!=cl.end(); it++) {
                        if (it->get_label() == 1) continue;

                        prev_n = next_n = it;
                        for (unsigned int i = 0; i < winsz && prev_n != beg; i++)
                            prev_n = std::prev(prev_n);

                        for (unsigned int i = 0; i < winsz + 1 && next_n != end; i++)
                            next_n = std::next(next_n);

                        while (prev_n != next_n) {
                            if (!prev_n->is_used()) {
                                prev_n->set_use(true);
                                prev_n->set_stream(true);
                                ss_data << *prev_n;
                                n_exec++;
                            }
                            prev_n++;
                        }
                        it = next_n;
                        it--;
                    } // exec list
                } // thread
            } // rank
        } // program

        m_head.inc_nframes();
        if (m_fHead.is_open() && m_fData.is_open()) {
            m_fHead << m_head;

            long long seekpos = m_fData.tellp();
            long long offset = m_step * 3 * sizeof(long long);
            m_fHead.seekp(m_head.get_offset() + offset, std::ios_base::beg);
            m_fHead.write((const char*)& seekpos, sizeof(long long));
            m_fHead.write((const char*)& n_exec, sizeof(long long));
            m_fHead.write((const char*)& m_step, sizeof(long long));

            ss_data.seekg(0);
            m_fData << ss_data.rdbuf();

            m_fHead.flush();
            m_fData.flush();
        }

        if (m_curl) {
            std::stringstream oss(std::stringstream::in | std::stringstream::out | std::stringstream::binary);
            oss << m_head;
            oss.write((const char*)&n_exec, sizeof(long long));
            
            ss_data.seekg(0);
            oss << ss_data.rdbuf();

            struct curl_slist * headers = nullptr;
            headers = curl_slist_append(headers, "Content-Type: application/octet-stream");

            std::string data = oss.str();

            curl_easy_setopt(m_curl, CURLOPT_URL, "http://0.0.0.0:5500/post");
            curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, data.c_str());
            curl_easy_setopt(m_curl, CURLOPT_POSTFIELDSIZE, data.size());
            curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 0);
            curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, &_curl_writefunc);

            CURLcode res = curl_easy_perform(m_curl);
            if (res != CURLE_OK) {
                std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
            }
            curl_slist_free_all(headers);
        }

        delete m_callList;
    }


private:
    std::fstream& m_fHead;
    std::fstream& m_fData;
    CURL* m_curl;
    FileHeader_t& m_head;
    CallListMap_p_t* m_callList;
    long long m_step;
};

IOError ADio::write(CallListMap_p_t* callListMap, long long step) {
    WriteFunctor wFunc(m_fHead, m_fData, m_curl, m_head, callListMap, step);
    if (m_dispatcher)
        m_dispatcher->dispatch(wFunc);
    else
        wFunc();
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

std::ostream& chimbuko::operator<<(std::ostream& os, ADio& io) {
    io.m_head.set_stream(false);
    os << io.m_head;
    io.m_head.set_stream(true);
    return os;
}

/* ---------------------------------------------------------------------------
 * Implementation of FileHeader_t class
 * --------------------------------------------------------------------------- */
std::ostream& chimbuko::operator<<(std::ostream& os, const FileHeader_t& head) {
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

std::istream& chimbuko::operator>>(std::istream& is, FileHeader_t& head) {
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
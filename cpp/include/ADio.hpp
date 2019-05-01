#pragma once
#include "ADDefine.hpp"
#include "ADEvent.hpp"
#include "DispatchQueue.hpp"
#include <fstream>
#include <curl/curl.h>

namespace AD {

class FileHeader_t {
public:
    typedef struct SeekPos {
        long long pos;
        long long n_exec;
        long long step;        
    } SeekPos_t;

public:
    FileHeader_t(): m_nframes(0), m_is_binary(true) {}
    FileHeader_t(
        unsigned int ver, unsigned int rank, 
        unsigned int alg, unsigned int winsz, unsigned int nparam=4
    ) : m_version(ver), m_rank(rank), m_nframes(0), 
        m_algorithm(alg), m_winsize(winsz), m_nparam(nparam), m_is_binary(true)
    {
        m_uparams.resize(2*m_nparam);
        m_dparams.resize(m_nparam);
    }
    ~FileHeader_t(){}

    void inc_nframes() { m_nframes++; }

    void set(
        unsigned int ver, unsigned int rank, 
        unsigned int alg, unsigned int winsz, unsigned int nparam=4
    ) {
        m_version = ver;
        m_rank = rank;
        m_nframes = 0;
        m_algorithm = alg;
        m_winsize = winsz;
        m_nparam = nparam;
        m_uparams.resize(2*m_nparam);
        m_dparams.resize(m_nparam);
    }
    void set(std::string key, unsigned value) {
        if (key.compare("version") == 0) {
            m_version = value;
        }
        else if (key.compare("rank") == 0) {
            m_rank = value;
        }
        else if (key.compare("algorithm") == 0) {
            m_algorithm = value;
        }
        else if (key.compare("nparam") == 0) {
            m_nparam = value;
            m_uparams.resize(2*m_nparam);
            m_dparams.resize(m_nparam);
        }
        else if (key.compare("winsz") == 0) {
            m_winsize = value;
        }
    }
    void set_param(unsigned int value, unsigned int pos=0) {
        if (pos >= m_uparams.size()) return;
        m_uparams[pos] = value;
    }
    void set_param(double value, unsigned int pos=0) {
        if (pos >= m_uparams.size()) return;
        m_dparams[pos] = value;
    }
    void set_stream(bool is_binary) { m_is_binary = is_binary; }

    long long get_offset() const {
        return 6*sizeof(unsigned int) + m_nparam*(2*sizeof(unsigned int) + sizeof(double));
    }
    unsigned int get_winsize() const { return m_winsize; }

    const std::vector<SeekPos_t>& get_datapos() const { return m_datapos; }
    bool get_datapos(SeekPos_t& pos, unsigned long idx) {
        if (idx >= m_datapos.size()) return false;
        pos = m_datapos[idx];
        return true;
    }

    friend std::ostream& operator<<(std::ostream& os, const FileHeader_t& head);
    friend std::istream& operator>>(std::istream& is, FileHeader_t& head);

private:
    unsigned int m_version;   // 4
    unsigned int m_rank;      // 4
    unsigned int m_nframes;   // 4
    unsigned int m_algorithm; // 4
    unsigned int m_winsize;   // 4
    unsigned int m_nparam;    // 4
                              // total: 24 B
    std::vector<unsigned int> m_uparams;
    std::vector<double> m_dparams;
    
    std::vector<SeekPos_t> m_datapos;

    bool m_is_binary;
};

std::ostream& operator<<(std::ostream& os, const FileHeader_t& head);
std::istream& operator>>(std::istream& is, FileHeader_t& head);

// TODO: 
// handling very large file output!!!!
class ADio {
public:
    ADio();
    ~ADio();

    void open_curl();
    void close_curl();

    void setWinSize(unsigned int sz) { m_execWindow = sz; }
    void setHeader(std::unordered_map<std::string, unsigned int> info);
    void setDispatcher(std::string name="ioDispatcher", size_t thread_cnt=1);
    
    void open(std::string prefix="execdata", IOOpenMode=IOOpenMode::Read);

    IOError read(CallList_t& cl, long long idx);
    IOError write(CallListMap_p_t* m, long long step);

    friend std::ostream& operator<<(std::ostream& os, ADio& io);

private:
    unsigned int m_execWindow;
    std::fstream m_fHead, m_fData;
    DispatchQueue * m_dispatcher;
    FileHeader_t m_head;
    CURL* m_curl;
};

std::ostream& operator<<(std::ostream& os, const ADio& io);

} // end of AD namespace
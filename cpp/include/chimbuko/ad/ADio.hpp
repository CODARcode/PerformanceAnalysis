#pragma once
#include "chimbuko/ad/ADDefine.hpp"
#include "chimbuko/ad/ADEvent.hpp"
#include "chimbuko/util/DispatchQueue.hpp"
#include <fstream>
#include <curl/curl.h>

namespace chimbuko {

inline char check_endian()
{
    int num = 1;
    if (*(char*)&num == 1)
        return 'L'; // little-endian
    return 'B'; // big-endian
}

class FileHeader_t {
public:
    class SeekPos_t {
    public:
        long long pos;
        long long n_exec;
        long long step;        
        long long fid;
        static const size_t SIZE = 32;
    };

public:
    FileHeader_t()
    : m_version(IO_VERSION), m_endianess(check_endian()),
    m_rank(0), m_nframes(0), m_algorithm(0), m_winsize(0), m_nparam(0), 
    m_is_binary(true) 
    {

    }
    FileHeader_t(
        unsigned int rank, unsigned int alg, unsigned int winsz, unsigned int nparam=4
    ) : m_version(IO_VERSION), m_endianess(check_endian()), 
    m_rank(rank), m_nframes(0), m_algorithm(alg), m_winsize(winsz), m_nparam(nparam), 
    m_is_binary(true)
    {
        m_uparams.resize(m_nparam);
        m_dparams.resize(m_nparam);
    }
    ~FileHeader_t(){}

    static const size_t OFFSET = 1024;

    void inc_nframes() { m_nframes++; }

    void set(unsigned int rank, unsigned int alg, unsigned int winsz, unsigned int nparam=4) {
        m_rank = rank;
        m_algorithm = alg;
        m_winsize = winsz;
        m_nparam = nparam;
        m_uparams.resize(m_nparam);
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
            m_uparams.resize(m_nparam);
            m_dparams.resize(m_nparam);
        }
        else if (key.compare("winsz") == 0) {
            m_winsize = value;
        }
    }

    bool set_param(int value, unsigned int pos=0) {
        if (pos >= m_uparams.size()) return false;
        m_uparams[pos] = value;
        return true;
    }

    bool set_param(double value, unsigned int pos=0) {
        if (pos >= m_uparams.size()) return false;
        m_dparams[pos] = value;
        return true;
    }

    void set_stream(bool is_binary) { m_is_binary = is_binary; }

    unsigned int get_rank() const { return m_rank; }
    unsigned int get_nframes() const { return m_nframes; }
    unsigned int get_algorithm() const { return m_algorithm; }
    unsigned int get_winsize() const { return m_winsize; }
    unsigned int get_nparam() const { return m_nparam; }

    const std::vector<SeekPos_t>& get_datapos() const { return m_datapos; }

    bool get_datapos(SeekPos_t& pos, size_t idx) {
        if (idx >= m_datapos.size()) return false;
        pos = m_datapos[idx];
        return true;
    }

    friend std::ostream& operator<<(std::ostream& os, const FileHeader_t& head);
    friend std::istream& operator>>(std::istream& is, FileHeader_t& head);

private:
    unsigned int m_version;   // 4
    char         m_endianess; // 1
    unsigned int m_rank;      // 4
    unsigned int m_nframes;   // 4
    unsigned int m_algorithm; // 4
    unsigned int m_winsize;   // 4
    unsigned int m_nparam;    // 4
                              // total: 27 B
    std::vector<int> m_uparams;
    std::vector<double> m_dparams;
                              // ~1024 B (fixed offset)
    
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

    bool open_curl(std::string url);
    void close_curl();

    void setHeader(std::unordered_map<std::string, unsigned int> info);
    void setDispatcher(std::string name="ioDispatcher", size_t thread_cnt=1);
    void setLimit(long long limit_bytes = 1e10); // default 10G, 10,000,000,000 

    unsigned int getWinSize() const { return m_execWindow; }
    FileHeader_t& getHeader() { return m_head; }
    std::fstream& getHeadFileStream() { return m_fHead; }
    std::fstream& getDataFileStream() { return m_fData; }
    long long getDataFileIndex() { return m_data_fid; }
    CURL* getCURL() { return m_curl; }

    size_t getNumIOJobs() const {
        if (m_dispatcher == nullptr) return 0;
        return m_dispatcher->size();
    }
    
    bool is_open(bool is_head=true) const {
        if (is_head) return m_fHead.is_open();
        return m_fData.is_open();
    }
    
    void open(std::string prefix="execdata", IOOpenMode=IOOpenMode::Read);

    IOError read(CallList_t& cl, long long idx);
    IOError write(CallListMap_p_t* m, long long step);

    friend std::ostream& operator<<(std::ostream& os, ADio& io);

    // todo: checkFileLimit and _open could be static or util functions
    void checkFileLimit(std::fstream& stream, long long required);

private:
    void _open(std::fstream& f, std::string filename, IOOpenMode mode);

private:
    unsigned int m_execWindow;
    std::fstream m_fHead, m_fData;
    std::string m_prefix;
    long long m_data_fid;
    long long m_limit;
    DispatchQueue * m_dispatcher;
    FileHeader_t m_head;
    CURL* m_curl;
    std::string m_url;
};

std::ostream& operator<<(std::ostream& os, ADio& io);

} // end of AD namespace
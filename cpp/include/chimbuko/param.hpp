#pragma once
#include <mutex>
#include <string>

namespace chimbuko {

//enum class ParamKind {
//    Undefined = 0,
//    SSTD = 1
//};

class ParamInterface {
public:
    ParamInterface();
    virtual ~ParamInterface();
    virtual void clear() = 0;

    //virtual ParamKind kind() const = 0;
    virtual size_t size() const = 0;

    virtual std::string serialize() = 0;
    virtual std::string update(const std::string& parameters, bool flag=false) = 0;
    virtual void assign(const std::string& parameters) = 0;

protected:
    std::mutex m_mutex;    
};

} // end of chimbuko namespace

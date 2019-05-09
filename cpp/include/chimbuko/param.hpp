#pragma once
#include <mutex>
#include <string>

namespace chimbuko {

enum class ParamKind {
    SSTD
};

class ParamInterface {
public:
    ParamInterface();
    virtual ~ParamInterface();
    virtual void clear() = 0;

    virtual ParamKind kind() const = 0;

    virtual std::string serialize() = 0;
    virtual std::string update(const std::string parameters, bool flag=false) = 0;

protected:
    std::mutex m_mutex;    
};

} // end of chimbuko namespace
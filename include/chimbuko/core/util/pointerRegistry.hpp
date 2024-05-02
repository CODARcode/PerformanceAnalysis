#include<memory>
#include<list>

namespace chimbuko{

  class PointerRegister{
  public:
    virtual void erase() = 0;
    virtual ~PointerRegister(){}
  };

  template<typename T>
  class PointerRegisterT: public PointerRegister{
  public:
    PointerRegisterT(T* &ptr): m_ptr(ptr){}
    void erase() override{
      if(m_ptr){
	delete m_ptr;
	m_ptr = nullptr;
      }
    }

  private:
    T* & m_ptr;
  };

  class PointerRegistry{
  public:
    template<typename T>
    void registerPointer(T* &ptr){
      m_ptrs.push_back(std::unique_ptr<PointerRegister>(new PointerRegisterT<T>(ptr)));
    }
    
    void resetPointers(){
      for(auto rit = m_ptrs.rbegin(); rit != m_ptrs.rend(); rit++){
	(*rit)->erase();
      }
    }

    void deregisterPointers(){
      m_ptrs.clear();
    }

  private:
    std::list<std::unique_ptr<PointerRegister> > m_ptrs;
  };



}

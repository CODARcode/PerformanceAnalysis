#include <chimbuko/util/memutils.hpp>

#include <unistd.h>
#include <fstream>

namespace chimbuko{

  size_t getMemPageSize(){ return sysconf(_SC_PAGE_SIZE); }

  void getMemUsage(size_t &vm_total, size_t &vm_resident, size_t &data_and_stack){
    size_t page_size_kB = getMemPageSize()/1024;

    /*
      /proc/[pid]/statm  
      Provides information about memory usage, measured in pages.  The columns are:  

      size       (1) total program size  
      (same as VmSize in /proc/[pid]/status)  
      resident   (2) resident set size  
      (same as VmRSS in /proc/[pid]/status)  
      share      (3) shared pages (i.e., backed by a file)  
      text       (4) text (code)  
      lib        (5) library (unused in Linux 2.6)  
      data       (6) data + stack  
      dt         (7) dirty pages (unused in Linux 2.6)  
    */
    std::ifstream in("/proc/self/statm");
    size_t share, text, lib, dt;
    in >> vm_total >> vm_resident >> share >> text >> lib >> data_and_stack >> dt;
    vm_total *= page_size_kB;
    vm_resident *= page_size_kB;
    data_and_stack *= page_size_kB;
  }
   

};

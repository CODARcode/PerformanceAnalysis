#pragma once
#include <chimbuko_config.h>
#include <cstddef>

namespace chimbuko{

  /**
   * @brief Get the memory page size in bytes
   */
  size_t getMemPageSize();

  /**
   * @brief Get the memory usage in kB
   * @param vm_total Total virtual memory usage (resident + disk)
   * @param vm_resident Total resident (physical) memory usage
   */
  void getMemUsage(size_t &vm_total, size_t &vm_resident);

};

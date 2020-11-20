#pragma once

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
   * @param data_and_stack Total resident (physical) memory usage devoted to data and stack
   */
  void getMemUsage(size_t &vm_total, size_t &vm_resident, size_t &data_and_stack);

};

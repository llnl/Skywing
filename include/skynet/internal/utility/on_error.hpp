#ifndef SKYNET_INTERNAL_UTILITY_ON_ERROR_HPP
#define SKYNET_INTERNAL_UTILITY_ON_ERROR_HPP

#include <iostream>

namespace skynet::internal
{
  /** \brief Function that should always be used when reporting that an error
   * occured, should make transitioning to a different error handling scheme easier
   */
  void on_error(const char* const desc)
  {
    std::cerr << desc << '\n';
    std::terminate();
  }
} // namespace skynet::internal

#endif // SKYNET_INTERNAL_UTILITY_ON_ERROR_HPP

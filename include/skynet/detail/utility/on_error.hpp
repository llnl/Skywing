#ifndef SKYNET_DETAIL_UTILITY_ON_ERROR_HPP
#define SKYNET_DETAIL_UTILITY_ON_ERROR_HPP

#include <stdexcept>
#include <string>

namespace skynet { namespace detail
{
  /** Skynet exception (temporary?)
   */
  class Exception : public std::exception
  {
  public:
    explicit Exception(const char* const err)
      : err_(err)
    {}

    const char* what() const noexcept override
    {
      return err_.c_str();
    }
  private:
    // Can throw on copy; technically undefined behavior but that only happens
    // on memory exhaustion, which we're not going to handle anyways
    std::string err_;
  }; // class Exception

  /** \brief Function that should always be used when reporting that an error
   * occured, should make transitioning to a different error handling scheme easier
   */
  void on_error(const char* const desc)
  {
    throw Exception(desc);
  }
} } // namespace skynet::detail

#endif // SKYNET_DETAIL_UTILITY_ON_ERROR_HPP

#ifndef SKYNET_DETAIL_JOB_BASE_HPP
#define SKYNET_DETAIL_JOB_BASE_HPP

#include "skynet/types.hpp"
#include <vector>

namespace skynet
{
  class Master;
  namespace detail
  {
    /** \brief Job that Skynet instances are working on
     *
     * Handles wrapping communications and buffering information from tags
     */
    class JobBase
    {
    public:
      // Allow Master access to only process_data
      struct Accessor
      {
      private:
        friend class skynet::Master;
        static bool process_data(
          JobBase& job,
          const std::size_t tag,
          const char* const data,
          const std::size_t size
        )
        {
          return job.process_data(tag, data, size);
        }
      }; // struct Accessor

      virtual ~JobBase() = default;

    protected:
      /** \brief Create a JobBase with the owning master
       */
      explicit JobBase(Master& master)
        : master_{master}
      {}

      /** \brief Returns a handle to the associated master
       */
      Master& get_master() noexcept { return master_; }
      const Master& get_master() const noexcept { return master_; }

    private:
      /** \brief Processes the raw information sent from a job on another instance
       *
       * \param tag The tag the data was sent with
       * \param data The raw data sent over
       * \return True if processing went fine, false if there was an error
       */
      virtual bool process_data(TagID tag, const char* data, std::size_t size) = 0;

      // The handle to the associated master
      Master& master_;
    }; // class JobBase
  } // namespace detail
} // namespace skynet

#endif // SKYNET_DETAIL_JOB_BASE_HPP

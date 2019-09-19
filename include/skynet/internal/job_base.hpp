#ifndef SKYNET_INTERNAL_JOB_BASE_HPP
#define SKYNET_INTERNAL_JOB_BASE_HPP

#include "skynet/types.hpp"
#include <vector>

namespace skynet
{
  class Master;
  namespace internal
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
          const TagID tag,
          const TagIndex tag_index,
          const char* const data,
          const std::size_t size
        )
        {
          return job.process_data(tag, tag_index, data, size);
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
       * \param tag The id of the tag the data was sent with
       * \param tag_index The index of the tag the data was sent with
       * \param data The raw data sent over
       * \param size The size of the data
       * \return True if processing went fine, false if there was an error
       */
      virtual bool process_data(TagID tag, TagIndex tag_index, const char* data, std::size_t size) = 0;

      // The handle to the associated master
      Master& master_;
    }; // class JobBase
  } // namespace internal
} // namespace skynet

#endif // SKYNET_INTERNAL_JOB_BASE_HPP

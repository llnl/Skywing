#ifndef SKYNET_JOB_HPP
#define SKYNET_JOB_HPP

#include "tag.hpp"
#include "utility/on_error.hpp"

#include <vector>
#include <tuple>

namespace skynet
{
  /** \brief Job that Skynet instances are working on
   *
   * Handles wrapping communications and buffering information from tags
   */
  class JobBase
  {
  public:
    /** \brief Processes the raw information sent from a job on another instance
     *
     * \param tag The tag the data was sent with
     * \param data The raw data sent over
     * \return True if processing went find, false if there was an error
     */
    bool process_data(const std::size_t tag, const std::vector<char>& data)
    {
      return do_process_data(tag, data);
    }

    virtual ~JobBase() = default;

  private:
    /** \brief Implementation of process_data
     */
    virtual bool do_process_data(std::size_t tag, const std::vector<char>& data) = 0;
  }; // Class JobBase

  /** \brief Wrapper for tags so that tags with the same underlying type can be used
   */
  template<typename Tag>
  struct TagWrapper
  {
    std::vector<typename Tag::value_type> buffer;
  };

  /** \brief Job with known tags
   */
  template<typename... Tags>
  class Job : public JobBase
  {
  public:
    /** \brief Pops data from a tag buffer
     *
     * \pre The specified tag has data in its buffer.
     */
    template<typename GetTag>
    typename GetTag::value_type get_value()
    {
      auto& buffer = std::get<TagWrapper<GetTag>>(buffers_).buffer;
      auto value = std::move(buffer.back());
      buffer.pop_back();
      return value;
    }

  private:
    // Override processing of data
    bool do_process_data(const std::size_t tag, const std::vector<char>& data) override
    {
      // Ensure that the tag number isn't too large
      if (tag >= sizeof...(Tags))
      {
        on_error("do_process_data - tag number is too large");
        return false;
      }
      // Otherwise add the data to the queue
      using func_ptr = void (*)(const std::vector<char>&, void*);
      static constexpr std::array<func_ptr, sizeof...(Tags)> func_ptrs{
        &Tags::append_to_queue...
      };
      func_ptrs[tag](data, get_buffer(tag));
      return true;
    }

    // Retrieve a buffer as a void* based on an index
    void* get_buffer(const std::size_t index)
    {
      // Make an array of void* to all of the buffers
      // It seems like there should be a way to just save these as offsets, but
      // I'm not sure if there is (maybe can just do some gross pointer
      // arithmetic?  Can't use offsetof since this is a non-standard layout
      // type due to the virtual functions)
      const std::array<void*, sizeof...(Tags)> pointers{
        static_cast<void*>(&std::get<TagWrapper<Tags>>(buffers_).buffer)...
      };
      return pointers[index];
    }

    // The buffer of data for each tag
    std::tuple<TagWrapper<Tags>...> buffers_;
  }; // Class Job
} // namespace skynet

#endif // SKYNET_JOB_HPP

#ifndef SKYNET_JOB_HPP
#define SKYNET_JOB_HPP

#include "utility/on_error.hpp"
#include "utility/optional.hpp"
#include "master.hpp"
#include "job_base.hpp"

#include <vector>
#include <tuple>
#include <chrono>

namespace skynet
{
  namespace detail
  {
    // Calculates the index in a list of tags
    template<typename SearchFor, typename... Tags>
    struct tag_id_impl;

    // Match
    template<typename SearchFor, typename... Rest>
    struct tag_id_impl<SearchFor, SearchFor, Rest...>
    {
      static constexpr std::uint32_t value = 0;
    };

    // No match
    template<typename SearchFor, typename Next, typename... Rest>
    struct tag_id_impl<SearchFor, Next, Rest...>
    {
      static constexpr std::uint32_t value = 1 + tag_id_impl<SearchFor, Rest...>::value;
    };

    /** \brief Wrapper for tags so that tags with the same underlying type can be used
     */
    template<typename Tag>
    struct TagWrapper
    {
      std::vector<typename Tag::value_type> buffer;
    }; // struct TagWrapper

    // The default poll frequency for Job::get_when_ready
    static constexpr std::chrono::milliseconds default_poll_freq{1};
  } // namespace detail

  /** \brief A tag for sending values
   *
   * Tags certain values to be sent; all values sent for a specific tag must
   * be of the same type.  This type should not be constructed by user code
   * (just inherited) so the constructor is protected.
   */
  template <typename T>
  class Tag
  {
  public:
    /** \brief Parses raw data and appends it to a vector
     *
     * This shouldn't be publicly exposed.
     * \param data The data to parse to add the the vector
     * \param append_to The vector to append to, must be of type std::vector<T>
     */
    static void append_to_queue(
      const char* const data,
      const std::size_t size,
      void* const append_to
    )
    {
      auto* const true_append_to = static_cast<std::vector<T>*>(append_to);
      true_append_to->push_back(from_bytes<T>(data, size));
    }

    /** \brief The type being sent over this tag
     */
    using value_type = T;

  protected:
    Tag() = default;
  }; // class Tag

  /** \brief Job with known tags
   */
  template<typename... Tags>
  class Job : public detail::JobBase
  {
  public:
    /** \brief Creates a job with the specified id and master
     */
    explicit Job(const std::uint32_t id, Master& master)
      : JobBase{master}
      , id_{id}
    {}

    /** \brief Pops data from a tag buffer if it is available
     *
     * \pre The specified tag has data in its buffer.
     */
    template<typename GetTag>
    Optional<typename GetTag::value_type> get() noexcept
    {
      if (!has_data<GetTag>())
      {
        return {};
      }
      auto& buffer = get_buffer<GetTag>();
      auto value = std::move(buffer.back());
      buffer.pop_back();
      return value;
    }

    /** \brief Waits for data to be available on a certain tag and returns it
     * when it is.
     */
    template<
      typename GetTag,
      typename Rep = decltype(detail::default_poll_freq)::rep,
      typename Period = decltype(detail::default_poll_freq)::period
    >
    typename GetTag::value_type get_when_ready(
      const std::chrono::duration<Rep, Period>& poll_freq = detail::default_poll_freq
    ) noexcept
    {
      while (!has_data<GetTag>())
      {
        std::this_thread::sleep_for(poll_freq);
      }
      return *get<GetTag>();
    }

    /** \brief Checks if a tag buffer has data or not
     */
    template<typename GetTag>
    bool has_data() noexcept
    {
      // Retrieve any possible pending messages from the master's neighbors
      Master::Accessor::handle_neighbor_message(get_master());
      return !get_buffer<GetTag>().empty();
    }

    /** \brief Broadcasts a value on a tag to all nodes in the network
     */
    template<typename SendTag>
    void global_broadcast(const typename SendTag::value_type& value)
    {
      Master::Accessor::global_broadcast(
        get_master(),
        id_,
        tag_id<SendTag>(),
        message_id_,
        to_bytes(value)
      );
      ++message_id_;
    }

    /** \brief Broadcasts a value on a tag to all neighbors
     */
    template<typename SendTag>
    void local_broadcast(const typename SendTag::value_type& value)
    {
      Master::Accessor::local_broadcast(
        get_master(),
        id_,
        tag_id<SendTag>(),
        message_id_,
        to_bytes(value)
      );
      ++message_id_;
    }

  private:
    // Override processing of data
    bool process_data(
      const std::size_t tag,
      const char* const data,
      const std::size_t size
    ) override
    {
      // Ensure that the tag number isn't too large
      if (tag >= sizeof...(Tags))
      {
        on_error("do_process_data - tag number is too large");
        return false;
      }
      // Otherwise add the data to the queue
      using func_ptr = void (*)(const char*, std::size_t, void*);
      static constexpr std::array<func_ptr, sizeof...(Tags)> func_ptrs{
        &Tags::append_to_queue...
      };
      func_ptrs[tag](data, size, get_buffer(tag));
      return true;
    }

    // Retrieve a buffer as a reference based on a tag
    template<typename GetTag>
    auto& get_buffer() noexcept
    {
      return std::get<detail::TagWrapper<GetTag>>(buffers_).buffer;
    }
    template<typename GetTag>
    const auto& get_buffer() const noexcept
    {
      return std::get<detail::TagWrapper<GetTag>>(buffers_).buffer;
    }

    // Retrieve a buffer as a void* based on an index
    void* get_buffer(const std::size_t index) noexcept
    {
      // Make an array of void* to all of the buffers
      // It seems like there should be a way to just save these as offsets, but
      // I'm not sure if there is (maybe can just do some gross pointer
      // arithmetic?  Can't use offsetof since this is a non-standard layout
      // type due to the virtual functions)
      const std::array<void*, sizeof...(Tags)> pointers{
        static_cast<void*>(&std::get<detail::TagWrapper<Tags>>(buffers_).buffer)...
      };
      return pointers[index];
    }

    // Returns the id of a specified tag
    template<typename Tag>
    static std::uint32_t tag_id() noexcept
    {
      return detail::tag_id_impl<Tag, Tags...>::value;
    }

    // The buffer of data for each tag
    std::tuple<detail::TagWrapper<Tags>...> buffers_;

    // The id for the message to send
    // Could keep a seperate id for each tag, but running out of message id's
    // isn't very realistic
    std::uint32_t message_id_{1};

    // The id for this job; must be the same across all instances
    std::uint32_t id_;
  }; // Class Job
} // namespace skynet

#endif // SKYNET_JOB_HPP

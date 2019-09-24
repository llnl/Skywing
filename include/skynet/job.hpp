#ifndef SKYNET_JOB_HPP
#define SKYNET_JOB_HPP

#include "skynet/types.hpp"
#include "skynet/internal/job_base.hpp"
#include "skynet/internal/utility/on_error.hpp"
#include "skynet/internal/utility/type_list.hpp"
#include "skynet/master.hpp"

#include <vector>
#include <tuple>
#include <chrono>
#include <limits>
#include <cstdint>
#include <optional>
#include <unordered_map>

namespace skynet
{
  namespace internal
  {
    // The default poll frequency for Job::get_when_ready
    static constexpr std::chrono::milliseconds default_poll_freq{1};
  } // namespace internal

  /** \brief A tag for sending values
   *
   * Tags certain values to be sent; all values sent for a specific tag must
   * be of the same type.  This type should not be constructed by user code
   * (just inherited) so the constructor is private.
   */
  template <typename T>
  class Tag
  {
  public:
    /** \brief The type being sent over this tag
     */
    using ValueType = T;

    /** \brief Construct a tag with an ID
     */
    explicit constexpr Tag(const TagID id) noexcept
      : id_{id}
    {}

    /** \brief Return the id of the tag
     */
    constexpr TagID id() const noexcept { return id_; }

  private:
    template<typename...>
    friend class Job;

    /** \brief Parses raw data and appends it to a vector if it's been subscribed to
     *
     * This shouldn't be publicly exposed.
     * \param data The data to parse to add the the vector
     * \param size The size of the data
     * \param tag_id The id of the tag being accessed
     * \param map_ptr The map of vectors to append to
     */
    static void append_to_queue(
      const std::byte* const data,
      const std::size_t size,
      const TagID tag_id,
      void* const map_ptr
    )
    {
      using MapType = std::unordered_map<TagID, std::vector<T>>;
      auto& the_map = *static_cast<MapType*>(map_ptr);
      // If the tag id isn't present then this tag hasn't been subscribed to
      const auto loc = the_map.find(tag_id);
      if (loc == the_map.end())
      {
        return;
      }
      T temp;
      loc->second.push_back(internal::Deserializer{data, size}.get(temp));
    }

    // The id of this tag
    TagID id_;
  }; // class Tag

  /** \brief Job with known tags
   */
  template<typename... Tags>
  class Job : public internal::JobBase
  {
    // Make sure there aren't too many tags
    static_assert(
      sizeof...(Tags) <= std::numeric_limits<TagIndex>::max(),
      "Too many tags!"
    );

  public:
    /** \brief Creates a job with the specified id and master
     */
    explicit Job(const JobID id, Master& master)
      : JobBase{master}
      , id_{id}
    {}

    /** \brief Pops data from a tag buffer if it is available
     *
     * \pre The specified tag has data in its buffer.
     */
    template<typename GetTag>
    std::optional<typename GetTag::ValueType> get(const GetTag tag) noexcept
    {
      if (!has_data(tag))
      {
        return {};
      }
      auto& map = get_buffer_map(tag);
      auto loc = map.find(tag.id());
      if (loc == map.end())
      {
        return {};
      }
      auto& buffer = loc->second;
      auto value = std::move(buffer.back());
      buffer.pop_back();
      return value;
    }

    /** \brief Waits for data to be available on a certain tag and returns it
     * when it is.
     */
    template<
      typename GetTag,
      typename Rep = decltype(internal::default_poll_freq)::rep,
      typename Period = decltype(internal::default_poll_freq)::period
    >
    typename GetTag::ValueType get_when_ready(
      const GetTag tag,
      const std::chrono::duration<Rep, Period>& poll_freq = internal::default_poll_freq
    ) noexcept
    {
      while (!has_data(tag))
      {
        std::this_thread::sleep_for(poll_freq);
      }
      return *get(tag);
    }

    /** \brief Checks if a tag buffer has data or not
     */
    template<typename GetTag>
    bool has_data(const GetTag tag) noexcept
    {
      // Retrieve any possible pending messages from the master's neighbors
      Master::Accessor::handle_neighbor_message(get_master());
      const auto& tag_map = get_buffer_map(tag);
      const auto loc = tag_map.find(tag.id());
      if (loc == tag_map.cend())
      {
        return false;
      }
      return !loc->second.empty();
    }

    /** \brief Subscribes to the passed tag, does nothing if the job is
     * already subscribed to the tag
     */
    template<typename Tag>
    void subscribe(const Tag tag) noexcept
    {
      // Having the tag ID in the corresponding map is all that's needed for
      // subscription
      // C++17: try_emplace
      get_buffer_map(tag).emplace(
        tag.id(),
        std::vector<typename Tag::ValueType>{}
      );
    }
    /** \brief Subscribes to all of the passed tags
     */
    template<typename... SubTags>
    void subscribe(const SubTags... tags) noexcept
    {
      (subscribe(tags), ...);
    }

    /** \brief Unsubscribes to the passed tag, does nothing if the job is not
     * subscribed to the tag
     */
    template<typename Tag>
    void unsubscribe(const Tag tag) noexcept
    {
      get_buffer_map(tag).erase(tag.id());
    }
    /** \brief Unsubscribes from all of the passed tags
     */
    template<typename... UnsubTags>
    void unsubscribe(const UnsubTags... tags) noexcept
    {
      (unsubscribe(tags), ...);
    }

    /** \brief Publish data on the passed tag
     */
    template<typename Tag>
    void publish(const Tag tag, const typename Tag::ValueType& value) noexcept
    {
      global_broadcast(tag, value);
    }

  private:
    // Type list of all the tag types
    using TagList = internal::TypeList<Tags...>;

    // Override processing of data, adds some data to a buffer
    bool process_data(
      const TagID tag_id,
      const TagIndex tag_index,
      const std::byte* const data,
      const std::size_t size
    ) override
    {
      // Ensure that the tag number isn't too large
      if (tag_index >= sizeof...(Tags))
      {
        // internal::on_error("do_process_data - tag number is too large");
        return false;
      }
      // Otherwise add the data to the queue
      using func_ptr = void (*)(const std::byte*, std::size_t, TagID, void*);
      static constexpr std::array<func_ptr, sizeof...(Tags)> func_ptrs{
        &Tags::append_to_queue...
      };
      func_ptrs[tag_index](data, size, tag_id, get_buffer_map(tag_index));
      return true;
    }

    // Retrieve a buffer as a reference based on a tag
    template<typename GetTag>
    auto& get_buffer_map(const GetTag /* tag */) noexcept
    {
      using MapType = std::unordered_map<TagID, std::vector<typename GetTag::ValueType>>;
      return std::get<MapType>(buffers_);
    }
    template<typename GetTag>
    const auto& get_buffer_map(const GetTag /* tag */) const noexcept
    {
      using MapType = std::unordered_map<TagID, std::vector<typename GetTag::ValueType>>;
      return std::get<MapType>(buffers_);
    }

    // Retrieve a buffer map as a void* based on an index
    void* get_buffer_map(const TagIndex index) noexcept
    {
      // Make an array of void* to all of the buffers
      // It seems like there should be a way to just save these as offsets, but
      // I'm not sure if there is (maybe can just do some gross pointer
      // arithmetic?  Can't use offsetof since this is a non-standard layout
      // type due to the virtual functions)
      const std::array<void*, sizeof...(Tags)> pointers{
        static_cast<void*>(
          &std::get<
            std::unordered_map<
              TagID,
              std::vector<typename Tags::ValueType>
            >
          >(buffers_)
        )...
      };
      return pointers[index];
    }

    /** \brief Broadcasts a value on a tag to all nodes in the network
     */
    template<typename SendTag>
    void global_broadcast(
      const SendTag tag,
      const typename SendTag::ValueType& value
    )
    {
      Master::Accessor::broadcast(
        get_master(),
        message_id_,
        id_,
        tag.id_,
        internal::index_of<SendTag, TagList>,
        0,
        value
      );
      ++message_id_;
    }

    /** \brief Broadcasts a value on a tag to all neighbors
     */
    template<typename SendTag>
    void local_broadcast(
      const SendTag tag,
      const typename SendTag::ValueType& value,
      const std::uint32_t num_hops = 1
    )
    {
      assert(num_hops >= 1);
      Master::Accessor::broadcast(
        get_master(),
        message_id_,
        id_,
        tag.id_,
        internal::index_of<SendTag, TagList>,
        num_hops,
        value
      );
      ++message_id_;
    }

    // The buffer of data for each tag
    std::tuple<
      std::unordered_map<
        TagID,
        std::vector<typename Tags::ValueType>
      >...
    > buffers_;

    // The id for the message to send
    // Could keep a seperate id for each tag, but running out of message id's
    // isn't very realistic
    MessageID message_id_{1};

    // The id for this job; must be the same across all instances
    JobID id_;
  }; // Class Job
} // namespace skynet

#endif // SKYNET_JOB_HPP

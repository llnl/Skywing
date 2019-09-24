#ifndef SKYNET_INTERNAL_MESSAGE_HEADERS_HPP
#define SKYNET_INTERNAL_MESSAGE_HEADERS_HPP

#include "skynet/types.hpp"
#include "skynet/internal/utility/serialize.hpp"
#include "skynet/internal/utility/type_list.hpp"

#include <cstdint>

/** \brief Convience macro that only appears in this header to keep serialization
 * and deserialization in sync and to reduce repetition.  It should be used in
 * the class to serialize or deserialize, and the parameters should be the list
 * of member variables in the class
 *
 * This adds the following methods to the class:
 *
 * ```
 * std::vector<std::byte> to_bytes() const noexcept
 * ```
 * Returns a serialized version of the header
 *
 * ```
 * class_name(const std::byte* data, std::size_t count) noexcept
 * explicit class_name(const std::vector<std::byte>& data) noexcept;
 * template<std::size_t N>
 * explicit class_name(const std::array<std::byte, N>& data) noexcept;
 * ```
 * Constructs the header from a serizalized source
 *
 * ```
 * class_name() = default;
 * ```
 * Allows default construction
 */
#define SKYNET_MAKE_SERIALIZABLE(class_name, ...) \
  std::vector<std::byte> to_bytes() const noexcept \
  { \
    return Serializer{}.add(__VA_ARGS__).bytes(); \
  } \
  class_name(const std::byte* const data, const std::size_t count) noexcept \
  { \
    Deserializer{data, count}.get(__VA_ARGS__); \
  } \
  explicit class_name(const std::vector<std::byte>& data) noexcept \
    : class_name{data.data(), data.size()} \
  {} \
  template<std::size_t N> \
  explicit class_name(const std::array<std::byte, N>& data) noexcept \
    : class_name{data.data(), data.size()} \
  {} \
  class_name() = default;

/** \brief No-op version of SKYNET_MAKE_SERIALIZABLE, for headers that don't carry
 * any additional data
 */
#define SKYNET_MAKE_SERIALIZABLE_NO_OP(class_name) \
  std::vector<std::byte> to_bytes() const noexcept { return {}; } \
  explicit class_name(const std::byte*, std::size_t) noexcept {} \
  explicit class_name(const std::vector<std::byte>&) noexcept {} \
  template<std::size_t N> \
  explicit class_name(const std::array<std::byte, N>&) noexcept {} \
  class_name() = default;

namespace skynet::internal
{
  /** \brief Type to allow headers to declare what category of message they
   * represent.
   */
  enum class MessageCategory : std::uint8_t
  {
    job,
    status
  };

  /** \brief The universal header that all messages start with
   */
  struct UniversalHeader
  {
    SKYNET_MAKE_SERIALIZABLE(UniversalHeader,
      index
    );
    // Allow construction with an index
    explicit UniversalHeader(const std::uint8_t i) noexcept
      : index{i}
    {}

    /// The index for the type of header that follows this header
    std::uint8_t index;
  }; // Struct UniversalHeader

  template <class Archive>
  void serialize(Archive& ar, UniversalHeader& h) noexcept
  {
    ar(h.index);
  }

  /** \brief The header for broadcast messages
   */
  struct BroadcastHeader
  {
    SKYNET_MAKE_SERIALIZABLE(BroadcastHeader,
      message_id, job_id, tag_id, tag_index, origin, hops_left_p1, message_size
    );

    template<typename Callable1, typename Callable2>
    static bool do_callback(const std::vector<std::byte>& data, Callable1 callback, Callable2 read_from)
    {
      // Get the header and read the required number of bytes
      const BroadcastHeader header{data};
      std::vector<std::byte> buffer(header.message_size);
      if (!read_from(buffer.data(), buffer.size()))
      {
        return false;
      }
      // otherwise can go ahead and do the callback
      return callback(header, std::move(buffer));
    }

    /// The id of the message
    MessageID message_id;
    /// The job that this message is for
    JobID job_id;
    /// The id of the tag that the message was sent with
    TagID tag_id;
    /// The index of the tag that the message was sent with
    TagIndex tag_index;
    /// The id of the computer that the message is originally from
    MachineID origin;
    /// The number of further hops to do, plus 1
    /// A value of zero here means a global broadcast
    std::uint32_t hops_left_p1;
    /// The size of the message that follows
    std::uint32_t message_size;
  }; // struct BroadcastHeader

  /** \brief The header for the greeting message
   */
  struct GreetingHeader
  {
    SKYNET_MAKE_SERIALIZABLE(GreetingHeader,
      from,
      message_size
    );

    GreetingHeader(const MachineID from, const std::uint32_t size)
      : from{from}
      , message_size{size}
    {}

    template<typename Callable1, typename Callable2>
    static bool do_callback(const std::vector<std::byte>& data, Callable1 callback, Callable2 read_from)
    {
      // Get the header and read the required number of bytes
      const GreetingHeader header{data};
      std::vector<std::byte> buffer(header.message_size);
      if (!read_from(buffer.data(), buffer.size()))
      {
        return false;
      }
      return callback(header, deserialize<std::vector<MachineID>>(buffer));
    }

    /// The machine that the greeting is from
    MachineID from;
    /// The size of the message that follows
    std::uint32_t message_size;
  }; // struct GreetingHeader

  struct GoodbyeHeader
  {
    SKYNET_MAKE_SERIALIZABLE_NO_OP(GoodbyeHeader);

    template<typename Callable1, typename Callable2>
    static bool do_callback(const std::vector<std::byte>& /* data */, Callable1 callback, Callable2 /* read_from */)
    {
      return callback(GoodbyeHeader{});
    }
  }; // struct GoodbyeHeader

  /** \brief The header for the neighbor messages
   */
  struct BaseNeighborHeader
  {
    SKYNET_MAKE_SERIALIZABLE(BaseNeighborHeader,
      neighbor
    );

    // Allow construction with just the neighbor
    explicit BaseNeighborHeader(const MachineID id)
      : neighbor{id}
    {}

    template<typename Derived, typename Callable1, typename Callable2>
    static bool base_callback(const std::vector<std::byte>& data, Callable1 callback, Callable2 /* read_from */)
    {
      const BaseNeighborHeader header{data};
      return callback(static_cast<const Derived&>(header));
    }

    MachineID neighbor;
  }; // struct BaseNeighborHeader

  struct NewNeighborHeader : BaseNeighborHeader
  {
    template<typename Callable1, typename Callable2>
    static bool do_callback(const std::vector<std::byte>& data, Callable1 callback, Callable2 read_from)
    {
      return base_callback<NewNeighborHeader>(data, callback, read_from);
    }
  }; // struct NewNeighborHeader

  struct RemoveNeighborHeader: BaseNeighborHeader
  {
    template<typename Callable1, typename Callable2>
    static bool do_callback(const std::vector<std::byte>& data, Callable1 callback, Callable2 read_from)
    {
      return base_callback<RemoveNeighborHeader>(data, callback, read_from);
    }
  }; // struct RemoveNeighborHeader

  /** \brief A list of all job headers that can appear after a universal header
   *
   * This must absolutely be updated anytime a new header type is added.
   */
  using JobHeaders = TypeList<
    BroadcastHeader
  >;

  /** \brief A list of all status headers that can appear after a universal header
   *
   * This must absolutely be updated anytime a new header type is added.
   */
  using StatusHeaders = TypeList<
    GreetingHeader,
    GoodbyeHeader,
    NewNeighborHeader,
    RemoveNeighborHeader
  >;

  /** \brief Calculate the index of a header
   */
  template<typename T>
  constexpr std::uint8_t header_index() noexcept
  {
    constexpr auto job_index = index_of<T, JobHeaders>;
    constexpr auto status_index = index_of<T, StatusHeaders>;
    static_assert(
      job_index != size<JobHeaders> || status_index != size<StatusHeaders>,
      "Non-header passed to header_index!"
    );
    if (job_index < size<JobHeaders>)
    {
      return job_index;
    }
    return job_index + status_index;
  }

  /** \brief The total number of headers
   */
  constexpr int num_headers = size<JobHeaders> + size<StatusHeaders>;
} // namespace skynet::internal

// Remove the macros meant for only this header
#undef SKYNET_MAKE_SERIALIZABLE
#undef SKYNET_MAKE_SERIALIZABLE_NO_OP

#endif // SKYNET_INTERNAL_MESSAGE_HEADERS_HPP

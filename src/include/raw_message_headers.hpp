#ifndef SKYNET_SRC_MESSAGE_HEADERS_HPP
#define SKYNET_SRC_MESSAGE_HEADERS_HPP

#include "skynet/types.hpp"
#include "skynet/internal/utility/serialize.hpp"
#include "skynet/internal/utility/type_list.hpp"
#include "skynet/internal/message.hpp"

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
  /** \brief The universal header that all messages start with
   */
  struct UniversalHeader
  {
    SKYNET_MAKE_SERIALIZABLE(UniversalHeader,
      info_
    )

    // Allow construction with an index / endian information
    // Don't use the endianness quite yet, still have to work out how to pass
    // it around and such, but will want it eventually to speed-up serialization
    // and deserialization
    explicit UniversalHeader(
      const std::uint8_t index
      // const bool data_is_little_endian = machine_is_little_endian,
      // const bool headers_are_little_endian = machine_is_little_endian
    ) noexcept
      : info_{
        static_cast<std::uint8_t>(
          //(data_is_little_endian ? 0b1000'0000 : 0) |
          //(headers_are_little_endian ? 0b0100'0000 : 0) |
          (index & 0b0011'1111)
        )
      }
    {}

    /// The index of the tag
    std::uint8_t index() const noexcept { return info_ & 0b0011'1111; }
    // /// If the data is in little endian
    // bool data_is_little_endian() const noexcept { return info_ & 0b0100'0000; }
    // /// If the headers are in little endian
    // bool headers_are_little_endian() const noexcept { return info_ & 0b1000'0000; }

    /// The index for the type of header that follows this header
    std::uint8_t info_;
  }; // Struct UniversalHeader

  /** \brief The header for broadcast messages
   */
  struct BroadcastHeader
  {
    SKYNET_MAKE_SERIALIZABLE(BroadcastHeader,
      message_id, job_id, tag_id, tag_index, origin, hops_left_p1, message_size
    )

    template<typename Callable>
    static std::optional<MessageVariant> build(const std::vector<std::byte>& data, Callable read_from) noexcept
    {
      // Get the header and read the required number of bytes
      const BroadcastHeader header{data};
      std::vector<std::byte> buffer(header.message_size);
      if (!read_from(buffer.data(), buffer.size()))
      {
        return {};
      }
      // Just copy all of the fields over
      Broadcast to_ret;
      to_ret.message_id = header.message_id;
      to_ret.job_id = header.job_id;
      to_ret.tag_id = header.tag_id;
      to_ret.tag_index = header.tag_index;
      to_ret.origin = header.origin;
      to_ret.hops_left_p1 = header.hops_left_p1;
      to_ret.data = std::move(buffer);
      // otherwise can go ahead and do the callback
      return to_ret;
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
    )

    GreetingHeader(const MachineID from, const std::uint32_t size) noexcept
      : from{from}
      , message_size{size}
    {}

    template<typename Callable>
    static std::optional<MessageVariant> build(const std::vector<std::byte>& data, Callable read_from) noexcept
    {
      // Get the header and read the required number of bytes
      const GreetingHeader header{data};
      std::vector<std::byte> buffer(header.message_size);
      if (!read_from(buffer.data(), buffer.size()))
      {
        return {};
      }
      Greeting to_ret;
      to_ret.from = header.from;
      to_ret.neighbors = deserialize<std::vector<MachineID>>(buffer);
      return to_ret;
    }

    /// The machine that the greeting is from
    MachineID from;
    /// The size of the message that follows
    std::uint32_t message_size;
  }; // struct GreetingHeader

  struct GoodbyeHeader
  {
    SKYNET_MAKE_SERIALIZABLE_NO_OP(GoodbyeHeader)

    template<typename Callable>
    static std::optional<MessageVariant> build(const std::vector<std::byte>&  /* data */, Callable /* read_from */) noexcept
    {
      return Goodbye{};
    }
  }; // struct GoodbyeHeader

  /** \brief The header for the neighbor messages
   */
  struct BaseNeighborHeader
  {
    SKYNET_MAKE_SERIALIZABLE(BaseNeighborHeader,
      neighbor
    )

    // Allow construction with just the neighbor
    explicit BaseNeighborHeader(const MachineID id)
      : neighbor{id}
    {}

    template<typename RetType>
    static std::optional<MessageVariant> base_build(const std::vector<std::byte>& data) noexcept
    {
      const BaseNeighborHeader header{data};
      RetType to_ret;
      to_ret.neighbor_id = header.neighbor;
      return to_ret;
    }

    MachineID neighbor;
  }; // struct BaseNeighborHeader

  struct NewNeighborHeader : BaseNeighborHeader
  {
    template<typename Callable>
    static std::optional<MessageVariant> build(const std::vector<std::byte>& data, Callable /* read_from */) noexcept
    {
      return base_build<NewNeighbor>(data);
    }
  }; // struct NewNeighborHeader

  struct RemoveNeighborHeader : BaseNeighborHeader
  {
    template<typename Callable>
    static std::optional<MessageVariant> build(const std::vector<std::byte>& data, Callable /* read_from */) noexcept
    {
      return base_build<RemoveNeighbor>(data);
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

#endif // SKYNET_SRC_MESSAGE_HEADERS_HPP

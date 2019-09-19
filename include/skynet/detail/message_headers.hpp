#ifndef SKYNET_DETAIL_MESSAGE_HEADERS_HPP
#define SKYNET_DETAIL_MESSAGE_HEADERS_HPP

#include "skynet/types.hpp"
#include "skynet/detail/utility/serialize.hpp"
#include "skynet/detail/utility/type_list.hpp"

#include <cstdint>

namespace skynet { namespace detail
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
    template<typename Callable1, typename Callable2>
    static bool do_callback(const std::vector<char>& data, Callable1 callback, Callable2 read_from)
    {
      // Get the header and read the required number of bytes
      const auto header = from_bytes<BroadcastHeader>(data);
      std::vector<char> buffer(header.message_size);
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
    /// The tag that the message was sent with
    TagID tag_id;
    /// The id of the computer that the message is originally from
    MachineID origin;
    /// The number of further hops to do, plus 1
    /// A value of zero here means a global broadcast
    std::uint32_t hops_left_p1;
    /// The size of the message that follows
    std::uint32_t message_size;
  }; // struct BroadcastHeader

  template <class Archive>
  void serialize(Archive& ar, BroadcastHeader& h) noexcept
  {
    ar(h.message_id, h.job_id, h.tag_id, h.origin, h.hops_left_p1, h.message_size);
  }

  /** \brief The header for the greeting message
   */
  struct GreetingHeader
  {
    template<typename Callable1, typename Callable2>
    static bool do_callback(const std::vector<char>& data, Callable1 callback, Callable2 read_from)
    {
      // Get the header and read the required number of bytes
      const auto header = from_bytes<GreetingHeader>(data);
      std::vector<char> buffer(header.message_size);
      if (!read_from(buffer.data(), buffer.size()))
      {
        return false;
      }
      return callback(header, from_bytes<std::vector<MachineID>>(buffer));
    }

    /// The machine that the greeting is from
    MachineID from;
    /// The size of the message that follows
    std::uint32_t message_size;
  }; // struct GreetingHeader

  template <class Archive>
  void serialize(Archive& ar, GreetingHeader& h) noexcept
  {
    ar(h.from, h.message_size);
  }

  struct GoodbyeHeader
  {
    template<typename Callable1, typename Callable2>
    static bool do_callback(const std::vector<char>& /* data */, Callable1 callback, Callable2 /* read_from */)
    {
      return callback(GoodbyeHeader{});
    }
  }; // struct GoodbyeHeader

  template <class Archive>
  void serialize(Archive& /* ar */, GoodbyeHeader& /* h */) noexcept
  {}

  /** \brief The header for the neighbor messages
   */
  struct BaseNeighborHeader
  {
    template<typename Derived, typename Callable1, typename Callable2>
    static bool base_callback(const std::vector<char>& data, Callable1 callback, Callable2 /* read_from */)
    {
      const auto header = from_bytes<BaseNeighborHeader>(data);
      return callback(static_cast<const Derived&>(header));
    }

    MachineID neighbor;
  }; // struct BaseNeighborHeader

  template <class Archive>
  void serialize(Archive& ar, BaseNeighborHeader& h) noexcept
  {
    ar(h.neighbor);
  }

  struct NewNeighborHeader : BaseNeighborHeader
  {
    template<typename Callable1, typename Callable2>
    static bool do_callback(const std::vector<char>& data, Callable1 callback, Callable2 read_from)
    {
      return base_callback<NewNeighborHeader>(data, callback, read_from);
    }
  }; // struct NewNeighborHeader

  struct RemoveNeighborHeader: BaseNeighborHeader
  {
    template<typename Callable1, typename Callable2>
    static bool do_callback(const std::vector<char>& data, Callable1 callback, Callable2 read_from)
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
} } // namespace skynet::detail

#endif // SKYNET_DETAIL_MESSAGE_HEADERS_HPP

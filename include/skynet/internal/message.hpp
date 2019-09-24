#ifndef SKYNET_INTERNAL_MESSAGE_HPP
#define SKYNET_INTERNAL_MESSAGE_HPP

#include "skynet/internal/utility/algorithms.hpp"
#include "skynet/internal/utility/overload_set.hpp"

#include <array>
#include <cassert>
#include <cstring>
#include <optional>
#include <vector>

// namespace skynet::internal::header_info {
// // Base size for reading a UniversalHeader
// constexpr int base_size = ...;
// // Additional number of bytes to read to read the corresponding header
// constexpr std::array<int, ...> continue_sizes{...};
// }
#include "message_header_information.hpp"

namespace skynet::internal
{
  /** \brief Create data for a broadcast
   */
  std::vector<std::byte> make_broadcast(
    const MessageID message_id,
    const JobID job_id,
    const TagID tag_id,
    const TagIndex tag_index,
    const MachineID origin,
    const std::uint32_t hops_left_p1,
    const std::vector<std::byte>& data
  ) noexcept
  {
    BroadcastHeader base;
    base.message_id = message_id;
    base.job_id = job_id;
    base.tag_id = tag_id;
    base.tag_index = tag_index;
    base.origin = origin;
    base.hops_left_p1 = hops_left_p1;
    base.message_size = data.size();
    return concatenate(
      UniversalHeader{header_index<BroadcastHeader>()}.to_bytes(),
      base.to_bytes(),
      data
    );
  }

  /** \brief Rebuilds a broadcast message for resending
   */
  std::vector<std::byte> rebuild_broadcast(const BroadcastHeader& b, const std::vector<std::byte>& data)
  {
    return concatenate(
      UniversalHeader{header_index<BroadcastHeader>()}.to_bytes(),
      b.to_bytes(),
      data
    );
  }

  /** \brief Create data for a greeting
   */
  std::vector<std::byte> make_greeting(
    const MachineID from,
    const std::vector<MachineID>& neighbors
  ) noexcept
  {
    const auto neighbor_data = Serializer{}.add(neighbors).bytes();
    return concatenate(
      UniversalHeader{header_index<GreetingHeader>()}.to_bytes(),
      GreetingHeader{from, static_cast<std::uint32_t>(neighbor_data.size())}.to_bytes(),
      neighbor_data
    );
  }

  /** \brief Create data for a goodbyte
   */
  std::vector<std::byte> make_goodbye() noexcept
  {
    return concatenate(
      UniversalHeader{header_index<GoodbyeHeader>()}.to_bytes(),
      GoodbyeHeader{}.to_bytes()
    );
  }

  /** \brief Create data for a neighbor notification
   */
  template<typename Type>
  std::vector<std::byte> make_neighbor_notification(const MachineID neighbor) noexcept
  {
    return concatenate(
      UniversalHeader{header_index<Type>()}.to_bytes(),
      static_cast<Type&&>(BaseNeighborHeader{neighbor}).to_bytes()
    );
  }

  /** \brief Class for processing and reading messages
   */
  class MessageHandler
  {
  public:
    /** \brief Attempt to read a message
     *
     * \param read_from A callable of the signature `bool (char*, std::size_t)`
     * that writes the specified number of bytes, returning true if it was
     * successful, false otherwise.
     */
    template<typename Callable1, typename Callable2>
    static std::optional<MessageHandler> try_to_get(Callable1 read_from, Callable2 on_error)
    {
      MessageHandler to_ret;
      // Start by loading the universal header
      std::array<std::byte, header_info::base_size> buf;
      if (!read_from(buf.data(), buf.size()))
      {
        return {};
      }
      to_ret.start_ = UniversalHeader{buf};
      if (to_ret.start_.index >= num_headers)
      {
        on_error();
        return {};
      }
      // Load the next header
      const auto bytes_to_read = header_info::continue_sizes[to_ret.start_.index];
      to_ret.data_.resize(bytes_to_read);
      // Don't try reading more if there's nothing to read
      if (bytes_to_read > 0)
      {
        // Couldn't read the second header, which should never happen
        if (!read_from(to_ret.data_.data(), to_ret.data_.size()))
        {
          on_error();
          return {};
        }
      }
      return to_ret;
    }

    /** \brief Return the category of header that this has
     */
    MessageCategory category() const noexcept
    {
      if (start_.index < size<JobHeaders>)
      {
        return MessageCategory::job;
      }
      return MessageCategory::status;
    }

    /** \brief Perform a callback for job category messages
     *
     * \pre category() == MessageCategory::job
     */
    template<typename Callable, typename... T>
    bool do_job_callback(Callable read_from, T&&... callbacks) const
    {
      assert(category() == MessageCategory::job);
      const auto callback_set = make_overload_set(std::forward<T>(callbacks)...);
      using callback_type = decltype(callback_set);
      constexpr auto ptrs = make_job_callback_array<callback_type, Callable>(JobHeaders{});
      return ptrs[start_.index](data_, callback_set, read_from);
    }

    /** \brief Perform a callback for job category messages
     *
     * \pre category() == MessageCategory::status
     */
    template<typename Callable, typename... T>
    bool do_status_callback(Callable read_from, T&&... callbacks) const
    {
      assert(category() == MessageCategory::status);
      const auto callback_set = make_overload_set(std::forward<T>(callbacks)...);
      using callback_type = decltype(callback_set);
      constexpr auto ptrs = make_job_callback_array<callback_type, Callable>(StatusHeaders{});
      // Need to remember to adjust the index for status headers
      return ptrs[start_.index - size<JobHeaders>](data_, callback_set, read_from);
    }

  private:
    // Don't allow public construction
    MessageHandler() = default;

    // Create a callback array
    template<typename OverloadSet, typename ReadFrom, typename... Headers>
    static constexpr auto make_job_callback_array(TypeList<Headers...>) noexcept
    {
      using call_type = bool (*)(const std::vector<std::byte>&, OverloadSet, ReadFrom);
      return std::array<call_type, sizeof...(Headers)>{
        &Headers::template do_callback<OverloadSet, ReadFrom>...
      };
    }

    // The data of the following header
    std::vector<std::byte> data_;

    // For knowing what type of header follows
    UniversalHeader start_;
  }; // class MessageHandler
} // namespace skynet::internal

#endif // SKYNET_INTERNAL_MESSAGE_HPP

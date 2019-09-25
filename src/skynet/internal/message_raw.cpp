#include "skynet/internal/utility/algorithms.hpp"
#include "skynet/internal/utility/overload_set.hpp"
#include "skynet/internal/message.hpp"
#include "skynet/master.hpp"

// namespace skynet::internal::header_info {
// // Base size for reading a UniversalHeader
// constexpr int base_size = ...;
// // Additional number of bytes to read to read the corresponding header
// constexpr std::array<int, ...> continue_sizes{...};
// }
#include "message_header_information.hpp"

#include <array>
#include <cassert>
#include <cstring>
#include <optional>
#include <vector>

namespace skynet::internal
{
  namespace
  {
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

    /** \brief Actual implementation for creating the variant, since the list
     * of header types is needed, it needs to be a seperate function
     */
    template<typename Callable, typename... HeaderTypes>
    std::optional<MessageVariant> get_message_impl(
      TypeList<HeaderTypes...>,
      const std::uint8_t header_index,
      const std::vector<std::byte>& buffer,
      Callable read_from
    ) noexcept
    {
      // Invalid indexes should have already been discarded
      assert(header_index < sizeof...(HeaderTypes));
      // Create an array for each of the header's functions that create
      // their corresponding message
      using ptr_type =
        std::optional<MessageVariant> (*)(const std::vector<std::byte>&, Callable) noexcept;
      static constexpr std::array<ptr_type, sizeof...(HeaderTypes)> call_ptrs{
        &HeaderTypes::build...
      };
      // Then just call the correct one
      return call_ptrs[header_index](buffer, read_from);
    }
  } // end namespace {anonymous}

  ////////////////////////////////////
  // Make functions
  ////////////////////////////////////

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

  std::vector<std::byte> make_goodbye() noexcept
  {
    return concatenate(
      UniversalHeader{header_index<GoodbyeHeader>()}.to_bytes(),
      GoodbyeHeader{}.to_bytes()
    );
  }

  std::vector<std::byte> make_new_neighbor(const MachineID neighbor) noexcept
  {
    return make_neighbor_notification<NewNeighborHeader>(neighbor);
  }

  std::vector<std::byte> make_remove_neighbor(const MachineID neighbor) noexcept
  {
    return make_neighbor_notification<RemoveNeighborHeader>(neighbor);
  }

  /////////////////////////////////////
  // MessageHandler
  /////////////////////////////////////
  struct MessageHandler::Impl
  {
    // Message header for various information
    UniversalHeader header;
    // buffer for the bytes
    std::vector<std::byte> buffer;
  };

  std::optional<MessageHandler> MessageHandler::try_to_get(ExternalMaster& from) noexcept
  {
    // Start by trying to load the universal header
    std::array<std::byte, header_info::base_size> buf;
    if (!ExternalMaster::Accessor::read(from, buf.data(), buf.size()))
    {
      return {};
    }
    MessageHandler to_ret;
    to_ret.impl_->header = UniversalHeader{buf};
    const auto& header = to_ret.impl_->header;
    // Make sure it's a valid number
    if (header.index() >= num_headers)
    {
      from.mark_as_dead();
      return {};
    }
    // Now load the next header, if required
    const auto bytes_to_read = header_info::continue_sizes[header.index()];
    if (bytes_to_read > 0)
    {
      auto& data = to_ret.impl_->buffer;
      data.resize(bytes_to_read);
      // If the second header can't be read then it's a fatal error
      if (!ExternalMaster::Accessor::read(from, data.data(), data.size()))
      {
        from.mark_as_dead();
        return {};
      }
    }
    return to_ret;
  }

  // Initializer just needs to create an object
  MessageHandler::MessageHandler() noexcept
    : impl_{std::make_unique<Impl>()}
  {}

  // These can just be defaulted
  MessageHandler::MessageHandler(MessageHandler&&) noexcept = default;
  MessageHandler& MessageHandler::operator=(MessageHandler&&) noexcept = default;
  MessageHandler::~MessageHandler() = default;

  MessageCategory MessageHandler::category() const noexcept
  {
    // This is simply index based
    if (impl_->header.index() < size<JobHeaders>)
    {
      return MessageCategory::job;
    }
    return MessageCategory::status;
  }

  std::optional<MessageVariant> MessageHandler::get_message(ExternalMaster& from) const noexcept
  {
    return get_message_impl(
      Append<JobHeaders, StatusHeaders>{},
      impl_->header.index(),
      impl_->buffer,
      [&from](std::byte* const buffer, const std::size_t count) {
        return ExternalMaster::Accessor::read(from, buffer, count);
      }
    );
  }

  std::vector<std::byte> MessageHandler::rebuild_broadcast(const Broadcast& b) const noexcept
  {
    BroadcastHeader to_send;
    to_send.message_id = b.message_id;
    to_send.job_id = b.job_id;
    to_send.tag_id = b.tag_id;
    to_send.tag_index = b.tag_index;
    to_send.origin = b.origin;
    to_send.hops_left_p1 = b.hops_left_p1 - 1;
    to_send.message_size = b.data.size();
    return concatenate(
      UniversalHeader{header_index<BroadcastHeader>()}.to_bytes(),
      to_send.to_bytes(),
      b.data
    );
  }
} // namespace skynet::internal

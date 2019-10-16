#include "skynet/internal/message_creators.hpp"

#include "skynet/internal/utility/network_conv.hpp"
#include "skynet/internal/capn_proto_wrapper.hpp"
#include "skynet/types.hpp"

#include "message_format.capnp.h"

#include <capnp/serialize.h>

#include <cstring>
#include <string>
#include <type_traits>

namespace skynet::internal
{
  namespace
  {
    // Creates a vector of bytes with a size pre-prended ready to be sent over the network
    std::vector<std::byte> finalize_message(capnp::MallocMessageBuilder& builder) noexcept
    {
      // Calculate the sizes
      constexpr auto net_size = sizeof(NetworkSizeType);
      const std::size_t msg_size = capnp::computeSerializedSizeInWords(builder) * sizeof(std::size_t);
      const std::size_t buf_size = net_size + msg_size;

      // Write the message to a buffer
      std::vector<std::byte> buffer_data(buf_size);
      kj::NullArrayDisposer null_disposer{};
      kj::Array<kj::byte> buffer{
        reinterpret_cast<kj::byte*>(buffer_data.data()) + net_size,
        buffer_data.size() - net_size,
        null_disposer
      };
      kj::ArrayOutputStream out_s{buffer};
      capnp::writeMessage(out_s, builder);

      // Write the size
      const auto size_bytes = to_network_bytes(msg_size);
      std::memcpy(buffer_data.data(), &size_bytes, sizeof(size_bytes));
      return buffer_data;
    }
  } // namespace {anonymous}

  /** \brief Create data for a publish
   */
  std::vector<std::byte> make_publish(
    const MessageID message_id,
    const TagID& tag_id,
    const MachineID& origin,
    const std::uint8_t hops_left_p1,
    const PublishDataVariant& data
  ) noexcept
  {
    capnp::MallocMessageBuilder builder;
    auto message = builder.initRoot<cpnpro::Message>().initPublish();
    // Just set the data now
    message.setMessageID(message_id);
    message.setTagID(tag_id);
    message.setOrigin(origin);
    message.setHopsLeftP1(hops_left_p1);
    auto publish_data = message.initData();
    std::visit(
      [&](const auto& value) {
        using ValueType = std::remove_cv_t<std::remove_reference_t<decltype(value)>>;
        detail::PublishDataHandler<ValueType>::set(publish_data, value);
      },
      data
    );
    return finalize_message(builder);
  }

  /** \brief Create data for a greeting
   */
  std::vector<std::byte> make_greeting(
    const MachineID& from,
    const std::vector<MachineID>& neighbors
  ) noexcept
  {
    capnp::MallocMessageBuilder builder;
    auto message = builder.initRoot<cpnpro::Message>().initGreeting();
    message.setFrom(from);
    auto to_set = message.initNeighbors(neighbors.size());
    for (std::size_t i = 0; i < neighbors.size(); ++i)
    {
      to_set.set(i, neighbors[i]);
    }
    return finalize_message(builder);
  }

  /** \brief Create data for a goodbyte
   */
  std::vector<std::byte> make_goodbye() noexcept
  {
    capnp::MallocMessageBuilder builder;
    builder.initRoot<cpnpro::Message>().setGoodbye();
    return finalize_message(builder);
  }

  /** \brief Create data for a new neighbor notification
   */
  std::vector<std::byte> make_new_neighbor(const MachineID& neighbor) noexcept
  {
    capnp::MallocMessageBuilder builder;
    auto message = builder.initRoot<cpnpro::Message>().initNewNeighbor();
    message.setNeighborID(neighbor);
    return finalize_message(builder);
  }

  /** \brief Create data for a removed neighbor notification
   */
  std::vector<std::byte> make_remove_neighbor(const MachineID& neighbor) noexcept
  {
    capnp::MallocMessageBuilder builder;
    auto message = builder.initRoot<cpnpro::Message>().initRemoveNeighbor();
    message.setNeighborID(neighbor);
    return finalize_message(builder);
  }

  /** \brief Create data for a heartbeat
   */
  std::vector<std::byte> make_heartbeat() noexcept
  {
    capnp::MallocMessageBuilder builder;
    builder.initRoot<cpnpro::Message>().setHeartbeat();
    return finalize_message(builder);
  }
} // namespace skynet::internal

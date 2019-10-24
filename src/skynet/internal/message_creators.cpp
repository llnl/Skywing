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
    const VersionID version,
    const TagID& tag_id,
    const MachineID& origin,
    const std::uint8_t hops_left_p1,
    const PublishValueVariant& data
  ) noexcept
  {
    capnp::MallocMessageBuilder builder;
    auto message = builder.initRoot<cpnpro::Publish>().initData();
    // Just set the data now
    message.setVersion(version);
    message.setTagID(tag_id);
    message.setOrigin(origin);
    message.setHopsLeftP1(hops_left_p1);
    auto publish_value = message.initValue();
    std::visit(
      [&](const auto& value) {
        using ValueType = std::remove_cv_t<std::remove_reference_t<decltype(value)>>;
        detail::PublishValueHandler<ValueType>::set(publish_value, value);
      },
      data
    );
    return finalize_message(builder);
  }

  /** \brief Create data to signify that a publication channel is closing
   */
  std::vector<std::byte> make_close_publish() noexcept
  {
    capnp::MallocMessageBuilder builder;
    builder.initRoot<cpnpro::Publish>().setClosingConnection();
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
    auto message = builder.initRoot<cpnpro::StatusMessage>().initGreeting();
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
    builder.initRoot<cpnpro::StatusMessage>().setGoodbye();
    return finalize_message(builder);
  }

  /** \brief Create data for a new neighbor notification
   */
  std::vector<std::byte> make_new_neighbor(const MachineID& neighbor) noexcept
  {
    capnp::MallocMessageBuilder builder;
    auto message = builder.initRoot<cpnpro::StatusMessage>().initNewNeighbor();
    message.setNeighborID(neighbor);
    return finalize_message(builder);
  }

  /** \brief Create data for a removed neighbor notification
   */
  std::vector<std::byte> make_remove_neighbor(const MachineID& neighbor) noexcept
  {
    capnp::MallocMessageBuilder builder;
    auto message = builder.initRoot<cpnpro::StatusMessage>().initRemoveNeighbor();
    message.setNeighborID(neighbor);
    return finalize_message(builder);
  }

  /** \brief Create data for a heartbeat
   */
  std::vector<std::byte> make_heartbeat() noexcept
  {
    capnp::MallocMessageBuilder builder;
    builder.initRoot<cpnpro::StatusMessage>().setHeartbeat();
    return finalize_message(builder);
  }

  std::vector<std::byte> make_tag_publishers(
    const std::vector<TagID>& tags,
    const std::vector<std::vector<MachineID>>& machines
  ) noexcept
  {
    capnp::MallocMessageBuilder builder;
    auto message = builder.initRoot<cpnpro::StatusMessage>().initTagPublishers();
    auto msg_tags = message.initMachines(tags.size());
    for (std::size_t i = 0; i < tags.size(); ++i)
    {
      msg_tags.set(i, tags[i]);
    }
    auto msg_machines = message.initTags(machines.size());
    for (std::size_t i = 0; i < tags.size(); ++i)
    {
      auto machine_to_set = msg_machines.init(i, machines[i].size());
      for (std::size_t j = 0; j < tags[i].size(); ++j)
      {
        machine_to_set.set(j, machines[i][j]);
      }
    }
    return finalize_message(builder);
  }

  /** \brief Create data for a request for producers of a tag
   */
  std::vector<std::byte> make_get_publishers(
    const std::vector<TagID>& tags
  ) noexcept
  {
    capnp::MallocMessageBuilder builder;
    auto message = builder.initRoot<cpnpro::StatusMessage>().initGetPublishers();
    auto msg_tags = message.initTags(tags.size());
    for (std::size_t i = 0; i < tags.size(); ++i)
    {
      msg_tags.set(i, tags[i]);
    }
    return finalize_message(builder);
  }
} // namespace skynet::internal

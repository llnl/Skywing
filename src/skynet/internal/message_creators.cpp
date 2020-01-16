#include "skynet/internal/message_creators.hpp"

#include "skynet/internal/utility/network_conv.hpp"
#include "skynet/internal/capn_proto_wrapper.hpp"
#include "skynet/types.hpp"
#include "publish_value_handler.hpp"

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

    void set_publish_data(
      cpnpro::PublishData::Builder to_set,
      const VersionID version,
      const TagID& tag_id,
      const PublishValueVariant& value
    ) noexcept
    {
      to_set.setVersion(version);
      to_set.setTagID(tag_id);
      auto publish_value = to_set.initValue();
      std::visit(
        [&](const auto& data) {
          using ValueType = std::remove_cv_t<std::remove_reference_t<decltype(data)>>;
          detail::PublishValueHandler<ValueType>::set(publish_value, data);
        },
        value
      );
    }
  } // namespace {anonymous}

  /** \brief Create data for a publish
   */
  std::vector<std::byte> make_publish(
    const VersionID version,
    const TagID& tag_id,
    const PublishValueVariant& value
  ) noexcept
  {
    capnp::MallocMessageBuilder builder;
    auto message = builder.initRoot<cpnpro::Publish>().initData();
    set_publish_data(message, version, tag_id, value);
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
    const std::vector<MachineID>& neighbors,
    const std::uint16_t base_port
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
    message.setBasePort(base_port);
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

  std::vector<std::byte> make_report_publishers(
    const std::vector<TagID>& tags,
    const std::vector<std::vector<std::string>>& addresses,
    const std::vector<TagID>& locally_produced_tags
  ) noexcept
  {
    capnp::MallocMessageBuilder builder;
    auto message = builder.initRoot<cpnpro::StatusMessage>().initReportPublishers();
    auto msg_tags = message.initTags(tags.size());
    for (std::size_t i = 0; i < tags.size(); ++i)
    {
      msg_tags.set(i, tags[i]);
    }
    auto msg_addresses = message.initAddresses(addresses.size());
    for (std::size_t i = 0; i < tags.size(); ++i)
    {
      auto address_to_set = msg_addresses.init(i, addresses[i].size());
      for (std::size_t j = 0; j < addresses[i].size(); ++j)
      {
        address_to_set.set(j, addresses[i][j]);
      }
    }
    auto msg_local_tags = message.initLocallyProducedTags(locally_produced_tags.size());
    for (std::size_t i = 0; i < locally_produced_tags.size(); ++i)
    {
      msg_local_tags.set(i, locally_produced_tags[i]);
    }
    return finalize_message(builder);
  }

  /** \brief Create data for a request for producers of a tag
   */
  std::vector<std::byte> make_get_publishers(
    const std::vector<TagID>& tags,
    const bool ignore_cache
  ) noexcept
  {
    capnp::MallocMessageBuilder builder;
    auto message = builder.initRoot<cpnpro::StatusMessage>().initGetPublishers();
    auto msg_tags = message.initTags(tags.size());
    for (std::size_t i = 0; i < tags.size(); ++i)
    {
      msg_tags.set(i, tags[i]);
    }
    message.setIgnoreCache(ignore_cache);
    return finalize_message(builder);
  }

  std::vector<std::byte> make_join_reduce_group(
    const TagID& reduce_tag,
    const TagID& tag_produced
  ) noexcept
  {
    capnp::MallocMessageBuilder builder;
    auto message = builder.initRoot<cpnpro::StatusMessage>().initJoinReduceGroup();
    message.setReduceTag(reduce_tag);
    message.setTagProduced(tag_produced);
    return finalize_message(builder);
  }

  std::vector<std::byte> make_submit_reduce_value(
    const TagID& reduce_tag,
    const VersionID version,
    const TagID& tag_id,
    const PublishValueVariant& value
  ) noexcept
  {
    capnp::MallocMessageBuilder builder;
    auto message = builder.initRoot<cpnpro::StatusMessage>().initSubmitReduceValue();
    message.setReduceTag(reduce_tag);
    auto publish_data = message.initData();
    set_publish_data(publish_data, version, tag_id, value);
    return finalize_message(builder);
  }

  std::vector<std::byte> make_report_reduce_result(
    const TagID& reduce_tag,
    const VersionID version,
    const TagID& tag_id,
    const PublishValueVariant& value
  ) noexcept
  {
    capnp::MallocMessageBuilder builder;
    auto message = builder.initRoot<cpnpro::StatusMessage>().initReportReduceResult();
    message.setReduceTag(reduce_tag);
    auto publish_data = message.initData();
    set_publish_data(publish_data, version, tag_id, value);
    return finalize_message(builder);
  }

  std::vector<std::byte> make_report_reduce_disconnection(
    const TagID& reduce_tag,
    const MachineID& initiating_machine,
    ReductionDisconnectID disconnection_id
  ) noexcept
  {
    capnp::MallocMessageBuilder builder;
    auto message = builder.initRoot<cpnpro::StatusMessage>().initReportReduceDisconnection();
    message.setReduceTag(reduce_tag);
    message.setInitiatingMachine(initiating_machine);
    message.setId(disconnection_id);
    return finalize_message(builder);
  }
} // namespace skynet::internal

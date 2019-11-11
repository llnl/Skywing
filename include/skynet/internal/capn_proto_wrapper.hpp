#ifndef SKYNET_INTERNAL_CAPN_PROTO_WRAPPER_HPP
#define SKYNET_INTERNAL_CAPN_PROTO_WRAPPER_HPP

// This header exists to allow more convienent and (within the codebase)
// conventional access to the Cap'n Proto messages

#include "message_format.capnp.h"

#include <capnp/serialize.h>

#include "skynet/internal/utility/overload_set.hpp"
#include "skynet/types.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace skynet::internal
{
  /** \brief Class representing values that can be published
   */
  class PublishValue
  {
  public:
    /** \brief Return the held value as a variant
     */
    std::optional<PublishValueVariant> get_variant() const noexcept;

  private:
    // Only allow PublishData to construct this
    friend class PublishData;
    explicit PublishValue(cpnpro::PublishData::Value::Reader reader) noexcept;

    cpnpro::PublishData::Value::Reader r;
  };

  /** \brief Class representing a publish message
   */
  class PublishData
  {
  public:
    VersionID version() const noexcept;
    TagID tag_id() const noexcept;
    PublishValue value() const noexcept;

  private:
    cpnpro::PublishData::Reader r;

    friend class PublishMessageHandler;
    explicit PublishData(cpnpro::PublishData::Reader reader) noexcept;
  };

  /** \brief Class representing a greeting message
   */
  class Greeting
  {
  public:
    MachineID from() const noexcept;
    std::vector<MachineID> neighbors() const noexcept;
    std::uint16_t base_port() const noexcept;

  private:
    cpnpro::Greeting::Reader r;

    friend class StatusMessageHandler;
    explicit Greeting(cpnpro::Greeting::Reader reader) noexcept;
  };

  /** \brief Class representing a goodbye message
   */
  class Goodbye
  {
    // Intentionally empty
  };

  /** \brief Class representing a new neighbor message
   */
  class NewNeighbor
  {
  public:
    MachineID neighbor_id() const noexcept;

  private:
    cpnpro::NewNeighbor::Reader r;

    friend class StatusMessageHandler;
    explicit NewNeighbor(cpnpro::NewNeighbor::Reader reader) noexcept;
  };

  /** \brief Class representing a remove neighbor message
   */
  class RemoveNeighbor
  {
  public:
    MachineID neighbor_id() const noexcept;

  private:
    cpnpro::RemoveNeighbor::Reader r;

    friend class StatusMessageHandler;
    explicit RemoveNeighbor(cpnpro::RemoveNeighbor::Reader reader) noexcept;
  };

  /** \brief Class representing a heartbeat
   */
  class Heartbeat
  {
    // Intentionally empty
  };

  /** \brief Class representing information on which machines produce what tags
   */
  class ReportPublishers
  {
  public:
    std::vector<TagID> tags() const noexcept;
    std::vector<std::vector<std::string>> addresses() const noexcept;
    std::vector<TagID> locally_produced_tags() const noexcept;

  private:
    cpnpro::ReportPublishers::Reader r;

    friend class StatusMessageHandler;
    explicit ReportPublishers(cpnpro::ReportPublishers::Reader reader) noexcept;
  };

  /** \brief Request information for which machines produce which tags
   */
  class GetPublishers
  {
  public:
    std::vector<TagID> tags() const noexcept;
    bool ignore_cache() const noexcept;

  private:
    cpnpro::GetPublishers::Reader r;

    friend class StatusMessageHandler;
    explicit GetPublishers(cpnpro::GetPublishers::Reader reader) noexcept;
  };

  /** \brief Message for when a machine join a reduce group
   */
  class JoinReduceGroup
  {
  public:
    TagID reduce_tag() const noexcept;
    std::vector<TagID> produced_tags() const noexcept;

  private:
    cpnpro::JoinReduceGroup::Reader r;

    friend class StatusMessageHandler;
    explicit JoinReduceGroup(cpnpro::JoinReduceGroup::Reader reader) noexcept;
  };

  /** \brief Class for converting the raw bytes of a message into a useable format
   */
  class StatusMessageHandler
  {
  public:
    /** \brief Construct a message handler from a raw set of bytes
     */
    static std::optional<StatusMessageHandler> try_to_create(const std::vector<std::byte>& data) noexcept;

    // Moveable only
    StatusMessageHandler() noexcept;
    StatusMessageHandler(const StatusMessageHandler&) = delete;
    StatusMessageHandler& operator=(const StatusMessageHandler&) = delete;
    StatusMessageHandler(StatusMessageHandler&&) noexcept;
    StatusMessageHandler& operator=(StatusMessageHandler&&) noexcept;

    /** \brief Perform a callback on the stored message
     *
     * Returns true if the callback was successful, false otherwise
     */
    template<typename... Ts>
    bool do_callback(Ts&&... callbacks) const noexcept
    {
      if (const auto msg = extract_message())
      {
        return std::visit(make_overload_set(std::forward<Ts>(callbacks)...), *msg);
      }
      return false;
    }

  private:
    // The types of messages that can be produced
    using MessageVariant = std::variant<
      Greeting,
      Goodbye,
      NewNeighbor,
      RemoveNeighbor,
      Heartbeat,
      ReportPublishers,
      GetPublishers,
      JoinReduceGroup
    >;

    // Process the stored message and return its internal type
    std::optional<MessageVariant> extract_message() const noexcept;

    // capnp::MallocMessageBuilder isn't copyable or movable, but needs to be
    // contained in this structure; use PIMPL to solve this
    // Impl needs to be defined here since this object is being returned as
    // an optional, which requires it to be complete
    struct Impl
    {
      kj::NullArrayDisposer null_disposer;
      capnp::MallocMessageBuilder message;
      cpnpro::StatusMessage::Reader root;
    };
    std::unique_ptr<Impl> impl_;
  };

  /** Class for converting raw bytes of a publish message into a usable format
   */
  class PublishMessageHandler
  {
  public:
    /** \brief Construct a message handler from a raw set of bytes
     */
    static std::optional<PublishMessageHandler> try_to_create(const std::vector<std::byte>& data) noexcept;

    // Moveable, not copyable
    PublishMessageHandler() noexcept;
    PublishMessageHandler(const PublishMessageHandler&) = delete;
    PublishMessageHandler& operator=(const PublishMessageHandler&) = delete;
    PublishMessageHandler(PublishMessageHandler&&) noexcept;
    PublishMessageHandler& operator=(PublishMessageHandler&&) noexcept;

    /** \brief Gets the published data, or if it was a shutdown message, no data
     */
    std::optional<PublishData> data() const noexcept;

  private:
    // Same thing as above
    struct Impl
    {
      kj::NullArrayDisposer null_disposer;
      capnp::MallocMessageBuilder message;
      cpnpro::Publish::Reader root;
    };
    std::unique_ptr<Impl> impl_;
  };
} // namespace skynet::internal

#endif // SKYNET_INTERNAL_CAPN_PROTO_WRAPPER_HPP

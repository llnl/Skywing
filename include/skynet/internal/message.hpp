#ifndef SKYNET_INTERNAL_MESSAGE_HPP
#define SKYNET_INTERNAL_MESSAGE_HPP

#include "skynet/internal/utility/overload_set.hpp"
#include "skynet/types.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace skynet::internal
{
  /** \brief The category that each message belongs in
   */
  enum class MessageCategory : std::uint8_t
  {
    job,
    status
  };

  /** \brief Broadcast; the information within is not deserialized
   */
  struct Broadcast
  {
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
    /// The raw broadcast data
    std::vector<std::byte> data;
  };

  /** \brief The message that a machine sends when it first connects
   */
  struct Greeting
  {
    /// The machine that the greeting is from
    MachineID from;
    /// The list of machines that the machine is connected to
    std::vector<MachineID> neighbors;
  };

  /** \brief The message that a machine sends before it disconnects
   * (no data currently)
   */
  struct Goodbye{};

  /** \brief The message that a machine sends when it has a new neighbor
   */
  struct NewNeighbor
  {
    /// The id of the neighbor
    MachineID neighbor_id;
  };

  /** \brief The message that a machine sends when it loses a neighbor
   */
  struct RemoveNeighbor
  {
    /// The id of the neighbor
    MachineID neighbor_id;
  };

  /** \brief A variant holding all of the message types
   */
  using MessageVariant = std::variant<
    Broadcast,
    Greeting,
    Goodbye,
    NewNeighbor,
    RemoveNeighbor
  >;

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
  ) noexcept;

  /** \brief Create data for a greeting
   */
  std::vector<std::byte> make_greeting(
    const MachineID from,
    const std::vector<MachineID>& neighbors
  ) noexcept;

  /** \brief Create data for a goodbyte
   */
  std::vector<std::byte> make_goodbye() noexcept;

  /** \brief Create data for a new neighbor notification
   */
  std::vector<std::byte> make_new_neighbor(const MachineID neighbor) noexcept;

  /** \brief Create data for a removed neighbor notification
   */
  std::vector<std::byte> make_remove_neighbor(const MachineID neighbor) noexcept;

  // Forward declaration of an external master
  class ExternalMaster;

  /** \brief Class for processing and reading messages
   */
  class MessageHandler
  {
  public:
    /** \brief Attempt to read a message from the specified external master
     */
    static std::optional<MessageHandler> try_to_get(ExternalMaster& from) noexcept;

    // Allow moving
    MessageHandler(MessageHandler&&) noexcept;
    MessageHandler& operator=(MessageHandler&&) noexcept;

    // Must be defined because of PIMPL
    ~MessageHandler();

    /** \brief Return the category of header that this has
     */
    MessageCategory category() const noexcept;

    /** \brief Perform a callback on the stored message
     *
     * Note that this can only be done once per object
     */
    template<typename... Ts>
    bool do_callback(ExternalMaster& from, Ts&&... callbacks) const noexcept
    {
      auto callback_set = make_overload_set(std::forward<Ts>(callbacks)...);
      if (auto msg = get_message(from))
      {
        return std::visit(callback_set, std::move(*msg));
      }
      return false;
    }

    /** \brief Get the data to propagate for a broadcast
     */
    std::vector<std::byte> rebuild_broadcast(const Broadcast&) const noexcept;

  private:
    // Don't allow public construction
    MessageHandler() noexcept;

    // Retrieve a message from the line
    std::optional<MessageVariant> get_message(ExternalMaster& from) const noexcept;

    // PIMPL since the backend can change
    struct Impl;
    std::unique_ptr<Impl> impl_;
  }; // class MessageHandler
} // namespace skynet::internal

#endif // SKYNET_INTERNAL_MESSAGE_HPP

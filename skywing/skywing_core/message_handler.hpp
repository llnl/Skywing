#ifndef SKYWING_MESSAGE_HANDLER_HPP
#define SKYWING_MESSAGE_HANDLER_HPP

#include <chrono>
#include <unordered_map>
#include <vector>

#include "skywing_core/internal/capn_proto_wrapper.hpp"
#include "skywing_core/internal/devices/socket_communicator.hpp"
#include "skywing_core/types.hpp"

namespace skywing
{
class Manager;

namespace internal
{
class NeighborAgent;

/** \brief Handles message-sending and message-receiving between Skywing agents.
 *
 *  Messages need to get sent between agents for tasks such as finding
 *  publishers of data, publishing new data under a tag, sending
 *  heartbeats to maintain liveness, and reporting information about
 *  other agents. This class handles organizing and sending these messages.
 */
class MessageHandler
{
public:
  /** \brief Build a MessageHandler.
   *
   * \param manager A reference to the Manager that builds (and owns)
   * this MessageHandler.
   */
  MessageHandler(Manager& manager) noexcept;

    /** \brief Check for any messages from the neighbor and handle them.
     *
     * \param nbr_agent The neighbor from which to check for messages.
     */
  void get_and_handle_messages(internal::NeighborAgent& nbr_agent) noexcept;

    /** \brief Sends a raw message to the other manager
     *
     * Also marks the connection as dead if any errors occur.  Does nothing
     * if the connection is marked as dead.
     *
     * \param nbr_agent The neighbor to which to send the message.
     * \param msg The raw bytes of the message to send.
     */
  void send_message(internal::NeighborAgent& nbr_agent,
		    const std::vector<std::byte>& msg) noexcept;

  /** \brief Sends a heartbeat ping to the neighbor if it hasn't been
   *   heard from in a bit.
   *
   * \param nbr_agent The neighbor to which to send the heartbeat ping.
   * \param interval The time threshold to trigger the ping.
   */
  void send_heartbeat_if_past_interval(internal::NeighborAgent& nbr_agent,
        std::chrono::milliseconds interval) noexcept;

  /** \brief Begins the search process for publishers of the specified tags.
   *
   * \param neighbors The neighbors to ask for publishers.
   * \param tags The tags seeking publishers.
   * \param publishers_needed TODO figure out what this does (sorry everyone)
   * \param urgent If true, make requests even neighbor asked for publishers recently
   */
  void find_publishers_for_tags(std::unordered_map<MachineID, internal::NeighborAgent>& neighbors,
				const std::vector<TagID>& tags,
				const std::vector<std::uint8_t>& publishers_needed,
				bool urgent = true) noexcept;

  /** \brief Sets if we must find more publishers for our tags upon request.
   *
   *  If yes, neighbors are directed to ignore their publisher
   *  caches and to seek more.
   */
  void set_must_find_more_publishers(bool must_find_more) noexcept
  {
    must_find_more_publishers_ = must_find_more;
  }

  /** \brief Get if we must find more publishers.
   */
  bool get_must_find_more_publishers() const noexcept
  {
      return must_find_more_publishers_;
  }

  private:
  // Attempts to get a message
  std::optional<MessageDeserializer>
  try_to_get_message(internal::NeighborAgent& nbr_agent,
                     SocketCommunicator& socket_comm) noexcept;

  // Handle status messages
  void handle_message(internal::NeighborAgent& nbr_agent,
                      MessageDeserializer& handle) noexcept;

  Manager* manager_;

  // This flag, if true, indicates that we have an immediate need to
  // force our neighbors to look harder for more publishers for a tag,
  // either because we need more publishers or because the currently
  // available ones aren't working for some reason.
  bool must_find_more_publishers_ = false;
}; // class MessageHandler

} // namespace internal

} // namespace skywing

#endif // SKYWING_MESSAGE_HANDLER_HPP

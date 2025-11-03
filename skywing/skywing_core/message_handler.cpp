#include "skywing_core/message_handler.hpp"
#include "skywing_core/neighbor_agent.hpp"
#include "skywing_core/manager.hpp"
#include "skywing_core/internal/utility/logging.hpp"
#include "skywing_core/internal/message_creators.hpp"

namespace skywing
{
namespace internal
{
  MessageHandler::MessageHandler(Manager& manager) noexcept
    : manager_{&manager}
{ }

void MessageHandler::get_and_handle_messages(internal::NeighborAgent& nbr_agent) noexcept
{
    if (nbr_agent.is_dead()) {
        return;
    }
    for (auto& socket_comm : nbr_agent.get_comms()) {
      while (auto handler = try_to_get_message(nbr_agent, socket_comm)) {
            // Update the last time something was heard
  	    nbr_agent.heard_from();
            // Handle the message
	    handle_message(nbr_agent, *handler);
        }
    }
}

  void MessageHandler::send_message(internal::NeighborAgent& nbr_agent,
				    const std::vector<std::byte>& msg) noexcept
{
    if (nbr_agent.is_dead()) {
        return;
    }
    // TODO: Maybe don't just use the first socket communicator if there are
    // multiple
    // TODO: Pretty sure this will incorrectly set to dead upon
    // ConnectionError::would_block
    if (nbr_agent.get_comms()[0].send_message(msg.data(), msg.size()) != ConnectionError::no_error)
    {
        nbr_agent.mark_as_dead();
    }
}

void MessageHandler::send_heartbeat_if_past_interval(internal::NeighborAgent& nbr_agent,
    std::chrono::milliseconds interval) noexcept
{
    using namespace std::chrono;
    const auto time_expired = steady_clock::now() - nbr_agent.time_last_heard();
    if (time_expired >= interval) {
      // Try to send a message
      send_message(nbr_agent, make_heartbeat());
      // This count as hearing from the device
      nbr_agent.heard_from();
    }
}

void MessageHandler::find_publishers_for_tags
  (std::unordered_map<MachineID, internal::NeighborAgent>& neighbors,
   const std::vector<TagID>& tags,
   const std::vector<std::uint8_t>& publishers_needed,
   bool urgent) noexcept
{
  for (auto& [nbr_id, nbr] : neighbors)
  {
    if (!urgent && (nbr.has_outstanding_publishers_request() ||
		    !nbr.is_time_for_another_request()))
      continue;

    SKYWING_TRACE_LOG("\"{}\" asking \"{}\" for tags {}{}",
                      manager_->id(),
                      nbr.id(),
                      tags,
                      nbr.has_outstanding_publishers_request()
                          ? ", but ignored due to already pending request"
                          : "");
    nbr.update_time_for_next_request(urgent);
    if (!nbr.has_outstanding_publishers_request()) {
      send_message
	(nbr, make_get_publishers(tags, publishers_needed,
				  must_find_more_publishers_));
      nbr.set_has_outstanding_publishers_request(true);
    }
  }
  // we have now asked for more publishers, so don't currently need to
  // ask for more
  set_must_find_more_publishers(false);
}


std::optional<MessageDeserializer>
MessageHandler::try_to_get_message(internal::NeighborAgent& nbr_agent,
				   SocketCommunicator& socket_comm) noexcept
{
    const auto bytes_to_read_or_error = read_network_size(socket_comm);
    if (std::holds_alternative<NetworkSizeType>(bytes_to_read_or_error)) {
        const auto bytes_to_read =
            *std::get_if<NetworkSizeType>(&bytes_to_read_or_error);
        //  Then read the actual message and parse it
        if (const auto message_buffer =
                read_chunked(socket_comm, bytes_to_read);
            !message_buffer.empty())
        {
            return MessageDeserializer::try_to_create(message_buffer);
        }
        else {
            // Couldn't read the size bytes - bad message
            SKYWING_TRACE_LOG("\"{}\" setting {} to dead due to bad message",
                              manager_->id(),
                              nbr_agent.id());
            nbr_agent.mark_as_dead();
            return {};
        }
    }
    else {
        const auto err = *std::get_if<ConnectionError>(&bytes_to_read_or_error);
        if (err == ConnectionError::closed) {
            SKYWING_TRACE_LOG(
                "\"{}\" setting {} to dead because connection has closed",
                manager_->id(),
                nbr_agent.id());
            nbr_agent.mark_as_dead();
        }
        else if (err != ConnectionError::would_block) {
            SKYWING_TRACE_LOG(
                "\"{}\" setting {} to dead because connection has some unknwon "
                "error, perhaps received an RST packer",
                manager_->id(),
                nbr_agent.id());
            nbr_agent.mark_as_dead();
        }
        // we get here if attempting to read_network_size returned
        // ConnectionError::would_block, which indicates there's the
        // connection is fine and there's just nothing currently on the
        // wire
        return {};
    }
}

// Handle status messages
void MessageHandler::handle_message(NeighborAgent& nbr_agent,
				    MessageDeserializer& handle) noexcept
{
    const auto okay = handle.do_callback(
        [&](const Greeting&) {
            // shouldn't be seeing a greeting here
            SKYWING_WARN_LOG(
                "\"{}\" received an unexpected greeting from \"{}\"",
                manager_->id(),
                nbr_agent.id());
            return false;
        },
        [&](const Goodbye&) {
            SKYWING_TRACE_LOG(
			      "\"{}\" received goodbye from \"{}\"",
			      manager_->id(), nbr_agent.id());
            nbr_agent.mark_as_dead();
            return true;
        },
        [&](const NewNeighbor& msg) {
            // Don't error if the neighbor is already present (as was previously
            // done) as if a machine disconnects and then re-connects it can
            // send a NewNeighbor message with a repeated ID
            SKYWING_TRACE_LOG(
                "\"{}\" received new neighbor from \"{}\" with id \"{}\"",
                manager_->id(),
                nbr_agent.id(),
                msg.neighbor_id());

	    nbr_agent.add_new_neighbor(msg.neighbor_id());
            return true;
        },
        [&](const RemoveNeighbor& msg) {
            SKYWING_TRACE_LOG(
                "\"{}\" received remove neighbor from \"{}\" with id \"{}\"",
                manager_->id(),
                nbr_agent.id(),
                msg.neighbor_id());

	    nbr_agent.remove_neighbor(msg.neighbor_id());
            return true;
        },
        [&, this](const Heartbeat&) {
            // If trace logging isn't enable then `this` isn't used, so make
            // sure it is marked as used
            (void) this;
            // Nothing to do; this is just to acknowledge it exists
            // (Last heard time was already updated in
            // get_and_handle_messages())
            SKYWING_TRACE_LOG("\"{}\" received heartbeat from \"{}\"",
			      manager_->id(), nbr_agent.id());
            return true;
        },
        [&](const ReportPublishers& msg) {
            SKYWING_TRACE_LOG("\"{}\" received report publishers from \"{}\" "
                              "with remote tags \"{}\" and local tags \"{}\"",
                              manager_->id(),
                              nbr_agent.id(),
                              msg.tags(),
                              msg.locally_produced_tags());

	    manager_->add_publishers_and_propagate(msg, nbr_agent);
            // Mark there as not being a request out there and update the time
            // to send out
            nbr_agent.set_has_outstanding_publishers_request(false);
	    nbr_agent.update_time_for_next_request(false);
            return true;
        },
        [&](const GetPublishers& msg) {
            SKYWING_TRACE_LOG(
                "\"{}\" received get publishers from \"{}\" requesting tags {}",
                manager_->id(),
                nbr_agent.id(),
                msg.tags());

	    manager_->handle_get_publishers(msg, nbr_agent);
            return true;
        },
        [&](const PublishData& msg) {
	    return manager_->handle_publish_data(msg, nbr_agent);
        },
        [&](const SubscriptionNotice& msg) {
            SKYWING_TRACE_LOG("\"{}\" received subscription notice from \"{}\" "
                              "for tags {}, is unsubscribe: {}",
                              manager_->id(),
                              nbr_agent.id(),
                              msg.tags(),
                              msg.is_unsubscribe());
            const auto reject_notice =
                [&]([[maybe_unused]] const std::string& why) {
                    SKYWING_TRACE_LOG(
                        "\"{}\" rejected subscription notice from \"{}\" as {}",
                        manager_->id(),
                        nbr_agent.id(),
                        why);
                };
            for (const auto& tag : msg.tags())
	    {
	        if (nbr_agent.is_subscribed_to(tag))
		{
		    reject_notice(fmt::format("repeated tag subscription to {}", tag));
		    return false;
		}
		else
		    nbr_agent.add_new_subscription(tag);
            }
	    if (!manager_->subscription_tags_are_produced(msg))
            {
                // TODO: Send a cancellation notice instead for the tags that
                // aren't there when this happens
                reject_notice(fmt::format(
                    "machine does not produce asked for tags {}", msg.tags()));
                return false;
            }
	    manager_->notify_subscriptions();
            SKYWING_TRACE_LOG("\"{}\" accepted subscription notice from \"{}\"",
                              manager_->id(),
                              nbr_agent.id());
            return true;
        },
        [](...) {
            // Anything else is a programming bug, this shouldn't be reached
            assert(
                false
                && "Missing message type in MessageHandler::handle_message");
            return false;
        });
    // Something incorrect happened
    if (!okay) {
        SKYWING_TRACE_LOG("\"{}\" setting {} to dead because something "
                          "incorrect happened upon message handle",
                          manager_->id(),
                          nbr_agent.id());
        nbr_agent.mark_as_dead();
    }
}

} // namespace internal
} // namespace skywing

#ifndef SKYNET_MASTER_HPP
#define SKYNET_MASTER_HPP

#include "message.hpp"
#include "devices/socket_communicator.hpp"
#include "job_base.hpp"
#include "utility/serialize.hpp"
#include "utility/on_error.hpp"
#include "job_base.hpp"

#include <vector>
#include <memory>
#include <array>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <thread>

// TODO: Support other types of communicators; will probably make
//       it a template and have it as a parameter, so not making a seperate
//       .cpp file even though there currently could be one.
// TODO: Support disconnecting of machines

namespace skynet
{
  /** \brief Tag to indicate that this connection was made by accepting a connection
   */
  struct ByAccept{};

  /** \brief Tag to indicate that this connection was made by requesting a connection
   */
  struct ByRequest{};

  /** \brief The handle used for external Skynet instances that are connected
   */
  class ExternalMaster
  {
  public:
    /** \brief Construct an ExternalMaster using an existing connection
     *
     * This is for when a server accepts a new connection.  Both have to
     * send/recieve greetings, but need to do so in the opposite order so
     * have seperate constructors for both.
     */
    ExternalMaster(ByAccept, SocketCommunicator conn)
      : conn_{std::move(conn)}
    {
      send_greeting();
      wait_for_greeting();
    }

    /** \brief Construct an ExternalMaster using an existing connection
     *
     * This is for when a client connects to a server.
     */
    ExternalMaster(ByRequest, SocketCommunicator conn)
      : conn_{std::move(conn)}
    {
      wait_for_greeting();
      send_greeting();
    }

    /** \brief Recieve a skynet::Message from an external connection if one exists
     *
     * Also returns the actually message that
     */
    Optional<MessageAndData> get_message() noexcept
    {
      std::array<char, Message::network_size> buffer;
      if (!conn_.read_message(buffer.data(), buffer.size()))
      {
        // No message yet
        return {};
      }
      MessageAndData to_ret{deserialize<Message>(buffer), id_};
      // Just return if there's nothing more to read
      if (to_ret.message().message_size == 0)
      {
        return to_ret;
      }

      if (!conn_.read_message(to_ret.data(), to_ret.message().message_size))
      {
        on_error("ExternalMaster::get_message failed to read the data part of the message!");
      }
      return to_ret;
    }

    /** \brief Sends a raw message to the other master
     */
    void send_message(const std::vector<char>& c)
    {
      conn_.send_message(c.data(), c.size());
    }

    /** \brief Returns the id of the computer this is connected to
     */
    std::uint32_t id() const noexcept { return id_; }

  private:
    // Wait until the greeting is sent
    void wait_for_greeting()
    {
      // Wait for the greeting
      // TODO: Probably want a time-out?
      while (true)
      {
        if (const auto opt_msg_data = get_message())
        {
          const auto& msg_data = *opt_msg_data;
          const auto& msg = msg_data.message();
          if (msg.type != MessageType::greeting)
          {
            on_error("ExternalMaster::ExternalMaster got non-greeting on connection!");
          }
          if (msg.message_size != 0)
          {
            on_error("ExternalMaster::ExternalMaster has a non-zero message size!");
          }
          // Otherwise just set the id
          id_ = msg_data.from();
          return;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(10));
      }
    }

    // Send the greeting
    void send_greeting()
    {
      Message to_send;
      to_send.type = MessageType::greeting;
      to_send.message_size = 0;
      send_message(serialize(to_send));
    }

    // For talking with the external master
    SocketCommunicator conn_;

    // The id of the external master
    std::uint32_t id_;
  }; // class ExternalMaster

  /** \brief The master Skynet instance used for communication
   */
  class Master
  {
  public:
    /** \brief Creates a Master instance that listens on the specified
     * port for connections.
     *
     * \param port The port to listen on
     * \param id The ID to assign to this machine
     */
    explicit Master(const std::uint16_t port, const std::uint32_t id)
      : id_{id}
    {
      server_socket_.set_to_listen(port);
    }

    /** \brief Connects to another instance at the specified address on
     * the specified port
     *
     * \param address The address to connect to
     * \param port The port to connect on
     */
    void connect_to_server(const char* const address, const std::uint16_t port)
    {
      SocketCommunicator to_connect;
      if (!to_connect.connect_to_server(address, port))
      {
        on_error("Master::connect_to_server failed!");
      }
      neighbors_.emplace_back(ByRequest{}, std::move(to_connect));
    }

    /** \brief Creates a job for the master
     *
     * \return A reference to the job
     */
    template<typename JobType>
    JobType& create_job()
    {
      jobs_.push_back(std::make_unique<JobType>());
      return jobs_.back();
    }

    /** \brief See if there are any pending connections and accept them if so
     */
    void make_pending_connections()
    {
      while(auto conn = server_socket_.accept())
      {
        neighbors_.emplace_back(ByAccept{}, std::move(*conn));
      }
    }

    /** \brief Listens for messages from neighbors and handles them if there
     * are any.
     */
    void handle_neighbor_messages()
    {
      for (auto&& neighbor : neighbors_)
      {
        if (auto msg = neighbor.get_message())
        {
          process_message(*msg);
        }
      }
    }

    /** \brief Broadcast a message to the entire network
     *
     * \param job_id The id of the job the message is for
     * \param tag_id The id of the tag the message is fo
     * \param msg_id The message's id
     * \param data The data to broadcast
     */
    void broadcast_message(
      const std::uint32_t job_id,
      const std::uint32_t tag_id,
      const std::uint32_t msg_id,
      const std::vector<char>& data
    ) noexcept
    {
      // Prepend the message describing the data and send it to all neighbors
      std::vector<char> to_send(Message::network_size + data.size());
      Message header;
      header.type = MessageType::broadcast;
      header.job_id = job_id;
      header.tag_id = tag_id;
      header.origin = id_;
      header.message_id = msg_id;
      header.message_size = data.size();
      const auto header_buffer = serialize(header);
      // copy the whole buffer over now
      std::memcpy(to_send.data(), header_buffer.data(), header_buffer.size());
      std::memcpy(to_send.data() + header_buffer.size(), data.data(), data.size());
      for (auto&& neighbor : neighbors_)
      {
        neighbor.send_message(to_send);
      }
    }

  private:
    // Does all processing that needs to be done when a message is recieved
    void process_message(const MessageAndData& message_and_data)
    {
      const auto& msg = message_and_data.message();
      // If the message is old just ignore it
      auto& last_id = last_message_id_[msg.origin];
      if (msg.message_id <= last_id)
      {
        return;
      }
      last_id = msg.message_id;
      switch(msg.type)
      {
      // Greeting messages should never been seen here, only when the
      // connection type is first made
      case MessageType::greeting:
        on_error("Unexpected greeting in Master::process_message");
        break;

      case MessageType::broadcast:
        // Just propagate the message to all neighbors but the one it was
        // recieved from
        for (auto&& neighbor : neighbors_)
        {
          if (neighbor.id() != message_and_data.from())
          {
            neighbor.send_message(message_and_data.vector());
          }
        }
        break;
      }
    }

    // For listening to connection requests
    SocketCommunicator server_socket_;

    // List of the jobs that are present
    std::vector<std::unique_ptr<JobBase>> jobs_;

    // List of neighboring connections
    std::vector<ExternalMaster> neighbors_;

    // The message id of each last heard message from each machine in the network
    std::unordered_map<std::uint32_t, std::uint32_t> last_message_id_;

    // The id of this machine
    std::uint32_t id_;
  }; // class Master
} // namespace skynet

#endif // SKYNET_MASTER_HPP

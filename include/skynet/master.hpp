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
    ExternalMaster(ByAccept, SocketCommunicator conn, const std::uint32_t local_id)
      : conn_{std::move(conn)}
    {
      send_greeting(local_id);
      wait_for_greeting();
    }

    /** \brief Construct an ExternalMaster using an existing connection
     *
     * This is for when a client connects to a server.
     */
    ExternalMaster(ByRequest, SocketCommunicator conn, const std::uint32_t local_id)
      : conn_{std::move(conn)}
    {
      wait_for_greeting();
      send_greeting(local_id);
    }

    /** \brief Recieve a skynet::Message from an external connection if one exists
     *
     * Returns the message as well as the buffer containing the serialized message
     * and data
     */
    Optional<std::pair<Message, MessageAndDataBuffer>> get_message() noexcept
    {
      MessageAndDataBuffer buffer{id_};
      if (!conn_.read_message(buffer.buffer(), Message::network_size))
      {
        // No message yet
        return {};
      }
      // Get the message and adjust the buffer
      const Message msg = buffer.message();
      // Read the rest (if there's anything)
      if (msg.message_size != 0)
      {
        if (!conn_.read_message(buffer.data(), msg.message_size))
        {
          on_error("ExternalMaster::get_message failed to read the data part of the message!");
        }
      }
      return std::make_pair(msg, std::move(buffer));
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
          const auto& msg = msg_data.first;
          if (msg.type != MessageType::greeting)
          {
            on_error("ExternalMaster::ExternalMaster got non-greeting on connection!");
          }
          if (msg.message_size != 0)
          {
            on_error("ExternalMaster::ExternalMaster has a non-zero message size!");
          }
          // Otherwise just set the id
          id_ = msg.origin;
          return;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(10));
      }
    }

    // Send the greeting
    void send_greeting(const std::uint32_t local_id)
    {
      Message to_send;
      to_send.type = MessageType::greeting;
      to_send.message_size = 0;
      to_send.origin = local_id;
      send_message(to_bytes(to_send));
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
      neighbors_.emplace_back(ByRequest{}, std::move(to_connect), id_);
    }

    /** \brief See if there are any pending connections and accept them if so
     */
    void accept_pending_connections()
    {
      while(auto conn = server_socket_.accept())
      {
        neighbors_.emplace_back(ByAccept{}, std::move(*conn), id_);
      }
    }

    /** \brief Creates a job for the master
     *
     * \return A reference to the job
     */
    template<typename JobType>
    JobType& create_job(const std::uint32_t id)
    {
      jobs_.push_back(std::make_unique<JobType>(id, *this));
      return static_cast<JobType&>(*jobs_.back());
    }

    /** \brief Listens for messages from neighbors and handles them if there
     * are any.
     */
    void handle_neighbor_messages()
    {
      for (auto&& neighbor : neighbors_)
      {
        if (auto msg_opt = neighbor.get_message())
        {
          const auto& msg = *msg_opt;
          process_message(msg.first, msg.second);
        }
      }
    }

    /** \brief Broadcast a message to the entire network
     *
     * \param job_id The id of the job the message is for
     * \param tag_id The id of the tag the message is fo
     * \param msg_id The message's id
     * \param data The data to broadcast (no serialization is done)
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
      const auto header_buffer = to_bytes(header);
      // copy the whole buffer over now
      std::memcpy(to_send.data(), header_buffer.data(), header_buffer.size());
      std::memcpy(to_send.data() + header_buffer.size(), data.data(), data.size());
      for (auto&& neighbor : neighbors_)
      {
        neighbor.send_message(to_send);
      }
    }

    // TODO: Is this something useful or just good for testing?
    /** \brief Returns the number of machines connected
     */
    int number_of_neighbors() const noexcept
    {
      return static_cast<int>(neighbors_.size());
    }

  private:
    // Does all processing that needs to be done when a message is recieved
    void process_message(const Message& msg, const MessageAndDataBuffer& buffer)
    {
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
        // Add the data to the appropriate queue
        add_data_to_queue(msg, buffer);
        // Propagate the message to all neighbors but the one it was
        // recieved from and the origin
        for (auto&& neighbor : neighbors_)
        {
          if (neighbor.id() != buffer.from() && neighbor.id() != msg.origin)
          {
            neighbor.send_message(buffer.vector());
          }
        }
        break;
      }
    }

    // Adds data to the tag queue for a job from a message
    void add_data_to_queue(const Message& msg, const MessageAndDataBuffer& buffer)
    {
      if (msg.job_id >= jobs_.size())
      {
        on_error("Job ID larger than number of jobs!");
      }
      jobs_[msg.job_id]->process_data(msg.tag_id, buffer.data(), msg.message_size);
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

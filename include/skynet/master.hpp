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
  namespace detail
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
      /** \brief Attempt to construct an ExternalMaster using an existing connection
       *
       * This is for when a server accepts a new connection.  Both have to
       * send/recieve greetings, but need to do so in the opposite order so
       * have seperate constructors for both.
       */
      static Optional<ExternalMaster> create(
        ByAccept,
        SocketCommunicator conn,
        const std::uint32_t local_id
      ) noexcept
      {
        ExternalMaster to_ret(std::move(conn));
        if (!to_ret.send_greeting(local_id)) { return {}; }
        if (!to_ret.wait_for_greeting()) { return {}; }
        return to_ret;
      }

      /** \brief Attempt to construct an ExternalMaster using an existing connection
       *
       * This is for when a client connects to a server.
       */
      static Optional<ExternalMaster> create(
        ByRequest,
        SocketCommunicator conn,
        const std::uint32_t local_id
      ) noexcept
      {
        ExternalMaster to_ret(std::move(conn));
        if (!to_ret.wait_for_greeting()) { return {}; }
        if (!to_ret.send_greeting(local_id)) { return {}; }
        return to_ret;
      }

      /** \brief Recieve a skynet::Message from an external connection if one exists
       *
       * Returns the message as well as the buffer containing the serialized message
       * and data.  Also marks the connection as dead if any errors occur.  Does
       * nothing if the connection is marked as dead.
       */
      Optional<std::pair<Message, MessageAndDataBuffer>> get_message() noexcept
      {
        if (dead_)
        {
          return {};
        }
        MessageAndDataBuffer buffer{id_};
        const auto err = conn_.read_message(buffer.buffer(), Message::network_size);
        switch (err)
        {
        case ConnectionError::no_error: break;
        case ConnectionError::would_block: return {};

        case ConnectionError::closed:
          // [[fallthrough]];
        case ConnectionError::unrecoverable:
          dead_ = true;
          return {};
        }
        // Get the message and adjust the buffer
        const Message msg = buffer.message();
        // Read the rest (if there's anything)
        if (msg.message_size != 0)
        {
          if (conn_.read_message(buffer.data(), msg.message_size) != ConnectionError::no_error)
          {
            on_error("ExternalMaster::get_message failed to read the data part of the message!");
          }
        }
        // If it's a goodbye message mark this connection as dead
        if (msg.type == MessageType::goodbye)
        {
          dead_ = true;
        }
        return std::make_pair(msg, std::move(buffer));
      }

      /** \brief Sends a raw message to the other master
       *
       * Also marks the connection as dead if any errors occur.  Does nothing
       * if the connection is marked as dead.
       */
      void send_message(const std::vector<char>& c)
      {
        if (dead_)
        {
          return;
        }
        if (conn_.send_message(c.data(), c.size()) != ConnectionError::no_error)
        {
          dead_ = true;
        }
      }

      /** \brief Returns the id of the computer this is connected to
       */
      std::uint32_t id() const noexcept { return id_; }

      /** \brief Returns if the connection is dead or not
       */
      bool is_dead() const noexcept { return dead_; }

    private:
      // Only allow private construction
      explicit ExternalMaster(SocketCommunicator conn)
        : conn_{std::move(conn)}
      {}

      // Wait until the greeting is sent
      bool wait_for_greeting()
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
            return true;
          }
          // If the connection died then don't keep trying to connect
          if (dead_)
          {
            return false;
          }
          std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
      }

      // Send the greeting
      bool send_greeting(const std::uint32_t local_id)
      {
        Message to_send;
        to_send.type = MessageType::greeting;
        to_send.message_size = 0;
        to_send.origin = local_id;
        send_message(to_bytes(to_send));
        return !dead_;
      }

      // For talking with the external master
      SocketCommunicator conn_;

      // The id of the external master
      std::uint32_t id_;

      // If the connection is dead or not
      bool dead_{false};
    }; // class ExternalMaster
  } // namespace detail

  /** \brief The master Skynet instance used for communication
   */
  class Master
  {
  public:
    // Allow Job classes to broadcast and handle neighbors but nothing else
    struct Accessor
    {
    private:
      template<typename...>
      friend class Job;

      static void handle_neighbor_message(Master& m)
      {
        m.handle_neighbor_messages();
      }

      static void local_broadcast(
        Master& m,
        const std::uint32_t job_id,
        const std::uint32_t tag_id,
        const std::uint32_t msg_id,
        const std::vector<char>& data
      )
      {
        m.do_broadcast(job_id, tag_id, msg_id, data, MessageType::local_broadcast);
      }

      static void global_broadcast(
        Master& m,
        const std::uint32_t job_id,
        const std::uint32_t tag_id,
        const std::uint32_t msg_id,
        const std::vector<char>& data
      )
      {
        m.do_broadcast(job_id, tag_id, msg_id, data, MessageType::global_broadcast);
      }
    }; // struct Accessor

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

    /** \brief Destructor; tells all neighbors that the device is dead
     */
    ~Master()
    {
      Message to_send;
      to_send.type = MessageType::goodbye;
      to_send.message_size = 0;
      to_send.origin = id_;
      // ensure that the message isn't ignored
      to_send.message_id = UINT32_MAX;
      // Job ID doesn't matter, but needs to be set, so just set it to 0
      to_send.job_id = 0;
      const auto buffer = to_bytes(to_send);
      for (auto& neighbor : neighbors_)
      {
        neighbor.send_message(buffer);
      }
    }

    // Can't copy
    Master(const Master&) = delete;
    Master& operator=(const Master&) = delete;
    // Movable
    Master(Master&&) = default;
    Master& operator=(Master&&) = default;

    /** \brief Connects to another instance at the specified address on
     * the specified port
     *
     * \param address The address to connect to
     * \param port The port to connect on
     */
    void connect_to_server(const char* const address, const std::uint16_t port)
    {
      SocketCommunicator to_connect;
      if (to_connect.connect_to_server(address, port) != ConnectionError::no_error)
      {
        on_error("Master::connect_to_server failed!");
      }
      if (auto new_neighbor = detail::ExternalMaster::create(detail::ByRequest{}, std::move(to_connect), id_))
      {
        neighbors_.emplace_back(std::move(*new_neighbor));
      }
    }

    /** \brief See if there are any pending connections and accept them if so
     */
    void accept_pending_connections()
    {
      while(auto conn = server_socket_.accept())
      {
        if (auto new_neighbor = detail::ExternalMaster::create(detail::ByAccept{}, std::move(*conn), id_))
        {
          neighbors_.emplace_back(std::move(*new_neighbor));
        }
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

    // TODO: Is this something useful or just good for testing?
    /** \brief Returns the number of machines connected
     */
    int number_of_neighbors() const noexcept
    {
      return static_cast<int>(neighbors_.size());
    }

  private:
    /** \brief Listens for messages from neighbors and handles them if there
     * are any.
     */
    void handle_neighbor_messages()
    {
      remove_dead_neighbors();
      for (auto&& neighbor : neighbors_)
      {
        while (auto msg_opt = neighbor.get_message())
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
     * \param type The type of broadcast
     */
    void do_broadcast(
      const std::uint32_t job_id,
      const std::uint32_t tag_id,
      const std::uint32_t msg_id,
      const std::vector<char>& data,
      const MessageType type
    ) noexcept
    {
      // Prepend the message describing the data and send it to all neighbors
      std::vector<char> to_send(Message::network_size + data.size());
      Message header;
      header.type = type;
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

    // Does all processing that needs to be done when a message is recieved
    void process_message(const Message& msg, const MessageAndDataBuffer& buffer)
    {
      // TODO: Probably a systematic way to say that a message isn't related to
      //       a job since only those should be broadcast?
      // If the message is old just ignore it, unless it's a parting
      // message
      if (msg.type != MessageType::goodbye) {
        auto& last_id = last_message_id_[msg.origin][msg.job_id];
        if (msg.message_id <= last_id)
        {
          return;
        }
        last_id = msg.message_id;
      }

      switch(msg.type)
      {
      // Greeting messages should never been seen here, only when the
      // connection type is first made
      case MessageType::greeting:
        on_error("Unexpected greeting in Master::process_message");
        break;

      case MessageType::goodbye:
        // Just exit for now; can't remove the dead neighbors as this is called
        // during iteration
        break;

      case MessageType::local_broadcast:
        add_data_to_queue(msg, buffer);
        break;

      case MessageType::global_broadcast:
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
      detail::JobBase::Accessor::process_data(
        *jobs_[msg.job_id],
        msg.tag_id,
        buffer.data(),
        msg.message_size
      );
    }

    /** \brief Removes all dead neighbors
     */
    void remove_dead_neighbors() noexcept
    {
      // TODO: Maybe want to free some of last_message_id_ since it's not required
      //       anymore, but there's always the chance that an old broadcast message
      //       comes along, so a list of dead ID's would be needed
      neighbors_.erase(
        std::remove_if(
          neighbors_.begin(),
          neighbors_.end(),
          [](const detail::ExternalMaster& m) { return m.is_dead(); }
        ),
        neighbors_.end()
      );
    }

    // For listening to connection requests
    SocketCommunicator server_socket_;

    // List of the jobs that are present
    std::vector<std::unique_ptr<detail::JobBase>> jobs_;

    // List of neighboring connections
    std::vector<detail::ExternalMaster> neighbors_;

    // The message id of each last heard message from each machine for each job in the network
    std::unordered_map<
      std::uint32_t,
      std::unordered_map<std::uint32_t, std::uint32_t>
    > last_message_id_;

    // The id of this machine
    std::uint32_t id_;
  }; // class Master
} // namespace skynet

#endif // SKYNET_MASTER_HPP

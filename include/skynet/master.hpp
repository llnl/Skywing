#ifndef SKYNET_MASTER_HPP
#define SKYNET_MASTER_HPP

#include "skynet/types.hpp"
#include "skynet/detail/job_base.hpp"
#include "skynet/detail/message.hpp"
#include "skynet/detail/devices/socket_communicator.hpp"
#include "skynet/detail/utility/on_error.hpp"

#include <vector>
#include <memory>
#include <array>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <thread>
#include <cassert>
#include <chrono>

// TODO: Support other types of communicators; will probably make
//       it a template and have it as a parameter, so not making a seperate
//       .cpp file even though there currently could be one.

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
        const MachineID local_id,
        const std::vector<MachineID>& local_neighbors
      ) noexcept
      {
        ExternalMaster to_ret(std::move(conn));
        return init_conn(
          to_ret,
          [&]() { return to_ret.send_greeting(local_id, local_neighbors); },
          [&]() { return to_ret.wait_for_greeting(); }
        );
      }

      /** \brief Attempt to construct an ExternalMaster using an existing connection
       *
       * This is for when a client connects to a server.
       */
      static Optional<ExternalMaster> create(
        ByRequest,
        SocketCommunicator conn,
        const MachineID local_id,
        const std::vector<MachineID>& local_neighbors
      ) noexcept
      {
        ExternalMaster to_ret(std::move(conn));
        return init_conn(
          to_ret,
          [&]() { return to_ret.wait_for_greeting(); },
          [&]() { return to_ret.send_greeting(local_id, local_neighbors); }
        );
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
            // on_error("ExternalMaster::get_message failed to read the data part of the message!");
            dead_ = true;
            return {};
          }
        }
        // Handle all status messages here
        if (is_status_message(msg.type))
        {
          handle_message(msg, buffer);
          return {};
        }
        return std::make_pair(msg, std::move(buffer));
      }

      /** \brief Sends a raw message to the other master
       *
       * Also marks the connection as dead if any errors occur.  Does nothing
       * if the connection is marked as dead.
       */
      void send_message(const std::vector<char>& c) noexcept
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
      MachineID id() const noexcept { return id_; }

      /** \brief Returns if the connection is dead or not
       */
      bool is_dead() const noexcept { return dead_; }

      /** \brief Marks the connection as dead
       */
      void mark_as_dead() noexcept { dead_ = true; }

    private:
      // Only allow private construction
      explicit ExternalMaster(SocketCommunicator conn) noexcept
        : conn_{std::move(conn)}
      {}

      // Function that handles the joining/accepting connection
      template<typename First, typename Second>
      static Optional<ExternalMaster> init_conn(ExternalMaster& m, First first, Second second) noexcept
      {
        using namespace std::chrono;
        const auto start = steady_clock::now();
        if (!first()) { return {}; }
        const auto mid = steady_clock::now();
        if (!second()) { return {}; }
        const auto end = steady_clock::now();
        const auto time1 = duration_cast<decltype(latency_)>(mid - start);
        const auto time2 = duration_cast<decltype(latency_)>(end - mid);
        m.latency_ = decltype(latency_){(time1.count() + time2.count()) / 2};
        return std::move(m);
      }

      // Wait until the greeting is sent
      bool wait_for_greeting() noexcept
      {
        // Wait for the greeting
        // TODO: Probably want a time-out?
        while (true)
        {
          if (const auto msg_data = get_message())
          {
            const auto& msg = msg_data->first;
            if (msg.type != MessageType::greeting)
            {
              // on_error("ExternalMaster::ExternalMaster got non-greeting on connection!");
              return false;
            }
            // Otherwise just set the id and neighbors
            id_ = msg.origin;
            neighbors_ = from_bytes<std::vector<MachineID>>(msg_data->second.data(), msg.message_size);
            return true;
          }
          // If the connection died then don't keep trying to connect
          if (dead_)
          {
            return false;
          }
          std::this_thread::sleep_for(std::chrono::microseconds{10});
        }
      }

      // Handle status messages
      void handle_message(const Message& msg, const MessageAndDataBuffer& /* buffer */) noexcept
      {
        assert(is_status_message(msg.type));
        switch(msg.type)
        {
        case MessageType::goodbye:
          dead_ = true;
          break;

        case MessageType::removed_neighbor:
          {
            // TODO: Fix this terrible hack
            const auto loc = std::find(neighbors_.begin(), neighbors_.end(), msg.message_id);
            // Neighbor is lying; can't trust it anymore
            if (loc == neighbors_.end())
            {
              dead_ = true;
              return;
            }
            // otherwise just remove it
            using std::swap;
            swap(*loc, neighbors_.back());
            neighbors_.pop_back();
          }
          break;

        case MessageType::new_neighbor:
          // TODO: Fix this terrible hack
          neighbors_.push_back(msg.message_id);
          break;

        default:
          break;
        }
      }

      // Send the greeting
      bool send_greeting(const MachineID local_id, const std::vector<MachineID>& local_neighbors) noexcept
      {
        const auto neighbor_data = to_bytes(local_neighbors);
        Message to_send;
        to_send.type = MessageType::greeting;
        to_send.message_size = neighbor_data.size();
        to_send.origin = local_id;
        send_message(prepare(to_send, local_neighbors));
        return !dead_;
      }

      // For talking with the external master
      SocketCommunicator conn_;

      // The id of the external master
      MachineID id_;

      // Estimated latency to the other machine
      std::chrono::microseconds latency_;

      // The neighbors that the external machine has
      std::vector<MachineID> neighbors_;

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

      template<typename T>
      static void local_broadcast(
        Master& m,
        const JobID job_id,
        const TagID tag_id,
        const MessageID msg_id,
        const T& data
      )
      {
        m.do_broadcast(job_id, tag_id, msg_id, data, detail::MessageType::local_broadcast);
      }

      template<typename T>
      static void global_broadcast(
        Master& m,
        const JobID job_id,
        const TagID tag_id,
        const MessageID msg_id,
        const T& data
      )
      {
        m.do_broadcast(job_id, tag_id, msg_id, data, detail::MessageType::global_broadcast);
      }
    }; // struct Accessor

    /** \brief Creates a Master instance that listens on the specified
     * port for connections.
     *
     * \param port The port to listen on
     * \param id The ID to assign to this machine
     */
    explicit Master(const std::uint16_t port, const MachineID id)
      : id_{id}
    {
      server_socket_.set_to_listen(port);
    }

    /** \brief Destructor; tells all neighbors that the device is dead
     */
    ~Master()
    {
      detail::Message to_send;
      to_send.type = detail::MessageType::goodbye;
      to_send.message_size = 0;
      to_send.origin = id_;
      const auto buffer = to_bytes(to_send);
      for (auto& neighbor : neighbors_)
      {
        neighbor.second.send_message(buffer);
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
      detail::SocketCommunicator to_connect;
      if (to_connect.connect_to_server(address, port) != detail::ConnectionError::no_error)
      {
        detail::on_error("Master::connect_to_server failed!");
      }
      if (auto new_neighbor = detail::ExternalMaster::create(detail::ByRequest{}, std::move(to_connect), id_, make_neighbor_vector()))
      {
        const auto new_id = new_neighbor->id();
        notify_of_new_neighbor(new_id);
        neighbors_.emplace(new_id, std::move(*new_neighbor));
      }
    }

    /** \brief See if there are any pending connections and accept them if so
     */
    void accept_pending_connections()
    {
      while(auto conn = server_socket_.accept())
      {
        if (auto new_neighbor = detail::ExternalMaster::create(detail::ByAccept{}, std::move(*conn), id_, make_neighbor_vector()))
        {
          const auto new_id = new_neighbor->id();
          notify_of_new_neighbor(new_id);
          neighbors_.emplace(new_id, std::move(*new_neighbor));
        }
      }
    }

    /** \brief Creates a job for the master
     *
     * \return A reference to the job
     */
    template<typename JobType>
    JobType& make_job(const JobID id)
    {
      if (jobs_.find(id) != jobs_.end())
      {
        detail::on_error("Job with duplicate ID created!");
      }
      // std::pair<iterator, bool>
      const auto res = jobs_.emplace(id, std::make_unique<JobType>(id, *this));
      return static_cast<JobType&>(*res.first->second);
    }

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
      for (auto&& neighbor : neighbors_)
      {
        while (auto msg_opt = neighbor.second.get_message())
        {
          const auto& msg = *msg_opt;
          process_message(msg.first, msg.second);
        }
      }
      remove_dead_neighbors();
    }

    /** \brief Broadcast a message to the entire network
     *
     * \param job_id The id of the job the message is for
     * \param tag_id The id of the tag the message is fo
     * \param msg_id The message's id
     * \param data The data to broadcast
     * \param type The type of broadcast
     */
    template<typename T>
    void do_broadcast(
      const JobID job_id,
      const TagID tag_id,
      const MessageID msg_id,
      const T& data,
      const detail::MessageType type
    ) noexcept
    {
      // Prepend the message describing the data and send it to all neighbors
      detail::Message header;
      header.type = type;
      header.job_id = job_id;
      header.tag_id = tag_id;
      header.origin = id_;
      header.message_id = msg_id;
      send_to_neighbors(detail::prepare(header, data));
    }

    // Does all processing that needs to be done when a message is recieved
    void process_message(const detail::Message& msg, const detail::MessageAndDataBuffer& buffer)
    {
      assert(!detail::is_status_message(msg.type));
      // If the message is bad ignore it and mark the connection as dead
      if (message_is_bad(msg, buffer))
      {
        mark_as_dead(buffer);
        return;
      }
      if (detail::is_job_message(msg.type)) {
        // If the message is old just ignore it
        auto& last_id = last_message_id_[msg.origin][msg.job_id];
        if (msg.message_id <= last_id)
        {
          return;
        }
        last_id = msg.message_id;
      }

      switch(msg.type)
      {
      case detail::MessageType::greeting:
        // Greeting messages should never been seen here, only when the
        // connection type is first made; this connection is malfunctioning
        // so consider it dead
        // detail::on_error("Unexpected greeting in Master::process_message");
        // break;
        mark_as_dead(buffer);
        break;

      case detail::MessageType::local_broadcast:
        add_data_to_queue(msg, buffer);
        break;

      case detail::MessageType::global_broadcast:
        // Add the data to the appropriate queue
        add_data_to_queue(msg, buffer);
        // Propagate the message to all neighbors but the one it was
        // recieved from and the origin
        send_to_neighbors_if(buffer.vector(), [&](const detail::ExternalMaster& m) {
          return m.id() != buffer.from() && m.id() != msg.origin;
        });
        break;

      default:
        break;
      }
    }

    // Adds data to the tag queue for a job from a message
    void add_data_to_queue(const detail::Message& msg, const detail::MessageAndDataBuffer& buffer)
    {
      assert(jobs_.find(msg.job_id) != jobs_.end());
      const auto okay = detail::JobBase::Accessor::process_data(
        *jobs_.find(msg.job_id)->second,
        msg.tag_id,
        buffer.data(),
        msg.message_size
      );
      if (!okay)
      {
        mark_as_dead(buffer);
      }
    }

    /** \brief Notify neighbors of a new new neighbor
     */
    void notify_of_new_neighbor(const MachineID id)
    {
      detail::Message msg;
      msg.origin = id_;
      msg.message_size = 0;
      // TODO: Fix this hack
      msg.message_id = id;
      msg.type = detail::MessageType::new_neighbor;
      send_to_neighbors(to_bytes(msg));
    }

    /** \brief Removes all dead neighbors
     */
    void remove_dead_neighbors() noexcept
    {
      for (auto it = neighbors_.begin(); it != neighbors_.end(); /* nothing */)
      {
        if (it->second.is_dead())
        {
          // notify neighbors that a neighbor has been lost
          detail::Message msg;
          msg.origin = id_;
          msg.message_size = 0;
          // TODO: This is a dirty hack to re-use memory; come up
          //       with a better solution later
          msg.message_id = it->first;
          msg.type = detail::MessageType::removed_neighbor;
          send_to_neighbors(to_bytes(msg));
          it = neighbors_.erase(it);
        }
        else
        {
          ++it;
        }
      }
    }

    /** \brief Checks if a message is bad or not
     */
    bool message_is_bad(const detail::Message& msg, const detail::MessageAndDataBuffer& buffer) const
    {
      // If the message is from a machine that isn't known
      if (neighbors_.find(buffer.from()) == neighbors_.end())
      {
        return true;
      }
      if (!detail::is_system_message(msg.type))
      {
        // If the job id isn't found
        if (jobs_.find(msg.job_id) == jobs_.end())
        {
          return true;
        }
      }
      return false;
    }

    /** \brief Marks the message a buffer was recieved from as dead
     */
    void mark_as_dead(const detail::MessageAndDataBuffer& buffer) noexcept
    {
      assert(neighbors_.find(buffer.from()) != neighbors_.end());
      neighbors_.find(buffer.from())->second.mark_as_dead();
    }

    /** \brief Returns a vector of all the neighboring ID's
     */
    std::vector<MachineID> make_neighbor_vector() const
    {
      std::vector<MachineID> to_ret(neighbors_.size());
      for (const auto& neighbor : neighbors_)
      {
        to_ret.push_back(neighbor.first);
      }
      return to_ret;
    }

    /** \biref Broadcasts a message to all neighbors that fit a criteria
     */
    template<typename Callable>
    void send_to_neighbors_if(const std::vector<char>& to_send, Callable c)
    {
      for (auto&& neighbor : neighbors_)
      {
        if (c(neighbor.second))
        {
          neighbor.second.send_message(to_send);
        }
      }
    }

    /** \brief Broadcasts a message to all neighbors
     */
    void send_to_neighbors(const std::vector<char>& to_send)
    {
      send_to_neighbors_if(to_send, [](const detail::ExternalMaster&) { return true; });
    }

    // For listening to connection requests
    detail::SocketCommunicator server_socket_;

    // List of the jobs that are present
    std::unordered_map<JobID, std::unique_ptr<detail::JobBase>> jobs_;

    // List of neighboring connections
    std::unordered_map<MachineID, detail::ExternalMaster> neighbors_;

    // The message id of each last heard message from each machine for each job in the network
    std::unordered_map<
      MachineID,
      std::unordered_map<JobID, MessageID>
    > last_message_id_;

    // The id of this machine
    std::uint32_t id_;
  }; // class Master
} // namespace skynet

#endif // SKYNET_MASTER_HPP

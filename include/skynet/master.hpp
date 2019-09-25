#ifndef SKYNET_MASTER_HPP
#define SKYNET_MASTER_HPP

#include "skynet/types.hpp"
#include "skynet/internal/job_base.hpp"
#include "skynet/internal/message.hpp"
#include "skynet/internal/devices/socket_communicator.hpp"
#include "skynet/internal/utility/on_error.hpp"
#include "skynet/internal/utility/serialize.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <thread>
#include <unordered_map>
#include <vector>

// TODO: Support other types of communicators; will probably make
//       it a template and have it as a parameter, so not making a seperate
//       .cpp file even though there currently could be one.

namespace skynet
{
  namespace internal
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
      static std::optional<ExternalMaster> create(
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
      static std::optional<ExternalMaster> create(
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
       * Returns the handler for the message if one exists.
       */
      std::optional<MessageHandler> get_message() noexcept
      {
        if (dead_)
        {
          return {};
        }
        if (auto handler = MessageHandler::try_to_get(*this))
        {
          // handle status messages here
          if (handler->category() == MessageCategory::status)
          {
            handle_message(*handler);
            // There could be non-status messages so check those as well
            return get_message();
          }
          return std::move(*handler);
        }
        return {};
      }

      /** \brief Sends a raw message to the other master
       *
       * Also marks the connection as dead if any errors occur.  Does nothing
       * if the connection is marked as dead.
       */
      void send_message(const std::vector<std::byte>& c) noexcept
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

      /** \brief Returns true if the given neighbor is present, false otherwise
       */
      bool has_neighbor(const MachineID id) const noexcept
      {
        const auto loc = std::lower_bound(neighbors_.cbegin(), neighbors_.cend(), id);
        return loc != neighbors_.cend() && *loc == id;
      }

      // Allow the MessageHandler to read bytes
      class Accessor
      {
        friend class MessageHandler;

        // Reads the specified data into the buffer, returns true if it succeeded,
        // false if it either couldn't read or it would've blocked
        static bool read(ExternalMaster& m, std::byte* const buffer, const std::size_t count)
        {
          const auto err = m.conn_.read_message(buffer, count);
          switch (err)
          {
          case ConnectionError::no_error: break;
          case ConnectionError::would_block: return false;

          case ConnectionError::closed:
            // [[fallthrough]];
          case ConnectionError::unrecoverable:
            m.dead_ = true;
            return false;
          }
          return true;
        }
      };

    private:
      // Only allow private construction
      explicit ExternalMaster(SocketCommunicator conn) noexcept
        : conn_{std::move(conn)}
      {}

      // Function that handles the joining/accepting connection
      template<typename First, typename Second>
      static std::optional<ExternalMaster> init_conn(ExternalMaster& m, First first, Second second) noexcept
      {
        using namespace std::chrono;
        const auto start = steady_clock::now();
        if (!first()) { return {}; }
        const auto mid = steady_clock::now();
        if (!second()) { return {}; }
        const auto end = steady_clock::now();
        const auto dur1 = duration_cast<decltype(latency_)>(mid - start);
        const auto dur2 = duration_cast<decltype(latency_)>(end - mid);
        m.latency_ = decltype(latency_){(dur1.count() + dur2.count()) / 2};
        return std::move(m);
      }

      // Send the greeting
      bool send_greeting(const MachineID local_id, const std::vector<MachineID>& local_neighbors) noexcept
      {
        send_message(make_greeting(local_id, local_neighbors));
        return !dead_;
      }

      // Wait until the greeting is sent
      bool wait_for_greeting() noexcept
      {
        // Wait for the greeting
        // TODO: Probably want a time-out?
        while (true)
        {
          if (const auto handle = MessageHandler::try_to_get(*this))
          {
            if (handle->category() != MessageCategory::status)
            {
              return false;
            }
            return handle->do_callback(
              *this,
              [&](Greeting msg) {
                neighbors_ = std::move(msg.neighbors);
                id_ = msg.from;
                return true;
              },
              // Any other kind of message is an error
              [&](...) { return false; }
            );
          }
          std::this_thread::sleep_for(std::chrono::microseconds{10});
        }
      }

      // Handle status messages
      void handle_message(const MessageHandler& handle) noexcept
      {
        assert(handle.category() == MessageCategory::status);
        const auto okay = handle.do_callback(
          *this,
          [&](const Greeting&) {
            // shouldn't be seeing a greeting here
            return false;
          },
          [&](const Goodbye&) {
            dead_ = true;
            return true;
          },
          [&](const NewNeighbor& msg) {
            const auto loc = std::lower_bound(neighbors_.cbegin(), neighbors_.cend(), msg.neighbor_id);
            // Already present -> connection is bad
            if (loc != neighbors_.cend() && *loc == msg.neighbor_id)
            {
              return false;
            }
            // Otherwise just insert it
            neighbors_.insert(loc, msg.neighbor_id);
            return true;
          },
          [&](const RemoveNeighbor& msg) {
            const auto loc = std::lower_bound(neighbors_.begin(), neighbors_.end(), msg.neighbor_id);
            // Neighbor is lying; can't trust it anymore
            if (loc == neighbors_.end() || *loc != msg.neighbor_id)
            {
              return false;
            }
            // otherwise just remove it
            using std::swap;
            swap(*loc, neighbors_.back());
            neighbors_.pop_back();
            return true;
          },
          [](...) {
            // Anything else is a programming bug
            assert(false && "Invalid message type in ExternalMaster::handle_message");
            return false;
          }
        );
        // Something incorrect happened
        if (!okay)
        {
          dead_ = true;
        }
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
  } // namespace internal

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
      static void broadcast(
        Master& m,
        const MessageID msg_id,
        const JobID job_id,
        const TagID tag_id,
        const TagIndex tag_index,
        const std::uint32_t hops_left_p1,
        const T& data
      )
      {
        m.do_broadcast(msg_id, job_id, tag_id, tag_index, hops_left_p1, internal::Serializer{}.add(data).bytes());
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
      send_to_neighbors(internal::make_goodbye());
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
     * \return True if the connection was successful, false if it failed
     */
    bool connect_to_server(const char* const address, const std::uint16_t port)
    {
      internal::SocketCommunicator to_connect;
      if (to_connect.connect_to_server(address, port) != internal::ConnectionError::no_error)
      {
        // internal::on_error("Master::connect_to_server failed!");
        return false;
      }
      if (auto new_neighbor = internal::ExternalMaster::create(
        internal::ByRequest{},
        std::move(to_connect),
        id_,
        make_neighbor_vector()))
      {
        const auto new_id = new_neighbor->id();
        // This ID already exists; so drop the connection
        if (neighbors_.find(new_id) != neighbors_.end())
        {
          std::cerr << "oh no repeat of " << new_id << "\n";
          return false;
        }
        notify_of_new_neighbor(new_id);
        neighbors_.emplace(new_id, std::move(*new_neighbor));
      }
      return true;
    }

    /** \brief See if there are any pending connections and accept them if so
     */
    void accept_pending_connections()
    {
      while(auto conn = server_socket_.accept())
      {
        if (auto new_neighbor = internal::ExternalMaster::create(
          internal::ByAccept{},
          std::move(*conn),
          id_,
          make_neighbor_vector()))
        {
          const auto new_id = new_neighbor->id();
          // This ID already exists; so drop the connection
          if (neighbors_.find(new_id) != neighbors_.end())
          {
            continue;
          }
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
        internal::on_error("Job with duplicate ID created!");
      }
      // decltype(res) == std::pair<iterator, bool>
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
          process_message(*msg_opt, neighbor.second);
        }
      }
      remove_dead_neighbors();
    }

    /** \brief Broadcast a message to the entire network
     *
     * \param msg_id The message's id
     * \param job_id The id of the job the message is for
     * \param tag_id The id of the tag the message is for
     * \param tag_index The index of the tag
     * \param hops_p1 The number of hops left + 1
     * \param data The data to broadcast
     */
    void do_broadcast(
      const MessageID msg_id,
      const JobID job_id,
      const TagID tag_id,
      const TagIndex tag_index,
      const std::uint32_t hops_left_p1,
      const std::vector<std::byte>& data
    ) noexcept
    {
      // Prepend the message describing the data and send it to all neighbors
      send_to_neighbors(internal::make_broadcast(msg_id, job_id, tag_id, tag_index, id_, hops_left_p1, data));
    }

    // Does all processing that needs to be done when a message is recieved
    void process_message(const internal::MessageHandler& handle, internal::ExternalMaster& from)
    {
      assert(handle.category() == internal::MessageCategory::job);
      const auto okay = handle.do_callback(from,
        [&](const internal::Broadcast& msg) {
          if (!message_is_okay(msg))
          {
            return false;
          }
          if (is_old_message(msg))
          {
            return true;
          }
          if (!add_data_to_queue(msg))
          {
            return false;
          }
          if (msg.hops_left_p1 != 1)
          {
            const auto to_send = handle.rebuild_broadcast(msg);
            // Propagate the message to all neighbors but ones that are known to
            // have already recieve it, so not the sender, the origin, or neighbors
            // that the sender also has
            send_to_neighbors_if(to_send, [&](const internal::ExternalMaster& m) {
              return
                m.id() != from.id() &&
                m.id() != msg.origin &&
                !from.has_neighbor(m.id());
            });
          }
          return true;
        },
        [](...) {
          assert(false && "Invalid message type in Master::process_message");
          return false;
        }
      );
      if (!okay)
      {
        from.mark_as_dead();
      }
    }

    // Adds data to the tag queue for a job from a message
    // Returns true if it was successful, false if something went wrong
    bool add_data_to_queue(const internal::Broadcast& msg)
    {
      return internal::JobBase::Accessor::process_data(
        *jobs_.find(msg.job_id)->second,
        msg.tag_id,
        msg.tag_index,
        msg.data.data(),
        msg.data.size()
      );
    }

    /** \brief Returns true if a message is okay/well-formed, false otherwise
     */
    bool message_is_okay(const internal::Broadcast& msg) noexcept
    {
      if (jobs_.find(msg.job_id) == jobs_.end())
      {
        return false;
      }
      return true;
    }

    /** \brief Returns true if a message is old, false otherwise
     */
    bool is_old_message(const internal::Broadcast& msg) noexcept
    {
      auto& last_id = last_message_id_[msg.origin][msg.job_id];
      if (msg.message_id <= last_id)
      {
        return true;
      }
      last_id = msg.message_id;
      return false;
    }

    /** \brief Notify neighbors of a new new neighbor
     */
    void notify_of_new_neighbor(const MachineID id)
    {
      send_to_neighbors(internal::make_new_neighbor(id));
    }

    /** \brief Removes all dead neighbors
     */
    void remove_dead_neighbors() noexcept
    {
      for (auto it = neighbors_.begin(); it != neighbors_.end(); /* nothing */)
      {
        if (it->second.is_dead())
        {
          send_to_neighbors(internal::make_remove_neighbor(it->first));
          it = neighbors_.erase(it);
        }
        else
        {
          ++it;
        }
      }
    }

    /** \brief Returns a vector of all the neighboring ID's
     */
    std::vector<MachineID> make_neighbor_vector() const
    {
      std::vector<MachineID> to_ret(neighbors_.size());
      std::transform(
        neighbors_.cbegin(),
        neighbors_.cend(),
        to_ret.begin(),
        [](const auto& val) {
          return val.first;
      });
      return to_ret;
    }

    /** \brief Broadcasts a message to all neighbors that fit a criteria
     */
    template<typename Callable>
    void send_to_neighbors_if(const std::vector<std::byte>& to_send, Callable c)
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
    void send_to_neighbors(const std::vector<std::byte>& to_send)
    {
      send_to_neighbors_if(to_send, [](const internal::ExternalMaster&) { return true; });
    }

    // For listening to connection requests
    internal::SocketCommunicator server_socket_;

    // List of the jobs that are present
    std::unordered_map<JobID, std::unique_ptr<internal::JobBase>> jobs_;

    // List of neighboring connections
    std::unordered_map<MachineID, internal::ExternalMaster> neighbors_;

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

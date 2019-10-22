#ifndef SKYNET_MASTER_HPP
#define SKYNET_MASTER_HPP

#include "skynet/internal/devices/socket_communicator.hpp"
#include "skynet/internal/utility/network_conv.hpp"
#include "skynet/internal/capn_proto_wrapper.hpp"
#include "skynet/internal/message_creators.hpp"
#include "skynet/job.hpp"
#include "skynet/types.hpp"

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

// TODO: Support other types of communicators; will probably just be a build
//       configuration since there doesn't seem to be a strong reason for
//       having different kinds of masters in the same program

namespace skynet
{
  namespace internal
  {
    // The default hearbeat interval
    inline static constexpr std::chrono::milliseconds default_heartbeat_interval{2000};

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
      ) noexcept;

      /** \brief Attempt to construct an ExternalMaster using an existing connection
       *
       * This is for when a client connects to a server.
       */
      static std::optional<ExternalMaster> create(
        ByRequest,
        SocketCommunicator conn,
        const MachineID local_id,
        const std::vector<MachineID>& local_neighbors
      ) noexcept;

      /** \brief Recieve a skynet::Message from an external connection if one exists
       *
       * Returns the handler for the message if one exists.
       */
      std::optional<MessageHandler> get_message() noexcept;

      /** \brief Sends a raw message to the other master
       *
       * Also marks the connection as dead if any errors occur.  Does nothing
       * if the connection is marked as dead.
       */
      void send_message(const std::vector<std::byte>& c) noexcept;

      /** \brief Returns the id of the computer this is connected to
       */
      MachineID id() const noexcept;

      /** \brief Returns if the connection is dead or not
       */
      bool is_dead() const noexcept;

      /** \brief Marks the connection as dead
       */
      void mark_as_dead() noexcept;

      /** \brief Returns true if the given neighbor is present, false otherwise
       */
      bool has_neighbor(const MachineID id) const noexcept;

      /** \brief Sends a heartbeat if enough time has passed
       */
      void send_heartbeat_if_past_interval(std::chrono::milliseconds interval) noexcept;

    private:
      // Only allow private construction
      explicit ExternalMaster(SocketCommunicator conn) noexcept;

      // Read some bytes from the connection, returning false if the read failed
      bool read_from_conn(std::byte* const buffer, const std::size_t count) noexcept;

      // Read some bytes from the connection, returning an empty vector if
      // the number of bytes couldn't be read
      std::vector<std::byte> read_from_conn(const std::size_t count) noexcept;

      // Attempt to get a MessageHandler from the connection
      std::optional<MessageHandler> try_to_get_message_handler() noexcept;

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
      bool send_greeting(const MachineID local_id, const std::vector<MachineID>& local_neighbors) noexcept;

      // Wait until the greeting is sent
      bool wait_for_greeting() noexcept;

      // Handle status messages
      void handle_message(MessageHandler& handle) noexcept;

      // For talking with the external master
      SocketCommunicator conn_;

      // The id of the external master
      MachineID id_;

      // Estimated latency to the other machine
      std::chrono::microseconds latency_;

      // The last time the machine was heard from
      std::chrono::steady_clock::time_point last_heard_;

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
      friend class Job;

      static void add_job(
        Master& m,
        const JobID& id,
        Job& to_add
      ) noexcept;

      static void broadcast(
        Master& m,
        const VersionID version,
        const TagID& tag_id,
        const std::uint32_t hops_left_p1,
        PublishDataVariant data
      ) noexcept;
    }; // struct Accessor

    /** \brief Creates a Master instance that listens on the specified
     * port for connections.
     *
     * \param port The port to listen on
     * \param id The ID to assign to this machine
     * \param heartbeat_interval The interval to wait between heartbeats
     */
    template<
      typename Rep = decltype(internal::default_heartbeat_interval)::rep,
      typename Period = decltype(internal::default_heartbeat_interval)::period
    >
    Master(
      const std::uint16_t port,
      const MachineID& id,
      const std::chrono::duration<Rep, Period> heartbeat_interval = internal::default_heartbeat_interval
    ) noexcept
      : Master{port, id, std::chrono::duration_cast<std::chrono::milliseconds>(heartbeat_interval)}
    {}

    /** \brief Constructor specifically for milliseconds
     */
    Master(
      const std::uint16_t port,
      const MachineID& id,
      const std::chrono::milliseconds heartbeat_interval
    ) noexcept;

    /** \brief Destructor; tells all neighbors that the device is dead
     */
    ~Master();

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
    bool connect_to_server(const char* const address, const std::uint16_t port) noexcept;

    /** \brief See if there are any pending connections and accept them if so
     */
    void accept_pending_connections() noexcept;

    /** \brief Returns the number of machines connected
     */
    int number_of_neighbors() const noexcept;

    /** \brief Start running all submitted jobs
     */
    void run() noexcept;

  private:
    /** \brief Listens for messages from neighbors and handles them if there
     * are any.
     */
    void handle_neighbor_messages() noexcept;

    /** \brief Broadcast a message to the entire network
     *
     * \param msg_id The message's version
     * \param tag_id The id of the tag the message is for
     * \param hops_p1 The number of hops left + 1
     * \param data The data to broadcast
     */
    void do_broadcast(
      const VersionID version,
      const TagID& tag_id,
      const std::uint8_t hops_left_p1,
      PublishDataVariant data
    ) noexcept;

    // Does all processing that needs to be done when a message is recieved
    void process_message(internal::MessageHandler& handle, internal::ExternalMaster& from) noexcept;

    // Adds data to the tag queue for a job from a message
    // Returns true if it was successful, false if something went wrong
    bool add_data_to_queue(const internal::Publish& msg) noexcept;

    /** \brief Returns true if a message is old, false otherwise
     */
    bool is_old_message(const internal::Publish& msg) noexcept;

    /** \brief Notify neighbors of a new new neighbor
     */
    void notify_of_new_neighbor(const MachineID id) noexcept;

    /** \brief Removes all dead neighbors
     */
    void remove_dead_neighbors() noexcept;

    /** \brief Returns a vector of all the neighboring ID's
     */
    std::vector<MachineID> make_neighbor_vector() const noexcept;

    /** \brief Broadcasts a message to all neighbors that fit a criteria
     */
    template<typename Callable>
    void send_to_neighbors_if(const std::vector<std::byte>& to_send, Callable c) noexcept
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
    void send_to_neighbors(const std::vector<std::byte>& to_send) noexcept;

    // For listening to connection requests
    internal::SocketCommunicator server_socket_;

    // List of the jobs that are present
    std::unordered_map<JobID, Job*> jobs_;

    // List of neighboring connections
    std::unordered_map<MachineID, internal::ExternalMaster> neighbors_;

    // The last heard message id for each tag in the network
    std::unordered_map<TagID, VersionID> last_tag_id_;

    // The id of this machine
    MachineID id_;

    // The time to send a heartbeat if nothing has been heard in the time
    std::chrono::milliseconds heartbeat_interval_;
  }; // class Master
} // namespace skynet

#endif // SKYNET_MASTER_HPP

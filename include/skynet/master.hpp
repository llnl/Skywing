#ifndef SKYNET_MASTER_HPP
#define SKYNET_MASTER_HPP

#include "skynet/internal/devices/socket_communicator.hpp"
#include "skynet/internal/utility/network_conv.hpp"
#include "skynet/internal/capn_proto_wrapper.hpp"
#include "skynet/internal/master_future_callables.hpp"
#include "skynet/internal/message_creators.hpp"
#include "skynet/internal/reduce_group.hpp"
#include "skynet/job.hpp"
#include "skynet/types.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// TODO: Support other types of communicators; will probably just be a build
//       configuration since there doesn't seem to be a strong reason for
//       having different kinds of masters in the same program

namespace skynet
{
  // Forward declaration
  class Master;

  // The port difference between the socket used from general communication by
  // a master and the port used for publications
  inline constexpr std::uint16_t publisher_port_offset = 100;

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
        const MachineID& local_id,
        const std::vector<MachineID>& local_neighbors,
        Master& master,
        std::uint16_t base_port
      ) noexcept;

      /** \brief Attempt to construct an ExternalMaster using an existing connection
       *
       * This is for when a client connects to a server.
       */
      static std::optional<ExternalMaster> create(
        ByRequest,
        SocketCommunicator conn,
        const MachineID& local_id,
        const std::vector<MachineID>& local_neighbors,
        Master& master,
        std::uint16_t base_port
      ) noexcept;

      /** \brief Handles any messages sent from the connection
       */
      void get_and_handle_messages() noexcept;

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
      bool has_neighbor(const MachineID& id) const noexcept;

      /** \brief Sends a heartbeat if enough time has passed
       */
      void send_heartbeat_if_past_interval(std::chrono::milliseconds interval) noexcept;

      /** \brief Begins the search process for publishers tags
       */
      void find_publishers_for_tags(
        const std::vector<TagID>& tags,
        bool ignore_cache
      ) noexcept;

      /** \brief The address for two-way communication with the external master
       */
      std::string two_way_address() const noexcept;

      /** \brief The address of the publisher for the external master
       */
      std::string publisher_address() const noexcept;

    private:
      // Only allow private construction from a socket
      explicit ExternalMaster(SocketCommunicator conn) noexcept;

      // Read some bytes from the connection, returning false if the read failed
      bool read_from_conn(std::byte* buffer, std::size_t count) noexcept;

      // Read some bytes from the connection, returning an empty vector if
      // the number of bytes couldn't be read
      std::vector<std::byte> read_from_conn(std::size_t count) noexcept;

      // Attempt to get a StatusMessageHandler from the connection
      std::optional<StatusMessageHandler> try_to_get_status_message() noexcept;

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
      bool send_greeting(
        const MachineID& local_id,
        const std::vector<MachineID>& local_neighbors,
        std::uint16_t base_port
      ) noexcept;

      // Wait until the greeting is sent
      bool wait_for_greeting() noexcept;

      // Handle status messages
      void handle_message(StatusMessageHandler& handle) noexcept;

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

      // The owning master
      Master* master_;

      // The base port to use to connect to the remote machine
      std::uint16_t base_port_;

      // If the next request for tags should ignore the cache or not
      bool ignore_cache_on_next_request_ = false;

      // If the connection is dead or not
      bool dead_{false};
    }; // class ExternalMaster
  } // namespace internal

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

    /** \brief Connects to another instance at the specified address on
     * the specified port
     *
     * \param address The address to connect to
     * \param port The port to connect on
     * \return True if the connection was successful, false if it failed
     */
    bool connect_to_server(const char* const address, const std::uint16_t port) noexcept;

    /** \brief Connects to another instance with the address:port format
     */
    bool connect_to_server(std::string_view address) noexcept;

    /** \brief See if there are any pending connections and accept them if so
     */
    void accept_pending_connections() noexcept;

    /** \brief Returns the number of machines connected
     */
    int number_of_neighbors() const noexcept;

    /** \brief Creates a job for the master to execute that produces the
     * specified tags.
     *
     * Returns false if the job could not be inserted (only happens on name collision)
     */
    bool submit_job(
      JobID name,
      std::function<void(Job&)> to_run
    ) noexcept;

    /** \brief Start running all submitted jobs
     */
    void run() noexcept;

    /** \brief Return the local address of the master, for subscribing to self
     */
    std::string local_publishing_address() const noexcept;

    /** \brief Returns the number of subscribers
     */
    int num_subscribers() const noexcept;

    /** \brief Returns the id of the master
     */
    const std::string& id() const noexcept;

    // Access for the Job class
    struct JobAccessor
    {
    private:
      friend class Job;

      static void publish(
        Master& m,
        const VersionID version,
        const TagID& tag_id,
        const PublishValueVariant& value
      ) noexcept
      {
        std::unique_lock lock{m.job_mut_};
        m.publish(version, tag_id, value);
      }

      static void report_new_publish_tags(
        Master& m,
        const std::vector<TagID>& tags
      ) noexcept
      {
        std::unique_lock{m.job_mut_};
        m.report_new_publish_tags(tags);
      }

      static auto subscribe(
        Master& m,
        const std::vector<TagID>& tag_ids
      ) noexcept
      {
        std::unique_lock lock{m.job_mut_};
        return m.subscribe(tag_ids);
      }

      static auto create_reduce_group(
        Master& m,
        const TagID& group_id,
        const TagID& tag_produced,
        const internal::ReduceGroupNeighbors& tags_to_find,
        const std::uint8_t expected_type
      ) noexcept
      {
        std::unique_lock lock{m.job_mut_};
        return m.create_reduce_group(group_id, tag_produced, tags_to_find, expected_type);
      }
    }; // struct JobAccessor

    // Accessor for the ExternalMaster class
    struct ExternalMasterAccessor
    {
    private:
      friend class internal::ExternalMaster;

      static void handle_get_publishers(
        Master& m,
        const internal::GetPublishers& msg,
        internal::ExternalMaster& from
      ) noexcept
      {
        m.handle_get_publishers(msg, from);
      }

      static auto add_publishers_and_propagate(
        Master& m,
        const internal::ReportPublishers& msg,
        const internal::ExternalMaster& from
      ) noexcept
      {
        return m.add_publishers_and_propagate(msg, from);
      }

      static bool handle_join_reduce_group(
        Master& m,
        const internal::JoinReduceGroup& msg,
        const internal::ExternalMaster& from
      ) noexcept
      {
        return m.handle_join_reduce_group(msg, from);
      }

      static bool handle_submit_reduce_value(
        Master& m,
        const internal::SubmitReduceValue& msg,
        const internal::ExternalMaster& from
      ) noexcept
      {
        return m.handle_submit_reduce_value(msg, from);
      }

      static const std::vector<TagID>& get_pending_tags(Master& m) noexcept
      {
        return m.get_pending_tags();
      }
    }; // struct ExternalMasterAccessor

    struct ReduceGroupAccessor
    {
    private:
      friend class internal::ReduceGroupBase;

      static void send_reduce_value_to_parent(
        Master& m,
        const TagID& group_id,
        const VersionID version,
        const TagID& produced_tag,
        const PublishValueVariant& value
      ) noexcept
      {
        m.send_reduce_value_to_parent(group_id, version, produced_tag, value);
      }
    }; // struct ReduceGroupAccessor

    struct FutureAccessor
    {
    private:
      friend class internal::MasterSubscribeIsDone;
      friend class internal::MasterReduceGroupIsCreated;
      friend class internal::MasterGetReduceGroup;

      static bool subscribe_is_done(Master& m, const std::vector<TagID>& tags) noexcept
      {
        return m.subscribe_is_done(tags);
      }

      static bool reduce_group_is_created(Master& m, const TagID& group_id) noexcept
      {
        return m.reduce_group_is_created(group_id);
      }

      static internal::ReduceGroupBase& get_reduce_group(Master& m, const TagID& group_id) noexcept
      {
        return m.get_reduce_group(group_id);
      }
    }; // struct FutureAccessor

  private:
    /** \brief Connects to a remote connection and returns an iterator to the new connection,
     * or an end iterator if the connection failed
     */
    std::unordered_map<MachineID, internal::ExternalMaster>::iterator connect_impl(
      const char* address,
      std::uint16_t port
    ) noexcept;

    /** \brief Listens for messages from neighbors and handles them if there
     * are any.
     */
    void handle_neighbor_messages() noexcept;

    /** \brief Broadcast a message to the entire network
     *
     * \param version The message's version
     * \param tag_id The id of the tag the message is for
     * \param value The value to send
     */
    void publish(
      const VersionID version,
      const TagID& tag_id,
      const PublishValueVariant& value
    ) noexcept;

    // Adds data to the tag queue for a job from a message
    // Returns true if it was successful, false if something went wrong
    bool add_data_to_queue(const internal::PublishData& msg) noexcept;

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

    // Auxillary function to help with subscribe function
    bool subscribe_is_done(const std::vector<TagID>& required_tags) noexcept;

    /** \brief Subscribes to the passed tags.
     */
    auto subscribe(const std::vector<TagID>& tag_ids) noexcept
      -> internal::Future<void, internal::MasterSubscribeIsDone, internal::FutureGetNoOp>;

    /** \brief Handles the get_publishers message
     */
    void handle_get_publishers(
      const internal::GetPublishers& msg,
      internal::ExternalMaster& from
    ) noexcept;

    /** \brief Removes any tags that have known publishers
     */
    std::vector<TagID> remove_tags_with_known_publishers(const internal::GetPublishers& msg) noexcept;

    /** \brief Adds the publishers and propagate the information is required
     *
     * Returns a bool indicating if the next request for publishers should ignore the cache
     */
    bool add_publishers_and_propagate(
      const internal::ReportPublishers& msg,
      const internal::ExternalMaster& from
    ) noexcept;

    /** \brief Produce a message containing the known publishers and tags
     */
    std::vector<std::byte> make_known_tag_publisher_message() const noexcept;

    /** \brief Attempt to subscribe on the passed address
     */
    bool try_to_subscribe(std::string_view address, std::vector<std::string> remote_tags_produced) noexcept;

    /** \brief Reads data from any subscriptions
     */
    void read_data_from_subscriptions() noexcept;

    /** \brief Returns the list of tags that a publisher is known to produce
     */
    std::vector<std::string> get_tags_for_publisher(std::string_view publisher_address) const noexcept;

    /** \brief Reports when new tags are being produced
     */
    void report_new_publish_tags(const std::vector<TagID>& tags) noexcept;

    /** \brief Starts the process of creating a reduce group
     */
    auto create_reduce_group(
      const TagID& group_id,
      const TagID& tag_produced,
      const internal::ReduceGroupNeighbors& tags_to_find,
      std::uint8_t expected_type
    ) noexcept
      -> internal::Future<internal::ReduceGroupBase&, internal::MasterReduceGroupIsCreated, internal::MasterGetReduceGroup>;

    /** \brief Returns true if the specified reduce group has been successfully created.
     *
     * "Success" in this case means that a connection with a parent and both children
     * has been established; there is no way to determine if the entire tree has been established.
     */
    bool reduce_group_is_created(const TagID& group_id) noexcept;

    /** \brief Handles a message that a child is joining a reduce group
     */
    bool handle_join_reduce_group(
      const internal::JoinReduceGroup& msg,
      const internal::ExternalMaster& from
    ) noexcept;

    /** \brief Returns a reference to a created reduce group
     *
     * \pre The reduce group exists
     */
    internal::ReduceGroupBase& get_reduce_group(const TagID& group_id) noexcept;

    /** \brief Sends a value for a reduce to the corresponding parents
     */
    void send_reduce_value_to_parent(
      const TagID& group_id,
      const VersionID version,
      const TagID& produced_tag,
      const PublishValueVariant& value
    ) noexcept;

    /** \brief Handles a submit reduce value message
     */
    bool handle_submit_reduce_value(
      const internal::SubmitReduceValue& msg,
      const internal::ExternalMaster& from
    ) noexcept;

    /** \brief Returns tags that are still pending.
     */
    const std::vector<TagID>& get_pending_tags() noexcept;

    // TODO: The return value/type for this feels really weird; probably want
    // to change it to use a enum class or something at some point?
    /** \brief Attempt to create connections for any pending tags.
     *
     * Returns true if the the cache needs to be ignored for the next request.
     */
    bool try_connections_for_pending_tags() noexcept;

    // For listening to connection requests
    internal::SocketCommunicator server_socket_;

    // List of the jobs that are present
    std::unordered_map<JobID, Job> jobs_;

    // List of neighboring connections
    std::unordered_map<MachineID, internal::ExternalMaster> neighbors_;

    // Subscriptions and the tags that they produce
    struct SubscriptionData
    {
      internal::Subscription subscription;
      std::vector<TagID> produced_tags;
    };
    std::vector<SubscriptionData> subscriptions_;

    // List of publishers that are known for each tag
    std::unordered_map<TagID, std::unordered_set<std::string>> publishers_for_tag_;

    // A list of tags that still need to have publishers found
    std::vector<std::string> pending_tags_;

    // Information for reduce groups, holds the tags that each group has and
    // ID for the machines that produce those tags for the group
    struct ReduceGroupData
    {
      // Need a constructor since ReduceGroups aren't movable so they need to
      // be constructed in place
      // This could be forwarded or something, but there doesn't seem to be a strong
      // motivating reason to do so
      ReduceGroupData(
        const internal::ReduceGroupNeighbors& tag_neighbors,
        Master& master,
        const TagID& group_id,
        const TagID& produced_tag,
        const std::uint8_t expected_type
      ) noexcept
        : group{tag_neighbors, master, group_id, produced_tag, expected_type}
      {}

      internal::ReduceGroupBase group;
      std::vector<MachineID> parent_machines;
      std::array<std::vector<MachineID>, 2> child_machines;
    };
    std::unordered_map<TagID, ReduceGroupData> reduce_tag_data_;

    // The id of this machine
    MachineID id_;

    // The time to send a heartbeat if nothing has been heard in the time
    std::chrono::milliseconds heartbeat_interval_;

    // Only allow one job access to the master at a time
    std::mutex job_mut_;

    // List of machines that are waiting for information for producers of a certain tag
    // Uses MachineID's instead of pointer in case the remote machine disconnects and
    // the ExternalMaster is deleted between the time a request is started and a response
    // is recieved
    std::unordered_map<TagID, std::unordered_set<MachineID>> send_publisher_information_to_;

    // The tags that this machine produces
    std::unordered_set<TagID> produced_tags_;

    // The publication channel for this machine
    internal::PublicationChannel pub_channel_;

    // The port used for communications
    std::uint16_t comm_port_;

    // Notification for when new subscriptions are created
    std::condition_variable new_subscription_cv_;

    // Notification for when reduce group related connections are made
    std::condition_variable reduce_group_cv_;
  }; // class Master
} // namespace skynet

#endif // SKYNET_MASTER_HPP

#ifndef SKYNET_MASTER_HPP
#define SKYNET_MASTER_HPP

#include "skynet_core/internal/devices/socket_communicator.hpp"
#include "skynet_core/internal/capn_proto_wrapper.hpp"
#include "skynet_core/internal/master_waiter_callables.hpp"
#include "skynet_core/internal/message_creators.hpp"
#include "skynet_core/internal/reduce_group.hpp"
// #include "skynet_core/basic_master_config.hpp"
#include "skynet_core/job.hpp"
#include "skynet_core/types.hpp"

#include "gsl/span"

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

namespace skynet
{
  class Master;
  class MasterHandle;
  class Job;

  namespace internal
  {
    // The default hearbeat interval
    inline static constexpr std::chrono::milliseconds default_heartbeat_interval{5000};

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
      ExternalMaster(
        SocketCommunicator comm,
        const MachineID& id,
        const std::vector<MachineID>& neighbors,
        Master& master,
        std::uint16_t port
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

      /** \brief Begins the search process for the specified tags
       */
      void find_publishers_for_tags(const std::vector<TagID>& tags) noexcept;

      /** \brief The address for communication with the external master
       */
      std::string address() const noexcept;

      /** \brief Pair version of the address
       */
      AddrPortPair address_pair() const noexcept;

      /** \brief Sets the external master to ignore the cache on the next request
       * for publishers
       */
      void ignore_cache_on_next_request() noexcept;

      /** \brief Returns true if the external master is subscribed to the tag,
       * returns false if it is not.
       */
      bool is_subscribed_to(const TagID& tag) const noexcept;

      /** \brief Returns true if tags should be asked for
       */
      bool should_ask_for_tags() const noexcept;

      /** \brief Returns true if there are pending tags
       */
      bool has_pending_tag_request() const noexcept;

      /** \brief Resets the backoff counter
       */
      void reset_backoff_counter() noexcept;

      /** \brief Increments the backoff counter
       */
      void increase_backoff_counter() noexcept;

    private:
      // Read some bytes from the connection, returning false if the read failed
      bool read_from_conn(std::byte* buffer, std::size_t count) noexcept;

      // Read some bytes from the connection, returning an empty vector if
      // the number of bytes couldn't be read
      std::vector<std::byte> read_from_conn(std::size_t count) noexcept;

      // Attempts to get a message
      std::optional<MessageHandler> try_to_get_message() noexcept;

      // Handle status messages
      void handle_message(MessageHandler& handle) noexcept;

      // Calculate the next time tags should be requested
      std::chrono::steady_clock::time_point calc_next_request_time() const noexcept;

      // For talking with the external master
      SocketCommunicator conn_;

      // The id of the external master
      MachineID id_;

      // The last time the machine was heard from
      std::chrono::steady_clock::time_point last_heard_;

      // The neighbors that the external machine has
      std::vector<MachineID> neighbors_;

      // The owning master
      Master* master_;

      // The time that will be waited until requesting tags again
      std::chrono::steady_clock::time_point request_tags_time_;

      // Tags that the remote is subscribed for
      // std::unordered_set for fast look-up
      std::unordered_set<TagID> remote_subscriptions_;

      // The port to use to connect to the remote machine
      std::uint16_t port_;

      // The number of times requests have been unfulfilled
      std::uint8_t backoff_counter_ = 0;

      // If the next request for tags should ignore the cache or not
      bool ignore_cache_on_next_request_ = false;

      // If the connection is dead or not
      bool dead_ = false;

      // If there is a request out for tags or not
      bool pending_tag_request_ = false;
    }; // class ExternalMaster
  } // namespace internal

  /** \brief The master Skynet instance used for communication
   */
  class Master
  {
  public:
    friend class MasterHandle;
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

    // /** \brief Constructor for building from a file format specified in
    //  * basic_master_config.hpp
    //  *
    //  * This will block until all of the specified connections have been made
    //  */
    // Master(const BuildMasterInfo& info) noexcept;

    /** \brief Destructor; tells all neighbors that the device is dead
     */
    ~Master();

    /** \brief Creates a job for the master to execute that produces the
     * specified tags.
     *
     * Returns false if the job could not be inserted (only happens on name collision)
     */
    bool submit_job(
      JobID name,
      std::function<void(Job&, MasterHandle)> to_run
    ) noexcept;

    /** \brief Start running all submitted jobs
     */
    void run() noexcept;

    /** \brief Gets the id of the master
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
        gsl::span<PublishValueVariant> value
      ) noexcept
      {
        std::lock_guard lock{m.job_mut_};
        m.publish(version, tag_id, value);
      }

      static void report_new_publish_tags(
        Master& m,
        const std::vector<TagID>& tags
      ) noexcept
      {
        std::lock_guard lock{m.job_mut_};
        m.report_new_publish_tags(tags);
      }

      static auto subscribe(
        Master& m,
        const std::vector<TagID>& tag_ids
      ) noexcept
      {
        std::lock_guard lock{m.job_mut_};
        return m.subscribe(tag_ids);
      }

      static auto create_reduce_group(
        Master& m,
        std::unique_ptr<internal::ReduceGroupBase> group_ptr
      ) noexcept
      {
        std::lock_guard lock{m.job_mut_};
        return m.create_reduce_group(std::move(group_ptr));
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

      static void add_publishers_and_propagate(
        Master& m,
        const internal::ReportPublishers& msg,
        const internal::ExternalMaster& from
      ) noexcept
      {
        m.add_publishers_and_propagate(msg, from);
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

      static bool handle_report_reduce_disconnection(
        Master& m,
        const internal::ReportReduceDisconnection& msg,
        const internal::ExternalMaster& from
      ) noexcept
      {
        return m.handle_report_reduce_disconnection(msg, from);
      }

      static bool subscription_tags_are_produced(
        Master& m,
        const internal::SubscriptionNotice& msg
      ) noexcept
      {
        return m.subscription_tags_are_produced(msg);
      }

      static bool handle_publish_data(
        Master& m,
        const internal::PublishData& msg,
        const internal::ExternalMaster& from
      ) noexcept
      {
        return m.handle_publish_data(msg, from);
      }
    }; // struct ExternalMasterAccessor

    struct ReduceGroupAccessor
    {
    private:
      friend class internal::ReduceGroupBase;

      static void send_reduce_data_to_parent(
        Master& m,
        const TagID& group_id,
        const VersionID version,
        const TagID& reduce_tag,
        gsl::span<const PublishValueVariant> value
      ) noexcept
      {
        m.send_reduce_data_to_parent(group_id, version, reduce_tag, value);
      }

      static void send_reduce_data_to_children(
        Master& m,
        const TagID& group_id,
        const VersionID version,
        const TagID& reduce_tag,
        gsl::span<const PublishValueVariant> value
      ) noexcept
      {
        m.send_reduce_data_to_children(group_id, version, reduce_tag, value);
      }

      static void send_report_disconnection(
        Master& m,
        const TagID& group_id,
        const MachineID& initiating_machine,
        const ReductionDisconnectID disconnect_id
      ) noexcept
      {
        m.send_report_disconnection(group_id, initiating_machine, disconnect_id);
      }

      static auto rebuild_reduce_group(
        Master& m,
        const TagID& group_id
      ) noexcept
      {
        std::lock_guard<std::mutex> lock{m.job_mut_};
        return m.rebuild_reduce_group(group_id);
      }
    }; // struct ReduceGroupAccessor

    struct WaiterAccessor
    {
    private:
      friend class internal::MasterSubscribeIsDone;
      friend class internal::MasterReduceGroupIsCreated;
      friend class internal::MasterGetReduceGroup;
      friend class internal::MasterConnectionIsComplete;
      friend class internal::MasterGetConnectionSuccess;

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

      static bool conn_is_complete(Master& m, const AddrPortPair& address) noexcept
      {
        return m.conn_is_complete(address);
      }

      static bool conn_get_success(Master& m, const AddrPortPair& address) noexcept
      {
        return m.addr_is_connected(address);
      }
    }; // struct WaiterAccessor

  private:
    ///////////////////////////////////////
    // Interface for MasterHandle
    ///////////////////////////////////////

    auto connect_to_server(const char* const address, const std::uint16_t port) noexcept
      -> Waiter<internal::MasterConnectionIsComplete, internal::MasterGetConnectionSuccess>;
    auto connect_to_server(std::string_view address) noexcept
      -> Waiter<internal::MasterConnectionIsComplete, internal::MasterGetConnectionSuccess>;
    int number_of_neighbors() const noexcept;
    int number_of_subscribers() const noexcept;
    int num_subscriptions(const internal::PublishTagBase& tag) const noexcept;

    ///////////////////////////////////////
    // End Interface for MasterHandle
    ///////////////////////////////////////

    /** \brief See if there are any pending connections and accept them if so
     */
    void accept_pending_connections() noexcept;

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
      gsl::span<PublishValueVariant> value
    ) noexcept;

    // Adds data to the tag queue for a job from a message
    // Returns true if it was successful, false if something went wrong
    bool add_data_to_queue(const internal::PublishData& msg) noexcept;

    /** \brief Notify neighbors of a new new neighbor
     */
    void notify_of_new_neighbor(const MachineID& id) noexcept;

    /** \brief Removes all dead neighbors
     */
    void remove_dead_neighbors() noexcept;

    /** \brief Returns a vector of all the neighboring ID's
     */
    std::vector<MachineID> make_neighbor_vector() const noexcept;

    /** \brief Broadcasts a message to all neighbors that fit a criteria
     */
    template<typename Callable>
    void send_to_neighbors_if(const std::vector<std::byte>& to_send, Callable condition) noexcept
    {
      for (auto&& neighbor : neighbors_)
      {
        if (condition(neighbor.second))
        {
          neighbor.second.send_message(to_send);
        }
      }
    }

    /** \brief Broadcasts a message to all neighbors
     */
    void send_to_neighbors(const std::vector<std::byte>& to_send) noexcept;

    // Auxillary function to help with subscribe function
    bool subscribe_is_done(const std::vector<TagID>& required_tags) const noexcept;

    /** \brief Subscribes to the passed tags.
     */
    auto subscribe(const std::vector<TagID>& tag_ids) noexcept
      -> Waiter<internal::MasterSubscribeIsDone, internal::WaiterGetNoOp>;

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
    void add_publishers_and_propagate(
      const internal::ReportPublishers& msg,
      const internal::ExternalMaster& from
    ) noexcept;

    /** \brief Produce a message containing the known publishers and tags
     */
    std::vector<std::byte> make_known_tag_publisher_message() const noexcept;

    /** \brief Returns the list of tags that a publisher is known to produce
     */
    std::vector<std::string> get_tags_for_publisher(std::string_view publisher_address) const noexcept;

    /** \brief Reports when new tags are being produced
     */
    void report_new_publish_tags(const std::vector<TagID>& tags) noexcept;

    /** \brief Starts the process of creating a reduce group
     */
    auto create_reduce_group(
      std::unique_ptr<internal::ReduceGroupBase> group_ptr
    ) noexcept
      -> Waiter<internal::MasterReduceGroupIsCreated, internal::MasterGetReduceGroup>;

    /** \brief Gets a future for when a reduce group has been re-built.
     */
    auto rebuild_reduce_group(
      const TagID& group_id
    ) noexcept
      -> Waiter<internal::MasterReduceGroupIsCreated, internal::WaiterGetNoOp>;

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

    /** \brief Sends a raw message to the specified ID's, removing the ID's from
     * the array if not present
     */
    void reduce_send_data_and_remove_missing(
      std::vector<MachineID>& machines,
      const std::vector<std::byte>& message
    ) noexcept;

    /** \brief Sends a value for a reduce to the corresponding parents
     */
    void send_reduce_data_to_parent(
      const TagID& group_id,
      const VersionID version,
      const TagID& reduce_tag,
      gsl::span<const PublishValueVariant> value
    ) noexcept;

    void send_reduce_data_to_children(
      const TagID& group_id,
      const VersionID version,
      const TagID& reduce_tag,
      gsl::span<const PublishValueVariant> value
    ) noexcept;

    void send_report_disconnection(
      const TagID& group_id,
      const MachineID& initiating_machine,
      const ReductionDisconnectID disconnect_id
    ) noexcept;

    /** \brief Handles a submit reduce value message
     */
    bool handle_submit_reduce_value(
      const internal::SubmitReduceValue& msg,
      const internal::ExternalMaster& from
    ) noexcept;

    /** \brief Implementation of the two above functions
     */
    bool handle_reduce_value(
      const TagID& reduce_group_id,
      const internal::PublishData& value,
      const internal::ExternalMaster& from
    ) noexcept;

    /** \brief Handle a reduce disconnect notification
     */
    bool handle_report_reduce_disconnection(
      const internal::ReportReduceDisconnection& msg,
      const internal::ExternalMaster& from
    ) noexcept;

    /** \brief Attempt to create connections for any pending tags.
     */
    void init_connections_for_pending_tags() noexcept;

    /** \brief Returns true if the connection to the specified address is complete
     */
    bool conn_is_complete(const AddrPortPair& address) noexcept;

    /** \brief Returns true if the connection was successful, false otherwise
     *
     * More accurately, checks if an address is currently connected, which may
     * be usedful to expose at some point?
     */
    bool addr_is_connected(const AddrPortPair& address) const noexcept;

    /** \brief Process pending user requested connections
     */
    void process_pending_conns() noexcept;

    /** \brief Creates the message for the initial handshake
     */
    std::vector<std::byte> make_handshake() const noexcept;

    /** \brief Does the final steps needed for creating a reduce group
     */
    void finalize_reduce_group(const MachineID& parent_machine_id, const TagID& group_tag) noexcept;

    /** \brief Returns a reduce_tag_data_ iterator from the parent machine
     *
     * Implemented in here since it would have to be below the member variables otherwise
     */
    decltype(auto) group_from_parent_tag(const TagID& parent_tag) noexcept
    {
      // TODO: Keep a look-up map if this becomes a performance issue
      for (auto& data_pair : reduce_tag_data_)
      {
        const auto& group_data = data_pair.second;
        const auto& parent = internal::ReduceGroupBase::Accessor::tag_neighbors(*group_data.group).parent();
        if (parent == parent_tag)
        {
          return data_pair;
        }
      }
      assert(false && "No group matching the produced tag found?");
      return *reduce_tag_data_.begin();
    }

    /** \brief Returns true if the subscription tags are all produced
     */
    bool subscription_tags_are_produced(const internal::SubscriptionNotice& msg) const noexcept;

    /** \brief Handles published information
     */
    bool handle_publish_data(const internal::PublishData& msg, const internal::ExternalMaster& from) noexcept;

    /** \brief Finalizes a subscription connection.
     *
     * \param tags '\0' seperated list of tags
     */
    void finalize_subscription(const std::string& tags, internal::ExternalMaster& source) noexcept;

    /** \brief Asks neighbors for publishers for pending tags with no know publishers
     */
    void find_publishers_for_pending_tags() noexcept;

    // For listening to connection requests
    internal::SocketCommunicator server_socket_;

    // List of the jobs that are present
    std::unordered_map<JobID, Job> jobs_;

    // List of neighboring connections
    std::unordered_map<MachineID, internal::ExternalMaster> neighbors_;

    // List of publishers that are known for each tag
    std::unordered_map<TagID, std::unordered_set<std::string>> publishers_for_tag_;

    // A list of tags that still need to have publishers found
    std::vector<std::string> pending_tags_;

    // Information for reduce groups, holds the tags that each group has and
    // ID for the machines that produce those tags for the group
    struct ReduceGroupData
    {
      explicit ReduceGroupData(std::unique_ptr<internal::ReduceGroupBase> group_ptr) noexcept
        : group{std::move(group_ptr)}
      {}

      // unique_ptr so that this is a movable type
      std::unique_ptr<internal::ReduceGroupBase> group;
      std::vector<MachineID> parent_machines;
      std::array<std::vector<MachineID>, 2> child_machines;
    };
    std::unordered_map<TagID, ReduceGroupData> reduce_tag_data_;

    // The id of this machine
    MachineID id_;

    // The time to send a heartbeat if nothing has been heard in the time
    std::chrono::milliseconds heartbeat_interval_;

    // Only allow one job access to the master at a time
    mutable std::mutex job_mut_;

    // List of machines that are waiting for information for producers of a certain tag
    // Uses MachineID's instead of pointers in case the remote machine disconnects and
    // the ExternalMaster is deleted between the time a request is started and a response
    // is received
    // TODO: Maybe move to pointers and just make sure to remove them when the neighbor is removed?
    // Also potentially combine with tag_to_machine_ since they are tags into the same thing
    std::unordered_map<TagID, std::unordered_set<MachineID>> send_publisher_information_to_;

    // The tags that this machine produces
    std::unordered_set<TagID> produced_tags_;

    // The port used for communications
    std::uint16_t port_;

    // Mapping from a machine address to a pointer to the external master
    // This is also used for testing that a connection has completed
    std::unordered_map<AddrPortPair, internal::ExternalMaster*> addr_to_machine_;

    // Mapping from a tag to the ID used for the subscription to the tag
    // Used to know when a subscription is done and for if multiple jobs
    // subscribe to the same tag
    // This is also use to mark when a pending connection is for a tag
    std::unordered_map<TagID, internal::ExternalMaster*> tag_to_machine_;

    /** \brief Connection status for pending connections
     */
    enum class ConnStatus
    {
      waiting_for_conn,
      waiting_for_resp
    };

    /** \brief Type of pending connection
     */
    enum class ConnType
    {
      user_requested,
      by_accept,
      subscription,
      reduce_group
    };
    // Pending connections for all types
    struct PendingInfo
    {
      internal::SocketCommunicator conn;
      ConnStatus status;
      ConnType type;
      std::string tag;
    };
    std::unordered_map<AddrPortPair, PendingInfo> pending_conns_;

    // Notification for when new subscriptions are created
    std::condition_variable new_subscription_cv_;

    // Notification for when reduce group related connections are made
    std::condition_variable reduce_group_cv_;

    // Notification for when connections are complete
    std::condition_variable connection_cv_;

    // Booleans separate for if notifications should be raised so that
    // the CV's can use notifications while the mutex is released
    bool notify_new_subscriptions_ = false;
    bool notify_reduce_group_ = false;
    bool notify_connection_ = false;
  }; // class Master

  class MasterHandle
  {
  public:
    /** \brief Connects to another instance at the specified address on
     * the specified port
     *
     * \param address The address to connect to
     * \param port The port to connect on
     */
    auto connect_to_server(const char* const address, const std::uint16_t port) noexcept
      -> Waiter<internal::MasterConnectionIsComplete, internal::MasterGetConnectionSuccess>
    {
      return handle_->connect_to_server(address, port);
    }

    /** \brief Connects to another instance with the address:port format
     */
    auto connect_to_server(std::string_view address) noexcept
      -> Waiter<internal::MasterConnectionIsComplete, internal::MasterGetConnectionSuccess>
    {
      return handle_->connect_to_server(address);
    }

    /** \brief Returns the number of machines connected
     */
    int number_of_neighbors() const noexcept { return handle_->number_of_neighbors(); }

    /** \brief Returns the number of subscribers
     */
    int number_of_subscribers() const noexcept { return handle_->number_of_subscribers(); }

    /** \brief Returns the id of the master
     */
    const std::string& id() const noexcept { return handle_->id(); }

    /** \brief Returns the number of subscriptions a tag has
     */
    int num_subscriptions(const internal::PublishTagBase& tag) const noexcept
    {
      return handle_->num_subscriptions(tag);
    }

  private:
    friend class Job;

    // Private so that only jobs can create a handle
    explicit MasterHandle(Master& m) noexcept : handle_{&m} {}

    Master* handle_;
  }; // class MasterHandle
} // namespace skynet

#endif // SKYNET_MASTER_HPP

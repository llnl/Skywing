#ifndef SKYWING_MANAGER_HPP
#define SKYWING_MANAGER_HPP

#include "tag.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "skywing_core/internal/capn_proto_wrapper.hpp"
#include "skywing_core/internal/devices/socket_communicator.hpp"
#include "skywing_core/internal/manager_waiter_callables.hpp"
#include "skywing_core/job.hpp"
#include "skywing_core/neighbor_agent.hpp"
#include "skywing_core/types.hpp"

// This has to be separate due to requiring hashing support for the structure
namespace skywing::internal
{
/** \brief Class for publisher names / addresses; would be a local structure
 * inside the manager class, but hashing support is needed
 */
struct PublisherInfo
{
    SocketAddr address;
    MachineID machine_id;

    // Hidden friend idiom - will only be found via ADL
    friend bool operator==(const PublisherInfo& lhs,
                           const PublisherInfo& rhs) noexcept
    {
        return lhs.address == rhs.address && lhs.machine_id == rhs.machine_id;
    }
}; // struct PublisherInfo
} // namespace skywing::internal

template <>
struct std::hash<skywing::internal::PublisherInfo>
{
    std::size_t
    operator()(const skywing::internal::PublisherInfo& i) const noexcept
    {
        return std::hash<skywing::SocketAddr>{}(i.address)
               ^ std::hash<skywing::MachineID>{}(i.machine_id);
    }
}; // struct std::hash

namespace skywing
{
class Manager;
class ManagerHandle;
class Job;

namespace internal
{
class MessageHandler;

/** \brief Tag to indicate that this connection was made by accepting a
 * connection
 */
struct ByAccept
{};

/** \brief Tag to indicate that this connection was made by requesting a
 * connection
 */
struct ByRequest
{};

} // namespace internal

namespace
{
inline std::int64_t to_ms(auto dur)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
}
} // namespace

/// @brief 1 heartbeat_cycle = 5000 ms
using heartbeat_cycle = std::chrono::duration<int, std::ratio<5000, 1000>>;

/// @brief User-defined literal for creating heartbeat cycles.
constexpr heartbeat_cycle operator"" _hb(unsigned long long val)
{
    return heartbeat_cycle{static_cast<int>(val)};
}

/** \brief The manager Skywing instance used for communication
 */
class Manager
{
public:
    friend class ManagerHandle;
    /** \brief Creates a Manager instance that listens on the specified
     * port for connections.
     *
     * \param port The port to listen on
     * \param id The ID to assign to this machine
     * \param heartbeat_interval The interval to wait between heartbeats,
     * default = 5000 ms \param neighbor_timeout_factor Heartbeat cycles to wait
     * before marking a neighbor as intentionally disconnected.
     */
    Manager(const std::uint16_t port,
            const MachineID& id,
            const heartbeat_cycle heartbeat_interval = 1_hb,
            const std::size_t neighbor_timeout_factor = 3) noexcept;

    Manager(std::string const& local_addr,
            const MachineID& id,
            const heartbeat_cycle heartbeat_interval = 1_hb,
            const std::size_t neighbor_timeout_factor = 3) noexcept;

    explicit Manager(SocketAddr const& addr,
                     const MachineID& id,
                     const heartbeat_cycle heartbeat_interval = 1_hb,
                     const std::size_t neighbor_timeout_factor = 3) noexcept;

    /**
     * \brief Utility ctor for general durations to keep
     * backward compatibilty with regression tests
     */
    template <typename Rep, typename Period>
    Manager(std::uint16_t port,
            const MachineID& id,
            const std::chrono::duration<Rep, Period> heartbeat_interval,
            std::size_t neighbor_timeout_factor = 3) noexcept
        : Manager{port,
                  id,
                  std::chrono::duration_cast<heartbeat_cycle>(
                      std::max(heartbeat_interval,
                               std::chrono::duration<Rep, Period>{1_hb})),
                  neighbor_timeout_factor}
    {}

    // /** \brief Constructor for building from a file format specified in
    //  * basic_manager_config.hpp
    //  *
    //  * This will block until all of the specified connections have been made
    //  */
    // Manager(const BuildManagerInfo& info) noexcept;

    /** \brief Destructor; tells all neighbors that the device is dead
     */
    ~Manager();

    /** \brief Configure the initial neighbor connections to be made
     *
     *  This sets multiple IP address / port number pairs to define the desired
     *  initial handshake connections between agents. This function may be
     *  called multiple times to configure multiple lists of address / port
     *  pairs.
     */
    void configure_initial_neighbors(
        std::vector<SocketAddr> const& neighbor_address_port_pairs,
        std::chrono::seconds timeout = std::chrono::seconds(10)) noexcept;

    // FIXME (trb): DEPRECATE!
    void configure_initial_neighbors(
        std::vector<std::tuple<std::string, std::uint16_t>> const&
            neighbor_address_port_pairs,
        std::chrono::seconds timeout = std::chrono::seconds(10)) noexcept;

    /** \brief Configure the initial neighbor connections to be made
     *
     *  This sets a IP address / port number pair to define the desired
     *  initial handshake connections between agents. This function may be
     *  called multiple times to configure multiple address / port pairs.
     */
    void configure_initial_neighbors(
        SocketAddr const& address,
        std::chrono::seconds timeout = std::chrono::seconds(10)) noexcept;

    // FIXME (trb): DEPRECATE!
    void configure_initial_neighbors(
        std::string address,
        std::uint16_t const port,
        std::chrono::seconds timeout = std::chrono::seconds(10)) noexcept
    {
        return configure_initial_neighbors(SocketAddr{std::move(address), port},
                                           timeout);
    }

    /** \brief Initiate the initial connections to neighbors
     *
     *  This is used internally within the Job.run() function to actually
     *  make the initial neighbor connections (because this
     *  must be done asynchronously) and is not meant to be
     *  called by an application.
     */
    void make_neighbor_connection() noexcept;

    /** \brief Creates a job for the manager to execute that produces the
     * specified tags.
     *
     * Returns false if the job could not be inserted (only happens on name
     * collision)
     */
    bool submit_job(JobID name,
                    std::function<void(Job&, ManagerHandle)> to_run) noexcept;

    /** \brief Start running all submitted jobs
     */
    void run() noexcept;

    /** \brief Gets the id of the manager
     */
    const std::string& id() const noexcept;

    /** \brief Subscribes to the passed tags only on a specific IP
     */
    Waiter<bool> ip_subscribe(const SocketAddr& addr,
                              const std::vector<TagID>& tag_ids) noexcept;

    /** \brief Subscribes to the passed tags.
     */
    Waiter<void> subscribe(const std::vector<TagID>& tag_ids) noexcept;

    // Auxillary function to help with subscribe function
    bool
    subscribe_is_done(const std::vector<TagID>& required_tags) const noexcept;

    /** \brief Returns true if the connection to the specified address is
     * complete
     */
    bool conn_is_complete(const SocketAddr& address) noexcept;

    /** \brief Returns true if the connection was successful, false otherwise
     *
     * More accurately, checks if an address is currently connected, which may
     * be useful to expose at some point?
     */
    bool addr_is_connected(const SocketAddr& address) const noexcept;

    /** \brief Handles the get_publishers message
     */
    void handle_get_publishers(const internal::GetPublishers& msg,
                               internal::NeighborAgent& from) noexcept;

    /** \brief Adds the publishers and propagate the information is required
     *
     * Returns a bool indicating if the next request for publishers should
     * ignore the cache
     */
    void
    add_publishers_and_propagate(const internal::ReportPublishers& msg,
                                 const internal::NeighborAgent& from) noexcept;

    /** \brief Returns true if the subscription tags are all produced
     */
    bool subscription_tags_are_produced(
        const internal::SubscriptionNotice& msg) const noexcept;

    /** \brief Handles published information
     */
    bool handle_publish_data(const internal::PublishData& msg,
                             const internal::NeighborAgent& from) noexcept;

    /** Notifications raised so that condition variables can
     * use notifications while the mutex is released.
     */
    void notify_subscriptions() noexcept { notify_subscriptions_ = true; }

    /** \brief Broadcast a message to the entire network
     *
     * \param version The message's version
     * \param tag_id The id of the tag the message is for
     * \param value The value to send
     */
    void publish(const VersionID version,
                 const TagID& tag_id,
                 std::span<PublishValueVariant> value) noexcept;

    /** \brief Reports when new tags are being produced
     */
    void report_new_publish_tags(const std::vector<TagID>& tags) noexcept;

    bool request_disconnect(const MachineID& neighbor_agent_id) noexcept;

    bool request_reconnect(const MachineID& neighbor_agent_id) noexcept;

    bool request_reconnect_with_retry(const MachineID& neighbor_id,
                                      int max_retries = 3) noexcept;

    /** \brief Returns a vector of all the neighboring ID's
     */
    std::vector<MachineID> make_neighbor_vector() const noexcept;

    size_t number_of_neighbors() const noexcept;

private:
    ///////////////////////////////////////
    // Interface for ManagerHandle
    ///////////////////////////////////////

    Waiter<bool> connect_to_server(SocketAddr const&) noexcept;
    size_t number_of_subscribers(const AbstractTag& tag) const noexcept;
    std::uint16_t port() const noexcept;

    Waiter<void> waiter_on_subscription_change(
        std::function<bool()> is_ready_callable) noexcept
    {
        return make_waiter(
            dummy_mutex_, subscription_cv_, std::move(is_ready_callable));
    }

    template <typename T>
    Waiter<T>
    waiter_on_subscription_change(std::function<bool()> is_ready_callable,
                                  std::function<T()> get_val_callable) noexcept
    {
        return Waiter<T>(dummy_mutex_,
                         subscription_cv_,
                         std::move(is_ready_callable),
                         std::move(get_val_callable));
    }

    ///////////////////////////////////////
    // End Interface for ManagerHandle
    ///////////////////////////////////////

    /** \brief See if there are any pending connections and accept them if so
     */
    void accept_pending_connections() noexcept;

    /** \brief Listens for messages from neighbors and handles them if there
     * are any.
     */
    void handle_neighbor_messages() noexcept;

    // Adds data to the tag queue for a job from a message
    // Returns true if it was successful, false if something went wrong
    bool add_data_to_queue(const internal::PublishData& msg) noexcept;

    /** \brief Notify neighbors of a new new neighbor
     */
    void notify_of_new_neighbor(const MachineID& id) noexcept;

    /** \brief Removes all dead neighbors
     */
    void remove_dead_neighbors() noexcept;

    /** @brief Cleans up local neighbor related state */
    void cleanup_neighbor_state(const MachineID& id,
                                internal::NeighborAgent& neighbor,
                                bool& new_tags) noexcept;

    /** @brief Checks if a neighbor has timed out based on time last heard
     */
    void check_neighbor_timeout(internal::NeighborAgent& neighbor) noexcept;

    /** \brief Broadcasts a message to all neighbors that fit a criteria
     */
    template <typename Callable>
    void send_to_neighbors_if(const std::vector<std::byte>& to_send,
                              Callable condition) noexcept;

    /** \brief Broadcasts a message to all neighbors
     */
    void send_to_neighbors(const std::vector<std::byte>& to_send) noexcept;

    /** \brief Removes any tags that have enough publishers, returning the tags
     * that remain and the number of publishers that they need
     */
    auto remove_tags_with_enough_publishers(
        const internal::GetPublishers& msg) noexcept
        -> std::pair<std::vector<TagID>, std::vector<std::uint8_t>>;

    /** \brief Produce a message containing the known publishers and tags
     */
    std::vector<std::byte> make_known_tag_publisher_message() const noexcept;

    /** \brief Attempt to create connections for any pending tags.
     */
    void init_connections_for_pending_tags() noexcept;

    /** \brief Process pending user requested connections
     */
    void process_pending_conns() noexcept;

    /** \brief Creates the message for the initial handshake
     */
    std::vector<std::byte> make_handshake() const noexcept;

    /** \brief Creates the message for reconnecting two agents.
     */
    std::vector<std::byte> make_reconnect() const noexcept;

    /** \brief Finalizes a subscription connection.
     *
     * \param tags '\0' seperated list of tags
     */
    void finalize_subscription(const std::string& tags,
                               internal::NeighborAgent& source) noexcept;

    /** \brief Asks neighbors for publishers for pending tags with no know
     * publishers
     */
    void find_publishers_for_pending_tags(bool force_ask = false) noexcept;

    /** \brief Returns all locally produced tags as a vector
     */
    std::vector<TagID> local_tags() const noexcept;

    // void process_neighbor_changes() noexcept;

    // void queue_remove_neighbor(const std::string& ip_address, const
    // std::uint16_t port) noexcept;

    // For listening to connection requests
    internal::SocketListener server_socket_;

    // List of the jobs that are present
    std::unordered_map<JobID, Job> jobs_;

    // List of neighboring connections
    std::unordered_map<MachineID, internal::NeighborAgent> neighbors_;

    // List of publishers that are known for each tag
    std::unordered_map<TagID, std::unordered_set<internal::PublisherInfo>>
        publishers_for_tag_;

    // A list of tags that still need to have publishers found
    std::vector<std::string> pending_tags_;

    // The id of this machine
    MachineID id_;

    // The time to send a heartbeat if nothing has been heard in the time
    heartbeat_cycle heartbeat_interval_;

    /// @brief Heartbeat cycles to wait before marking a neighbor as
    /// intentionally disconnected.
    heartbeat_cycle neighbor_timeout_threshold_;

    // Only allow one job access to the manager at a time
    mutable std::mutex job_mut_;

    // Dummy mutex - only used for custom waiters created by users
    mutable std::mutex dummy_mutex_;

    // List of machines that are waiting for information for producers of a
    // certain tag Uses MachineID's instead of pointers in case the remote
    // machine disconnects and the NeighborAgent is deleted between the time a
    // request is started and a response is received
    // TODO: Maybe move to pointers and just make sure to remove them when the
    // neighbor is removed? Also potentially combine with tag_to_machine_ since
    // they are tags into the same thing
    std::unordered_map<TagID, std::unordered_set<MachineID>>
        send_publisher_information_to_;

    // The tags that this machine produces and the self-subscription count
    std::unordered_map<TagID, int> self_sub_count_;

    // The port used for communications
    std::uint16_t port_;

    // Mapping from a machine address to a pointer to the neighbor agent
    // This is also used for testing that a connection has completed
    std::unordered_map<SocketAddr, internal::NeighborAgent*> addr_to_machine_;

    // Cache ip and port for disconnected agents for reconnection later
    std::unordered_map<MachineID, SocketAddr> reconnect_cache;

    // Mapping from a tag to the ID used for the subscription to the tag
    // Used to know when a subscription is done and for if multiple jobs
    // subscribe to the same tag
    // This is also use to mark when a pending connection is for a tag
    std::unordered_map<TagID, internal::NeighborAgent*> tag_to_machine_;

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
        reconnect,
        specific_ip
    };
    static const char* to_c_str(ConnType type) noexcept;
    // Pending connections for all types
    struct PendingInfo
    {
        inline static std::atomic<std::uint64_t> next_id{0};
        std::uint64_t id;
        internal::SocketCommunicator conn;
        ConnStatus status;
        ConnType type;
        std::string tag{};

        PendingInfo(internal::SocketCommunicator&& c,
                    ConnStatus s,
                    ConnType t,
                    std::string tag = {})
            : id(next_id.fetch_add(1, std::memory_order_relaxed)),
              conn(std::move(c)),
              status(s),
              type(t),
              tag(std::move(tag))
        {}

        PendingInfo(const PendingInfo&) = delete;
        PendingInfo& operator=(const PendingInfo&) = delete;
        PendingInfo(PendingInfo&&) = default;
        PendingInfo& operator=(PendingInfo&&) = default;
    };
    std::unordered_map<SocketAddr, PendingInfo> pending_conns_;

    // Notification for when new subscriptions are created
    std::condition_variable subscription_cv_;

    // Notification for when connections are complete
    std::condition_variable connection_cv_;

    // Booleans separate for if notifications should be raised so that
    // the CV's can use notifications while the mutex is released
    bool notify_subscriptions_ = false;
    bool notify_connection_ = false;

    // Requested initial neighbor address and port pairs for connections
    mutable std::vector<SocketAddr> initial_neighbor_address_port_pairs_;

    // Maximum time to wait for initial neighbor connection to be established
    mutable std::chrono::seconds initial_neighbor_connection_timeout_;

    std::unique_ptr<internal::MessageHandler> message_handler_;

    std::unordered_set<SocketAddr> reconnecting_addrs_;

}; // class Manager

class ManagerHandle
{
public:
    /** \brief Connects to another instance at the specified address on
     * the specified port
     *
     * \param address The address to connect to
     * \param port The port to connect on
     */
    Waiter<bool> connect_to_server(SocketAddr const& address) noexcept
    {
        return handle_->connect_to_server(address);
    }

    Waiter<bool> connect_to_server(std::string address,
                                   std::uint16_t const port) noexcept
    {
        return connect_to_server(SocketAddr{std::move(address), port});
    }

    /** \brief Returns the number of machines connected
     */
    int number_of_neighbors() const noexcept
    {
        return handle_->number_of_neighbors();
    }

    bool request_disconnect(const MachineID& neighbor_agent_id) noexcept
    {
        return handle_->request_disconnect(neighbor_agent_id);
    }

    bool request_reconnect(const MachineID& neighbor_agent_id) noexcept
    {
        return handle_->request_reconnect(neighbor_agent_id);
    }

    bool request_reconnect_with_retry(const MachineID& neighbor_agent_id,
                                      int max_retries = 3) noexcept
    {
        return handle_->request_reconnect_with_retry(neighbor_agent_id,
                                                     max_retries);
    }

    /** \brief Returns the id of the manager
     */
    const std::string& id() const noexcept { return handle_->id(); }

    /** \brief Returns the number of subscribers that a tag has
     */
    int number_of_subscribers(const AbstractTag& tag) const noexcept
    {
        return handle_->number_of_subscribers(tag);
    }

    /** \brief Creates a waiter that has a done condition that is run anytime
     * anything with subscriptions happens
     */
    Waiter<void> waiter_on_subscription_change(
        std::function<bool()> is_ready_callable) noexcept
    {
        return handle_->waiter_on_subscription_change(
            std::move(is_ready_callable));
    }

    template <typename T>
    Waiter<T>
    waiter_on_subscription_change(std::function<bool()> is_ready_callable,
                                  std::function<T()> get_val_callable) noexcept
    {
        return handle_->waiter_on_subscription_change(
            std::move(is_ready_callable), std::move(get_val_callable));
    }

    /** \brief Returns the port the manager is listening on
     */
    std::uint16_t port() const noexcept { return handle_->port(); }

private:
    friend class Job;

    // Private so that only jobs can create a handle
    explicit ManagerHandle(Manager& m) noexcept : handle_{&m} {}

    Manager* handle_;
}; // class ManagerHandle
} // namespace skywing

#endif // SKYWING_MANAGER_HPP

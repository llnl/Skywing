#ifndef SKYWING_NEIGHBOR_AGENT_HPP
#define SKYWING_NEIGHBOR_AGENT_HPP

#include <atomic>
#include <chrono>
#include <unordered_set>
#include <vector>

#include "skywing_core/internal/capn_proto_wrapper.hpp"
#include "skywing_core/internal/devices/socket_communicator.hpp"
#include "skywing_core/types.hpp"

namespace skywing
{
class Manager;

namespace internal
{
/** \brief Represents our knowledge of another agent (a neighbor) in
 *  the Skywing collective.
 */
class NeighborAgent
{
public:
    /** \brief Construct a new NeighborAgent
     *
     * \param comm A SocketCommunicator used to talk to the neighbor.
     * \param id The MachineID of the neighbor.
     * \param neigbhors Our knowledge of this neighbor's neighbors.
     * \param Manager A reference to the Manager that builds (and owns) this
     * NeighborAgent object.
     * \param addr The address of this neighbor's server.
     */
    NeighborAgent(SocketCommunicator comm,
                  const MachineID& id,
                  const std::vector<MachineID>& neighbors,
                  Manager& manager,
                  SocketAddr const& addr,
                  std::string hi) noexcept;

    /** \brief Returns the id of the computer this is connected to
     */
    MachineID id() const noexcept;

    /** \brief The address for communication with the neighboring agent
     */
    std::string address() const noexcept;

    /** \brief Pair version of the address
     */
    SocketAddr address_pair() const noexcept;

    /** \brief Returns if the connection is believed dead or not
     */
    bool is_dead() const noexcept;

    /** \brief Marks the connection as dead
     */
    void mark_as_dead() noexcept;

    /** \brief Returns true if the NeighborAgent is known to have the given neighbor.
     */
    bool has_neighbor(const MachineID& id) const noexcept;

    /** \brief Add a new neighbor to this NeighborAgent.
     *
     * Returns true if the neighbor was successfully added, false
     * otherwise (eg if the neighbor was already present).
     */
    bool add_new_neighbor(const MachineID& new_id);

    /** \brief Remove a neighbor from this NeighborAgent.
    *
    *  Returns true if successfully removed, false otherwise (eg if it
    *  already wasn't there).
    */
    bool remove_neighbor(const MachineID& id);

    /** \brief Returns true if the neighbor is subscribed to the tag,
     * returns false if it is not.
     */
    bool is_subscribed_to(const TagID& tag) const noexcept;

    /** \brief Adds a new tag to the list of tags to which this
     * neighbor is subscribed.
     *
     * Returns true if the tag was added (ie the tag wasn't already
     * present).
     */
    bool add_new_subscription(TagID tag);

    /** \brief Returns true if we are awaiting a response to an
     *  outstanding request for publisher information.
     */
    bool has_outstanding_publishers_request() const noexcept
    { return has_outstanding_publishers_request_; }

    /** \brief Set if we are awaiting a response to an outstanding
     *   request for publisher information.
     */
    void set_has_outstanding_publishers_request(bool has_outst_req) noexcept
    {
        has_outstanding_publishers_request_ = has_outst_req;
    }

    /** Return the time point when this neighbor agent was last heard
    from.
     */
    std::chrono::steady_clock::time_point time_last_heard() const noexcept
    {
        return last_heard_.load();
    }

    /** Note that we just heard from this device.
     */
    void heard_from() { last_heard_.store(std::chrono::steady_clock::now()); }

    /** \brief Returns a reference to the known neighbors of this neighbor.
     */
    const std::vector<MachineID>& neighbors() { return neighbors_; }

    void add_communicator(SocketCommunicator&& comm)
    {
        comms_.push_back(std::move(comm));
    }

    /** \brief Returns a reference to the set of SocketCommunicators to
        communicate with this neighbor.
     */
    std::vector<SocketCommunicator>& get_comms() { return comms_; }

    /** \brief Update the time point when we should next ask for
     *   publisher information from this neighbor.
     *
     * \param urgent True if we urgently need a response.
     */
    void update_time_for_next_request(bool urgent);

    /** \brief Returns true if it is time to make another publishers
        request.
     */
    bool is_time_for_another_request()
    {
        return std::chrono::steady_clock::now() > request_tags_time_;
    }

    class AtomicTime
    {
    public:
        AtomicTime() = default;

        explicit AtomicTime(std::chrono::steady_clock::time_point t)
        {
            store(t);
        }

        void store(std::chrono::steady_clock::time_point t) noexcept
        {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          t.time_since_epoch())
                          .count();
            timestamp_ms_.store(ms, std::memory_order_relaxed);
        }

        std::chrono::steady_clock::time_point load() const noexcept
        {
            auto ms = timestamp_ms_.load(std::memory_order_relaxed);
            return std::chrono::steady_clock::time_point(
                std::chrono::milliseconds(ms));
        }

    private:
        std::atomic<int64_t> timestamp_ms_{0};
    };

private:
  /** \brief Calculate and set the next time tags should be requested
   */
    void calc_next_request_time() noexcept;

    // For talking with the neighbor.
    // You'd think there would only be one SocketCommunicator for
    // talking to another agent, so why the vector? It's because
    // sometimes agents initiate connections with each other
    // simulataneously, creating multiple socket connections between the
    // same pair of agents. Deciding which one to drop would require an
    // entire agreement protocol, which isn't worth it, so just hang on
    // to both.
    std::vector<SocketCommunicator> comms_;

    // The id of the neighbor agent
    MachineID id_;

    // The last time the machine was heard from
    AtomicTime last_heard_;

    // Vector representing this `neighbor_agent`'s neighbors. Not to
    // be confused with `Manager::neighbors_`.
    std::vector<MachineID> neighbors_;

    // The owning manager
    Manager* manager_;

    // The time that will be waited until requesting tags again
    std::chrono::steady_clock::time_point request_tags_time_;

    // Tags that the remote is subscribed for
    // std::unordered_set for fast look-up
    std::unordered_set<TagID> remote_subscriptions_;

    // The address to use to connect to the remote machine
    SocketAddr addr_;

    // The number of times requests have been unfulfilled
    std::uint8_t backoff_counter_ = 0;

    // If the next request for tags should ignore the cache or not
    bool must_find_more_publishers_ = false;

    // If the connection is dead or not
    bool dead_ = false;

    // If there we are waiting on a publishers report (ie a tag
    // request) from this neighbor agent
    bool has_outstanding_publishers_request_ = false;
}; // class NeighborAgent

} // namespace internal

} // namespace skywing

#endif // SKYWING_NEIGHBOR_AGENT_HPP

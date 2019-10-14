#include "skynet/master.hpp"

// TODO: Support other types of communicators; will probably make
//       it a template and have it as a parameter, so not making a seperate
//       .cpp file even though there currently could be one.

namespace skynet
{
  namespace internal
  {
    /** \brief Attempt to construct an ExternalMaster using an existing connection
     *
     * This is for when a server accepts a new connection.  Both have to
     * send/recieve greetings, but need to do so in the opposite order so
     * have seperate constructors for both.
     */
    std::optional<ExternalMaster> ExternalMaster::create(
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
    std::optional<ExternalMaster> ExternalMaster::create(
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
    std::optional<MessageHandler> ExternalMaster::get_message() noexcept
    {
      if (dead_)
      {
        return {};
      }
      if (auto handler = try_to_get_message_handler())
      {
        // Handle status messages here
        if (handler->category() == MessageCategory::status)
        {
          handle_message(*handler);
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
    void ExternalMaster::send_message(const std::vector<std::byte>& c) noexcept
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
    MachineID ExternalMaster::id() const noexcept { return id_; }

    /** \brief Returns if the connection is dead or not
     */
    bool ExternalMaster::is_dead() const noexcept { return dead_; }

    /** \brief Marks the connection as dead
     */
    void ExternalMaster::mark_as_dead() noexcept { dead_ = true; }

    /** \brief Returns true if the given neighbor is present, false otherwise
     */
    bool ExternalMaster::has_neighbor(const MachineID id) const noexcept
    {
      const auto loc = std::lower_bound(neighbors_.cbegin(), neighbors_.cend(), id);
      return loc != neighbors_.cend() && *loc == id;
    }

    // Only allow private construction
    ExternalMaster::ExternalMaster(SocketCommunicator conn) noexcept
      : conn_{std::move(conn)}
    {}

    // Read some bytes from the connection, returning false if the read failed
    bool ExternalMaster::read_from_conn(std::byte* const buffer, const std::size_t count) noexcept
    {
      const auto err = conn_.read_message(buffer, count);
      switch (err)
      {
      case ConnectionError::no_error: break;
      case ConnectionError::would_block: return false;

      case ConnectionError::closed:
        // [[fallthrough]];
      case ConnectionError::unrecoverable:
        dead_ = true;
        return false;
      }
      return true;
    }

    // Attempt to get a MessageHandler from the connection
    std::optional<MessageHandler> ExternalMaster::try_to_get_message_handler() noexcept
    {
      std::array<std::byte, sizeof(NetworkSizeType)> size_buffer;
      if (read_from_conn(size_buffer.data(), size_buffer.size()))
      {
        const auto bytes_to_read = from_network_bytes(size_buffer);
        // Then read the actual message and parse it
        std::vector<std::byte> message_buffer(bytes_to_read);
        if (read_from_conn(message_buffer.data(), message_buffer.size()))
        {
          return MessageHandler{message_buffer};
        }
        else
        {
          // Couldn't read the size bytes - bad message
          dead_ = true;
          return {};
        }
      }
      return {};
    }

    // Send the greeting
    bool ExternalMaster::send_greeting(const MachineID local_id, const std::vector<MachineID>& local_neighbors) noexcept
    {
      send_message(make_greeting(local_id, local_neighbors));
      return !dead_;
    }

    // Wait until the greeting is sent
    bool ExternalMaster::wait_for_greeting() noexcept
    {
      // Wait for the greeting
      // TODO: Probably want a time-out?
      while (true)
      {
        if (auto handle = try_to_get_message_handler())
        {
          if (handle->category() != MessageCategory::status)
          {
            return false;
          }
          return handle->do_callback(
            [&](const Greeting& msg) {
              neighbors_ = msg.neighbors();
              id_ = msg.from();
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
    void ExternalMaster::handle_message(MessageHandler& handle) noexcept
    {
      assert(handle.category() == MessageCategory::status);
      const auto okay = handle.do_callback(
        [&](const Greeting&) {
          // shouldn't be seeing a greeting here
          return false;
        },
        [&](const Goodbye&) {
          dead_ = true;
          return true;
        },
        [&](const NewNeighbor& msg) {
          const auto loc = std::lower_bound(neighbors_.cbegin(), neighbors_.cend(), msg.neighbor_id());
          // Already present -> connection is bad
          if (loc != neighbors_.cend() && *loc == msg.neighbor_id())
          {
            return false;
          }
          // Otherwise just insert it
          neighbors_.insert(loc, msg.neighbor_id());
          return true;
        },
        [&](const RemoveNeighbor& msg) {
          const auto loc = std::lower_bound(neighbors_.begin(), neighbors_.end(), msg.neighbor_id());
          // Neighbor is lying; can't trust it anymore
          if (loc == neighbors_.end() || *loc != msg.neighbor_id())
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
  } // namespace internal

  ////////////////////////////////////////////////
  // Class Master
  ////////////////////////////////////////////////
  void Master::Accessor::add_job(
    Master& m,
    const JobID& id,
    Job& to_add
  ) noexcept
  {
    m.jobs_.emplace(id, &to_add);
  }

  void Master::Accessor::broadcast(
    Master& m,
    const MessageID msg_id,
    const TagID& tag_id,
    const std::uint32_t hops_left_p1,
    BroadcastDataVariant data
  ) noexcept
  {
    m.do_broadcast(msg_id, tag_id, hops_left_p1, std::move(data));
  }

  /** \brief Creates a Master instance that listens on the specified
   * port for connections.
   *
   * \param port The port to listen on
   * \param id The ID to assign to this machine
   */
  Master::Master(const std::uint16_t port, const MachineID& id) noexcept
    : id_{id}
  {
    server_socket_.set_to_listen(port);
  }

  /** \brief Destructor; tells all neighbors that the device is dead
   */
  Master::~Master()
  {
    send_to_neighbors(internal::make_goodbye());
  }

  /** \brief Connects to another instance at the specified address on
   * the specified port
   *
   * \param address The address to connect to
   * \param port The port to connect on
   * \return True if the connection was successful, false if it failed
   */
  bool Master::connect_to_server(const char* const address, const std::uint16_t port) noexcept
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
        return false;
      }
      notify_of_new_neighbor(new_id);
      neighbors_.emplace(new_id, std::move(*new_neighbor));
    }
    return true;
  }

  /** \brief See if there are any pending connections and accept them if so
   */
  void Master::accept_pending_connections() noexcept
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

  /** \brief Returns the number of machines connected
   */
  int Master::number_of_neighbors() const noexcept
  {
    return static_cast<int>(neighbors_.size());
  }

  /** \brief Start running all submitted jobs
   */
  void Master::run() noexcept
  {
    std::vector<std::thread> threads;
    threads.reserve(jobs_.size());
    for (auto& [name, job] : jobs_)
    {
      (void)name;
      threads.push_back(Job::Accessor::run(*job));
    }
    // Do processing while there are still jobs
    while (!jobs_.empty()) {
      // Remove any finished jobs
      for (auto iter = jobs_.cbegin(); iter != jobs_.cend(); ) {
        if (iter->second->is_finished()) {
          iter = jobs_.erase(iter);
        }
        else {
          ++iter;
        }
      }
      // Handle any messages from neighbors
      handle_neighbor_messages();
      // Wait a bit for other messages
      std::this_thread::yield();
    }
    // Join all of the threads now
    for (auto& thread : threads)
    {
      thread.join();
    }
  }

  /** \brief Listens for messages from neighbors and handles them if there
   * are any.
   */
  void Master::handle_neighbor_messages() noexcept
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
   * \param tag_id The id of the tag the message is for
   * \param hops_p1 The number of hops left + 1
   * \param data The data to broadcast
   */
  void Master::do_broadcast(
    const MessageID msg_id,
    const TagID& tag_id,
    const std::uint8_t hops_left_p1,
    BroadcastDataVariant data
  ) noexcept
  {
    // Prepend the message describing the data and send it to all neighbors
    send_to_neighbors(internal::make_broadcast(msg_id, tag_id, id_, hops_left_p1, std::move(data)));
  }

  // Does all processing that needs to be done when a message is recieved
  void Master::process_message(internal::MessageHandler& handle, internal::ExternalMaster& from) noexcept
  {
    assert(handle.category() == internal::MessageCategory::job);
    const auto okay = handle.do_callback(
      [&](const internal::Broadcast& msg) {
        if (is_old_message(msg))
        {
          return true;
        }
        if (!add_data_to_queue(msg))
        {
          return false;
        }
        if (msg.hops_left_p1() != 1)
        {
          const auto hops_p1 = msg.hops_left_p1();
          const auto to_send = internal::make_broadcast(
            msg.message_id(),
            msg.tag_id(),
            msg.origin(),
            hops_p1 == 0 ? 0 : hops_p1 - 1,
            *msg.data().get_variant()
          );
          // Propagate the message to all neighbors but ones that are known to
          // have already recieve it, so not the sender, the origin, or neighbors
          // that the sender also has
          send_to_neighbors_if(to_send, [&](const internal::ExternalMaster& m) {
            return
              m.id() != from.id() &&
              m.id() != msg.origin() &&
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
  bool Master::add_data_to_queue(const internal::Broadcast& msg) noexcept
  {
    for (auto& [name, job] : jobs_)
    {
      (void)name;
      if (!Job::Accessor::process_data(*job, msg.tag_id(), *msg.data().get_variant()))
      {
        return false;
      }
    }
    return true;
  }

  /** \brief Returns true if a message is old, false otherwise
   */
  bool Master::is_old_message(const internal::Broadcast& msg) noexcept
  {
    auto& last_id = last_message_id_[msg.origin()][msg.tag_id()];
    if (msg.message_id() <= last_id)
    {
      return true;
    }
    last_id = msg.message_id();
    return false;
  }

  /** \brief Notify neighbors of a new new neighbor
   */
  void Master::notify_of_new_neighbor(const MachineID id) noexcept
  {
    send_to_neighbors(internal::make_new_neighbor(id));
  }

  /** \brief Removes all dead neighbors
   */
  void Master::remove_dead_neighbors() noexcept
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
  std::vector<MachineID> Master::make_neighbor_vector() const noexcept
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

  /** \brief Broadcasts a message to all neighbors
   */
  void Master::send_to_neighbors(const std::vector<std::byte>& to_send) noexcept
  {
    send_to_neighbors_if(to_send, [](const internal::ExternalMaster&) { return true; });
  }
} // namespace skynet

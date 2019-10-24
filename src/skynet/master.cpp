#include "skynet/master.hpp"

// TODO: Support other types of communicators

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
      const std::vector<MachineID>& local_neighbors,
      Master& master
    ) noexcept
    {
      ExternalMaster to_ret(std::move(conn));
      to_ret.master_ = &master;
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
      const std::vector<MachineID>& local_neighbors,
      Master& master
    ) noexcept
    {
      ExternalMaster to_ret(std::move(conn));
      to_ret.master_ = &master;
      return init_conn(
        to_ret,
        [&]() { return to_ret.wait_for_greeting(); },
        [&]() { return to_ret.send_greeting(local_id, local_neighbors); }
      );
    }

    void ExternalMaster::handle_messages() noexcept
    {
      if (dead_)
      {
        return;
      }
      while (auto handler = try_to_get_status_message())
      {
        // Update the last time something was heard
        last_heard_ = std::chrono::steady_clock::now();
        // Handle the message
        handle_message(*handler);
      }
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

    /** \brief Sends a heartbeat if enough time has passed
     */
    void ExternalMaster::send_heartbeat_if_past_interval(std::chrono::milliseconds interval) noexcept
    {
      using namespace std::chrono;
      const auto time_expired = steady_clock::now() - last_heard_;
      if (time_expired >= interval)
      {
        // Try to send a message
        send_message(make_heartbeat());
        // This count as hearing from the device
        last_heard_ = steady_clock::now();
      }
    }

    void ExternalMaster::find_producers_for_tags(const std::vector<TagID>& tags) noexcept
    {
      bool work_to_do = false;
      for (const auto& tag_id : tags)
      {
        // The tag is not pending, mark this as not needing to be propagated
        // and ask the neighbor for details
        if (pending_tags_propagate_back_.find(tag_id) == pending_tags_propagate_back_.cend())
        {
          pending_tags_propagate_back_.try_emplace(tag_id, false);
          work_to_do = true;
        }
      }
      if (work_to_do)
      {
        send_message(make_get_publishers(tags));
      }
    }

    ExternalMaster::ExternalMaster(SocketCommunicator conn) noexcept
      : conn_{std::move(conn)}
      , last_heard_{std::chrono::steady_clock::now()}
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

    std::vector<std::byte> ExternalMaster::read_from_conn(const std::size_t count) noexcept
    {
      // Size of memory to allocate/read each step
      constexpr std::size_t read_step_size     = 0x0'1000;
      constexpr std::size_t allocate_step_size = read_step_size * 16;
      // How often memory needs to be resized
      constexpr std::size_t resize_every_n_steps = allocate_step_size / read_step_size;
      // Ensure that the allocate size is evenly divisible by the read size
      static_assert(allocate_step_size % read_step_size == 0);
      static_assert(allocate_step_size >= read_step_size);
      // To prevent overallocation of memory, don't allocate a ton of memory to start
      std::vector<std::byte> read_bytes;
      // The final bytes to read in the end
      const int final_read_size = count % read_step_size;
      // Read memory in 4KiB chunks
      const int num_iters = count / read_step_size + (final_read_size == 0 ? 0 : 1);
      for (int i = 0; i < num_iters; ++i)
      {
        if (i % resize_every_n_steps == 0)
        {
          // Allocate more memory
          const std::size_t mem_left_to_read = count - read_bytes.size();
          const std::size_t additional_size =
            mem_left_to_read > allocate_step_size
              ? allocate_step_size
              : mem_left_to_read;
          read_bytes.resize(read_bytes.size() + additional_size);
        }
        const std::size_t num_bytes_to_read = (i == num_iters - 1 ? final_read_size : read_step_size);
        // Allocate more memory if needed
        if (!read_from_conn(&read_bytes[i * read_step_size], num_bytes_to_read))
        {
          return {};
        }
      }
      return read_bytes;
    }

    std::optional<StatusMessageHandler> ExternalMaster::try_to_get_status_message() noexcept
    {
      std::array<std::byte, sizeof(NetworkSizeType)> size_buffer;
      if (read_from_conn(size_buffer.data(), size_buffer.size()))
      {
        const auto bytes_to_read = from_network_bytes(size_buffer);
        // Then read the actual message and parse it
        if (const auto message_buffer = read_from_conn(bytes_to_read); !message_buffer.empty())
        {
          return StatusMessageHandler::try_to_create(message_buffer);
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
        if (auto handle = try_to_get_status_message())
        {
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
    void ExternalMaster::handle_message(StatusMessageHandler& handle) noexcept
    {
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
        [](const Heartbeat&) {
          // Nothing to do; this is just to acknowledge it exists
          // (Last heard time was already updated)
          return true;
        },
        [&](const TagPublishers& msg) {
          // // Add this machine's information
          // std::unordered_map<std::string, std::vector<std::string>> producers;
          // auto machines = msg.machines();
          // auto tags = msg.tags();
          // const auto str_view_tags = master_->produced_tags();
          // machines.push_back(master_->address());
          // std::vector<std::string> local_tags;
          // std::copy(
          //   str_view_tags.cbegin(),
          //   str_view_tags.cend(),
          //   local_tags.begin()
          // );
          // tags.push_back(local_tags);
          // // Propagate this back if need be
          // // const auto loc =
          // // Just report it to the master
          // Master::ExternalMasterAccessor::report_tag_publishers(master_, std::move(producers));
          // return true;
        },
        [&](const GetPublishers& msg) {
          Master::ExternalMasterAccessor::handle_get_publishers(*master_, msg, *this);
          return true;
        },
        [](...) {
          // Anything else is a programming bug, this shouldn't be reached
          assert(false && "Missing message type in ExternalMaster::handle_message");
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

  Master::Master(
    const std::uint16_t port,
    const MachineID& id,
    const std::chrono::milliseconds heartbeat_interval
  ) noexcept
    : id_{id}
    , heartbeat_interval_{heartbeat_interval}
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
      make_neighbor_vector()),
      *this
    ) {
      const auto new_id = new_neighbor->id();
      // This ID already exists; drop the connection
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
    while (auto conn = server_socket_.accept())
    {
      if (auto new_neighbor = internal::ExternalMaster::create(
        internal::ByAccept{},
        std::move(*conn),
        id_,
        make_neighbor_vector(),
        *this)
      ) {
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

  bool Master::submit_job(
    JobID name,
    std::vector<TagID> tags_produced,
    std::function<void(Job&)> to_run
  ) noexcept
  {
    // TODO: Mark the tags produced by this job
    const auto res = jobs_.try_emplace(
      std::move(name),
      Job::Accessor::AllowConstruction{},
      *this,
      std::move(tags_produced),
      std::move(to_run)
    );
    return res.second;
  }

  /** \brief Start running all submitted jobs
   */
  void Master::run() noexcept
  {
    using namespace std::chrono_literals;
    std::vector<std::thread> threads;
    threads.reserve(jobs_.size());
    for (auto& [name, job] : jobs_)
    {
      (void)name;
      threads.push_back(Job::Accessor::run(job));
    }
    // Do processing while there are still jobs
    while (!jobs_.empty()) {
      // Remove any finished jobs
      for (auto iter = jobs_.cbegin(); iter != jobs_.cend(); ) {
        if (iter->second.is_finished()) {
          iter = jobs_.erase(iter);
        }
        else {
          ++iter;
        }
      }
      {
        // Ensure there's no data race with jobs
        std::unique_lock lock{job_mut_};
        // Handle any messages from neighbors
        handle_neighbor_messages();
        // Send any heartbeat messages if needed
        for (auto&& neighbor : neighbors_)
        {
          neighbor.second.send_heartbeat_if_past_interval(heartbeat_interval_);
        }
      }
      // Wait a bit for other messages
      std::this_thread::sleep_for(100us);
    }
    // Join all of the threads now
    for (auto& thread : threads)
    {
      thread.join();
    }
  }

  std::vector<std::string_view> Master::produced_tags() const noexcept
  {
    std::vector<std::string_view> to_ret;
    for (const auto& job : jobs_)
    {
      const auto& job_tags = job.second.tags_produced();
      std::transform(
        job_tags.cbegin(),
        job_tags.cend(),
        std::back_inserter(to_ret),
        [](const std::string& str) { return std::string_view{str}; }
      );
    }
    return to_ret;
  }

  std::string Master::address() const noexcept
  {
    return server_socket_.ip_address() + ':' + std::to_string(server_socket_.port());
  }

  /** \brief Listens for messages from neighbors and handles them if there
   * are any.
   */
  void Master::handle_neighbor_messages() noexcept
  {
    for (auto&& neighbor : neighbors_)
    {
      neighbor.second.get_and_handle_messages();
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
    const VersionID version,
    const TagID& tag_id,
    const std::uint8_t hops_left_p1,
    PublishValueVariant data
  ) noexcept
  {
    // Prepend the message describing the data and send it to all neighbors
    send_to_neighbors(internal::make_publish(version, tag_id, id_, hops_left_p1, std::move(data)));
  }

  // Does all processing that needs to be done when a message is recieved
  // void Master::process_message(internal::MessageHandler& handle, internal::ExternalMaster& from) noexcept
  // {
  //   assert(handle.category() == internal::MessageCategory::job);
  //   const auto okay = handle.do_callback(
  //     [&](const internal::Publish& msg) {
  //       if (is_old_message(msg))
  //       {
  //         return true;
  //       }
  //       if (!add_data_to_queue(msg))
  //       {
  //         return false;
  //       }
  //       if (msg.hops_left_p1() != 1)
  //       {
  //         const auto hops_p1 = msg.hops_left_p1();
  //         const auto to_send = internal::make_publish(
  //           msg.version(),
  //           msg.tag_id(),
  //           msg.origin(),
  //           hops_p1 == 0 ? 0 : hops_p1 - 1,
  //           *msg.data().get_variant()
  //         );
  //         // Propagate the message to all neighbors but ones that are known to
  //         // have already recieve it, so not the sender, the origin, or neighbors
  //         // that the sender also has
  //         send_to_neighbors_if(to_send, [&](const internal::ExternalMaster& m) {
  //           return
  //             m.id() != from.id() &&
  //             m.id() != msg.origin() &&
  //             !from.has_neighbor(m.id());
  //         });
  //       }
  //       return true;
  //     },
  //     [](...) {
  //       assert(false && "Invalid message type in Master::process_message");
  //       return false;
  //     }
  //   );
  //   if (!okay)
  //   {
  //     from.mark_as_dead();
  //   }
  // }

  // Adds data to the tag queue for a job from a message
  // Returns true if it was successful, false if something went wrong
  bool Master::add_data_to_queue(const internal::PublishData& msg) noexcept
  {
    for (auto& [name, job] : jobs_)
    {
      (void)name;
      if (!Job::Accessor::process_data(job, msg.tag_id(), *msg.data().get_variant(), msg.version()))
      {
        return false;
      }
    }
    return true;
  }

  /** \brief Returns true if a message is old, false otherwise
   */
  bool Master::is_old_message(const internal::PublishData& msg) noexcept
  {
    // If the message can't be found it's always new
    const auto loc = tag_data_.find(msg.tag_id());
    if (loc == tag_data_.cend())
    {
      tag_data_.try_emplace(msg.tag_id(), TagData{msg.version(), {}, {}});
      return false;
    }
    // Otherwise have to check if it's old
    if (msg.version() <= loc->second.last_version)
    {
      return true;
    }
    loc->second.last_version = msg.version();
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

  bool Master::subscribe(const std::vector<TagID>& tag_ids) noexcept
  {
    std::vector<TagID> tag_to_find;
    for (const auto tag_id : tag_ids)
    {
      // Check if there is a list of known producers for the tag
      auto loc = tag_data_.find(tag_id);
      // Create the entry if required
      if (loc == tag_data_.end())
      {
        loc = tag_data_.try_emplace(
          tag_id,
          TagData{
            Job::tag_default_version,
            {},
            {}
          }
        ).first;
      }
      auto& tag_data = loc->second;
      // If already subscribed then skip then just go to the next tag
      if (tag_data.subscription)
      {
        continue;
      }
      // Now, if there are any known subscriptions, subscribe to them
      if (!tag_data.publishers.empty())
      {
        // TODO: Subscription stuff
      }
      else
      {
        // Couldn't do this subscription; need to look for a producer for the tag
        tag_to_find.push_back(tag_id);
      }
    }
    // Find producers for any tags that need it
    if (!tag_to_find.empty())
    {
      for (auto& neighbor : neighbors_)
      {
        neighbor.find_producers_for_tag(tag_id);
      }
    }
    // Nothing to find - succesfully subscribed
    return tag_to_find.empty();
  }

  void Master::handle_get_publishers(
    const internal::GetPublishers& msg,
    const internal::ExternalMaster& from
  ) noexcept
  {
    // If all of the tag requirements are fulfilled then
    const auto remaining_tags = remove_tags_with_known_publishers(msg);
    if (tags_left.empty())
    {
      // Have to transform the std::string_view tags into std::string ones
      // std::vector<std::string> tags_to_send(local_tags.size());
      // std::copy(
      //   local_tags.cbegin(),
      //   local_tags.cend(),
      //   tags_to_send.begin()
      // );
      if (tags_left.empty())
      {
        // TODO: Send all information about the known tags back
        // send_message(make_tag_publishers(
        //   {master_->address()},
        //   tags_to_send
        // ));
      }
    }
    else
    {
      // TODO: Mark the requesting machine as needing the information and request
      // information from other machines
      // If a request is in-progress for a tag, don't send out any additional
      // requests for that tag
      // Otherwise have to request information from other neighbors
      // Master::ExternalMasterAccessor::send_to_other_neighbors(
      //   *master_,
      //   make_get_publishers(tags_left),
      //   *this
      // );
      // Mark the returned message as needing to be propagated back
    }
  }

  // void Master::add_tag_producers(std::unordered_map<MachineID, std::vector<TagID>> publishers) noexcept
  // {
  //   auto tags = publishers.tags();
  //   auto machines = publishers.machines();
  //   for (auto&& [machine_name, produced_tags] : publishers)
  //   {
  //     for (auto&& tag_id : produced_tags)
  //     {
  //       // Find or create the tag
  //       auto loc = tag_data_.find(tag_id);
  //       if (loc == tag_data_.end())
  //       {
  //         loc = tag_data_.try_emplace(
  //           std::move(tag_id),
  //           TagData{
  //             Job::tag_default_version,
  //             {},
  //             {}
  //           }
  //         ).first;
  //       }
  //       // Add it as a producer
  //       loc->second.producers.push_back(machine_name);
  //     }
  //   }
  // }

  std::vector<TagID> Master::remove_tags_with_known_publishers(const GetPublishers& msg) const noexcept
  {
    auto tags_left = msg.tags();
    tags_left.erase(
      std::remove_if(
        tags_left.begin(),
        tags_left.end(),
        [&](const TagID& id) {
          const auto loc = TagData.find(id);
          return loc == TagData.cend() ? false : !loc.second->publishers.empty();
        }
      ),
      tags_left.end()
    );
    return tags_left;
  }
} // namespace skynet

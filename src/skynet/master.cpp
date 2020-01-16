#include "skynet/master.hpp"

#include "skynet/internal/utility/algorithms.hpp"
#include "skynet/internal/utility/logging.hpp"

// TODO: Support other types of communicators

#include <iomanip>
#include <iostream>
#include <limits>

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
      const MachineID& local_id,
      const std::vector<MachineID>& local_neighbors,
      Master& master,
      const std::uint16_t base_port
    ) noexcept
    {
      ExternalMaster to_ret(std::move(conn));
      to_ret.master_ = &master;
      return init_conn(
        to_ret,
        [&]() { return to_ret.send_greeting(local_id, local_neighbors, base_port); },
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
      const MachineID& local_id,
      const std::vector<MachineID>& local_neighbors,
      Master& master,
      const std::uint16_t base_port
    ) noexcept
    {
      ExternalMaster to_ret(std::move(conn));
      to_ret.master_ = &master;
      return init_conn(
        to_ret,
        [&]() { return to_ret.wait_for_greeting(); },
        [&]() { return to_ret.send_greeting(local_id, local_neighbors, base_port); }
      );
    }

    void ExternalMaster::get_and_handle_messages() noexcept
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
    bool ExternalMaster::has_neighbor(const MachineID& id) const noexcept
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

    void ExternalMaster::find_publishers_for_tags(
      const std::vector<TagID>& tags,
      const bool ignore_cache
    ) noexcept
    {
      SKYNET_TRACE_LOG(
        "\"{}\" asking \"{}\" for tags {}",
        master_->id(),
        id_,
        tags
      );
      if (!pending_tag_request_)
      {
        send_message(make_get_publishers(tags, ignore_cache));
        pending_tag_request_ = true;
      }
      // Always update these when new requests come in;
      backoff_counter_ = 0;
      request_tags_time_ = calc_next_request_time();
    }

    std::string ExternalMaster::two_way_address() const noexcept
    {
      const auto [ip_address, dummy] = conn_.ip_address_and_port();
      (void)dummy;
      return ip_address + ':' + std::to_string(base_port_);
    }

    std::string ExternalMaster::publisher_address() const noexcept
    {
      const auto [ip_address, dummy] = conn_.ip_address_and_port();
      (void)dummy;
      const auto str = ip_address + ':' + std::to_string(
        static_cast<std::uint16_t>(base_port_ + publisher_port_offset)
      );
      return str;
    }

    void ExternalMaster::ask_for_pending_tags_if_past_time(const std::vector<TagID>& tags) noexcept
    {
      assert(!tags.empty());
      if (std::chrono::steady_clock::now() > request_tags_time_)
      {
        send_message(make_get_publishers(tags, ignore_cache_on_next_request_));
        ignore_cache_on_next_request_ = false;
        ++backoff_counter_;
        request_tags_time_ = calc_next_request_time();
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

    std::optional<StatusMessageHandler> ExternalMaster::try_to_get_status_message() noexcept
    {
      std::array<std::byte, sizeof(NetworkSizeType)> size_buffer;
      if (read_from_conn(size_buffer.data(), size_buffer.size()))
      {
        const auto bytes_to_read = from_network_bytes(size_buffer);
        // Then read the actual message and parse it
        if (const auto message_buffer = read_chunked(conn_, bytes_to_read); !message_buffer.empty())
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
    bool ExternalMaster::send_greeting(
      const MachineID& local_id,
      const std::vector<MachineID>& local_neighbors,
      const std::uint16_t base_port
    ) noexcept
    {
      send_message(make_greeting(local_id, local_neighbors, base_port));
      SKYNET_TRACE_LOG(
        "\"{}\" sending greeting to {}",
        master_->id(),
        conn_.ip_address_and_port()
      );
      return !dead_;
    }

    // Wait until the greeting is sent
    bool ExternalMaster::wait_for_greeting() noexcept
    {
      // Wait for the greeting
      // TODO: Probably want a time-out?
      SKYNET_TRACE_LOG(
        "\"{}\" waiting for greeting from {}",
        master_->id(),
        conn_.ip_address_and_port()
      );
      while (!dead_)
      {
        if (auto handle = try_to_get_status_message())
        {
          return handle->do_callback(
            [&](const Greeting& msg) {
              neighbors_ = msg.neighbors();
              id_ = msg.from();
              base_port_ = msg.base_port();
              SKYNET_TRACE_LOG(
                "\"{}\" got greeting from {}",
                master_->id(),
                conn_.ip_address_and_port()
              );
              return true;
            },
            // Any other kind of message is an error
            [&](...) {
              SKYNET_WARN_LOG(
                "\"{}\" recieved a non-greeting from {} during the handshake",
                master_->id(),
                conn_.ip_address_and_port().first
              );
              return false;
            }
          );
        }
        std::this_thread::sleep_for(std::chrono::microseconds{10});
      }
      return false;
    }

    // Handle status messages
    void ExternalMaster::handle_message(StatusMessageHandler& handle) noexcept
    {
      const auto okay = handle.do_callback(
        [&](const Greeting&) {
          // shouldn't be seeing a greeting here
          SKYNET_WARN_LOG(
            "\"{}\" recieved an unexpected greeting from \"{}\"",
            master_->id(),
            id_
          );
          return false;
        },
        [&](const Goodbye&) {
          SKYNET_TRACE_LOG(
            "\"{}\" recieved goodbye from \"{}\"",
            master_->id(),
            id_
          );
          dead_ = true;
          return true;
        },
        [&](const NewNeighbor& msg) {
          // Don't error if the neighbor is already present (as was previously
          // done) as if a machine disconnects and then re-connects it can send a
          // NewNeighbor message with a repeated ID
          const auto loc = std::lower_bound(neighbors_.cbegin(), neighbors_.cend(), msg.neighbor_id());
          SKYNET_TRACE_LOG(
            "\"{}\" recieved new neighbor from \"{}\" with id \"{}\"",
            master_->id(),
            id_,
            msg.neighbor_id()
          );
          // Insert it if it isn't already present
          if (loc == neighbors_.cend() || *loc != msg.neighbor_id())
          {
            neighbors_.insert(loc, msg.neighbor_id());
          }
          return true;
        },
        [&](const RemoveNeighbor& msg) {
          SKYNET_TRACE_LOG(
            "\"{}\" recieved remove neighbor from \"{}\" with id \"{}\"",
            master_->id(),
            id_,
            msg.neighbor_id()
          );
          const auto loc = std::lower_bound(neighbors_.begin(), neighbors_.end(), msg.neighbor_id());
          // Neighbors that don't exist will often be reported if it's a shared neighbor and
          // it has already been removed due to the goodbye message
          if (loc != neighbors_.end())
          {
            // otherwise just remove it
            using std::swap;
            swap(*loc, neighbors_.back());
            neighbors_.pop_back();
          }
          return true;
        },
        [this](const Heartbeat&) {
          // If trace logging isn't enable then `this` isn't used, so make
          // sure it is marked as used
          (void)this;
          // Nothing to do; this is just to acknowledge it exists
          // (Last heard time was already updated)
          SKYNET_TRACE_LOG(
            "\"{}\" recieved heartbeat from \"{}\"",
            master_->id(),
            id_
          );
          return true;
        },
        [&](const ReportPublishers& msg) {
          SKYNET_TRACE_LOG(
            "\"{}\" recieved report publishers from \"{}\" with remote tags \"{}\" and local tags \"{}\"",
            master_->id(),
            id_,
            msg.tags(),
            msg.locally_produced_tags()
          );
          // Make sure all of the tag names are okay
          for (const auto& tag_list : {msg.tags(), msg.locally_produced_tags()})
          {
            for (const auto& tag : tag_list)
            {
              if (!tag_name_okay(tag))
              {
                SKYNET_WARN_LOG(
                  "\"{}\" dropping connection with \"{}\" due to bad tag \"{}\" in report.",
                  master_->id(),
                  id_,
                  tag
                );
                return false;
              }
            }
          }
          const bool ignore_cache =
            Master::ExternalMasterAccessor::add_publishers_and_propagate(*master_, msg, *this);
          // Don't overwrite needing to ignore the cache with not needing to ignore it
          if (ignore_cache)
          {
            ignore_cache_on_next_request_ = true;
          }
          // Mark there as not being a request out there and update the time to send out
          pending_tag_request_ = false;
          request_tags_time_ = calc_next_request_time();
          return true;
        },
        [&](const GetPublishers& msg) {
          SKYNET_TRACE_LOG(
            "\"{}\" recieved get publishers from \"{}\" requesting tags {}",
            master_->id(),
            id_,
            msg.tags()
          );
          for (const auto& tag : msg.tags())
          {
            if (!tag_name_okay(tag))
            {
              SKYNET_WARN_LOG(
                "\"{}\" discarded connection with \"{}\" due to bad tag name \"{}\"",
                master_->id(),
                id_,
                tag
              );
              return false;
            }
          }
          Master::ExternalMasterAccessor::handle_get_publishers(*master_, msg, *this);
          return true;
        },
        [&](const JoinReduceGroup& msg) {
          SKYNET_TRACE_LOG(
            "\"{}\" recieved join reduce group from \"{}\" for group \"{}\", producing tag \"{}\"",
            master_->id(),
            id_,
            msg.reduce_tag(),
            msg.tag_produced()
          );
          if (!tag_name_okay(msg.reduce_tag()) || !tag_name_okay(msg.tag_produced()))
          {
            return false;
          }
          return Master::ExternalMasterAccessor::handle_join_reduce_group(*master_, msg, *this);
        },
        [&](const SubmitReduceValue& msg) {
          SKYNET_TRACE_LOG(
            "\"{}\" recieved submit reduce value from \"{}\" for group \"{}\", tag \"{}\"",
            master_->id(),
            id_,
            msg.reduce_tag(),
            msg.data().tag_id()
          );
          if (!tag_name_okay(msg.reduce_tag()) || !tag_name_okay(msg.data().tag_id()))
          {
            return false;
          }
          return Master::ExternalMasterAccessor::handle_submit_reduce_value(*master_, msg, *this);
        },
        [&](const ReportReduceResult& msg) {
          SKYNET_TRACE_LOG(
            "\"{}\" recieved report reduce value from \"{}\" for group \"{}\", tag \"{}\"",
            master_->id(),
            id_,
            msg.reduce_tag(),
            msg.data().tag_id()
          );
          if (!tag_name_okay(msg.reduce_tag()) || !tag_name_okay(msg.data().tag_id()))
          {
            return false;
          }
          return Master::ExternalMasterAccessor::handle_report_reduce_result(*master_, msg, *this);
        },
        [&](const ReportReduceDisconnection& msg) {
          // TODO: Fill this out
          if (!tag_name_okay(msg.reduce_tag()))
          {
            return false;
          }
          return Master::ExternalMasterAccessor::handle_report_reduce_disconnection(*master_, msg, *this);
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

    std::chrono::steady_clock::time_point ExternalMaster::calc_next_request_time() const noexcept
    {
      using namespace std::chrono_literals;
      static constexpr std::array backoff_times{
        5ms, 10ms, 20ms, 40ms, 80ms, 160ms, 320ms, 500ms, 750ms, 1000ms, 2000ms, 5000ms
      };
      const auto add_time = backoff_counter_ >= backoff_times.size()
        ? backoff_times.back()
        : backoff_times[backoff_counter_];
      return std::chrono::steady_clock::now() + add_time;
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
    , pub_channel_{static_cast<std::uint16_t>(port + publisher_port_offset)}
    , comm_port_{port}
  {
    if (server_socket_.set_to_listen(port) != internal::ConnectionError::no_error)
    {
      std::cerr << "Master::Master failed to set port to listening!\n";
      std::exit(1);
    }
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
    return connect_impl(address, port) != neighbors_.end();
  }

  bool Master::connect_to_server(std::string_view address) noexcept
  {
    const auto [port, addr] = internal::split_address(address);
    return connect_to_server(addr.c_str(), port);
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
        *this,
        comm_port_
      )) {
        const auto new_id = new_neighbor->id();
        // This ID already exists; so drop the connection
        if (neighbors_.find(new_id) != neighbors_.end())
        {
          SKYNET_WARN_LOG(
            "\"{}\" rejected connection due to the id already being used",
            id_,
            new_neighbor->id()
          );
          continue;
        }
        SKYNET_TRACE_LOG("\"{}\" accepted connection from \"{}\"", id_, new_neighbor->id());
        notify_of_new_neighbor(new_id);
        const auto [iter, inserted] = neighbors_.emplace(new_id, std::move(*new_neighbor));
        assert(inserted);
        // Send request for any pending tags
        if (!pending_tags_.empty())
        {
          iter->second.find_publishers_for_tags(pending_tags_, false);
        }
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
    std::function<void(Job&)> to_run
  ) noexcept
  {
    const auto res = jobs_.try_emplace(
      name,
      Job::Accessor::AllowConstruction{},
      name,
      *this,
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
    while (!jobs_.empty())
    {
      const auto end_sleep_time = std::chrono::steady_clock::now() + 100us;
      // Remove any finished jobs
      for (auto iter = jobs_.begin(); iter != jobs_.end(); )
      {
        std::unique_lock lock{Job::Accessor::get_mutex(iter->second), std::try_to_lock};
        if (lock.owns_lock() && iter->second.is_finished())
        {
          // Need to unlock before deallocation
          lock.unlock();
          iter = jobs_.erase(iter);
        }
        else
        {
          ++iter;
        }
      }
      {
        // Ensure there's no data race with jobs
        std::lock_guard lock{job_mut_};
        accept_pending_connections();
        pub_channel_.accept_subscriptions();
        read_data_from_subscriptions();
        handle_neighbor_messages();
        remove_dead_neighbors();
        remove_dead_subscriptions();
        for (auto&& neighbor : neighbors_)
        {
          neighbor.second.send_heartbeat_if_past_interval(heartbeat_interval_);
          if (!pending_tags_.empty())
          {
            neighbor.second.ask_for_pending_tags_if_past_time(pending_tags_);
          }
        }
      }
      // Mutex has been released - notify CV's if requested
      using cv_ref_pair = std::pair<bool&, std::condition_variable&>;
      std::array<cv_ref_pair, 2> cv_array{
        cv_ref_pair{notify_new_subscriptions_, new_subscription_cv_},
        cv_ref_pair{notify_reduce_group_, reduce_group_cv_}
      };
      for (auto& [notify, cv] : cv_array)
      {
        if (notify)
        {
          cv.notify_all();
          notify = false;
        }
      }
      // Wait a bit for other messages
      std::this_thread::sleep_until(end_sleep_time);
    }
    // Join all of the threads now
    for (auto& thread : threads)
    {
      thread.join();
    }
  }

  std::string Master::local_publishing_address() const noexcept
  {
    const auto [address, port] = server_socket_.ip_address_and_port();
    (void)address;
    return "localhost:" + std::to_string(
      static_cast<std::uint16_t>(port + publisher_port_offset)
    );
  }

  int Master::num_subscribers() const noexcept
  {
    std::lock_guard lock{job_mut_};
    return pub_channel_.num_subscriptions();
  }

  const std::string& Master::id() const noexcept
  {
    return id_;
  }

  auto Master::connect_impl(const char* address, std::uint16_t port) noexcept -> decltype(neighbors_)::iterator
  {
    internal::SocketCommunicator to_connect;
    SKYNET_TRACE_LOG("\"{}\" attempting to connect to {}:{}", id_, address, port);
    if (to_connect.connect_to_server(address, port) != internal::ConnectionError::no_error)
    {
      // Failing to connect to a server is normal enough that the logging level
      // here should probably be trace or debug at the most
      SKYNET_TRACE_LOG("\"{}\" failed to connect to {}:{}", id_, address, port);
      // internal::on_error("Master::connect_to_server failed!");
      return neighbors_.end();
    }
    if (auto new_neighbor = internal::ExternalMaster::create(
      internal::ByRequest{},
      std::move(to_connect),
      id_,
      make_neighbor_vector(),
      *this,
      comm_port_
    ))
    {
      const auto new_id = new_neighbor->id();
      // This ID already exists; drop the connection
      if (neighbors_.find(new_id) != neighbors_.end())
      {
        SKYNET_WARN_LOG(
          "\"{}\" rejected {}:{} due to a neighbor already using the id \"{}\"",
          id_,
          address,
          port,
          new_neighbor->id()
        );
        return neighbors_.end();
      }
      notify_of_new_neighbor(new_id);
      const auto [iter, inserted] = neighbors_.emplace(new_id, std::move(*new_neighbor));
      assert(inserted);
      // Send request for any pending tags
      if (!pending_tags_.empty())
      {
        iter->second.find_publishers_for_tags(pending_tags_, false);
      }
      return iter;
    }
    else
    {
      return neighbors_.end();
    }
  }

  void Master::handle_neighbor_messages() noexcept
  {
    for (auto&& neighbor : neighbors_)
    {
      neighbor.second.get_and_handle_messages();
    }
  }

  void Master::publish(
    const VersionID version,
    const TagID& tag_id,
    const PublishValueVariant& value
  ) noexcept
  {
    const auto msg = internal::make_publish(version, tag_id, value);
    SKYNET_TRACE_LOG(
      "\"{}\" publishing on tag \"{}\", version \"{}\", data {} to {} subscribers",
      id_,
      tag_id,
      version,
      value,
      pub_channel_.num_subscriptions()
    );
    pub_channel_.send_message(msg.data(), msg.size());
  }

  // Adds data to the tag queue for a job from a message
  // Returns true if it was successful, false if something went wrong
  bool Master::add_data_to_queue(const internal::PublishData& msg) noexcept
  {
    for (auto& [name, job] : jobs_)
    {
      (void)name;
      const auto msg_var = msg.value().get_variant();
      if (!msg_var) { return false; }
      if (!Job::Accessor::process_data(job, msg.tag_id(), *msg_var, msg.version()))
      {
        return false;
      }
    }
    return true;
  }

  void Master::notify_of_new_neighbor(const MachineID id) noexcept
  {
    send_to_neighbors(internal::make_new_neighbor(id));
  }

  void Master::remove_dead_neighbors() noexcept
  {
    for (auto it = neighbors_.begin(); it != neighbors_.end(); /* nothing */)
    {
      if (it->second.is_dead())
      {
        // TODO: Tell reduce groups when this happens
        send_to_neighbors(internal::make_remove_neighbor(it->first));
        // Find any reduce groups that this machine is a part of and
        // notify them of the disconnection
        // TODO: Probably want to cache this at some point so everything
        // doesn't have to be scanned over anytime something disconnects?
        for (auto& [tag, info] : reduce_tag_data_)
        {
          (void)tag;
          // Pointers to prevent copies
          auto scan_lists = {
            &info.parent_machines,
            &info.child_machines[0],
            &info.child_machines[1]
          };
          for (auto& list_ptr : scan_lists)
          {
            auto& list = *list_ptr;
            // Also remove the connection
            const auto iter = std::find(list.cbegin(), list.cend(), it->first);
            if (iter != list.cend())
            {
              list.erase(iter);
              info.group.report_disconnection();
            }
          }
        }
        it = neighbors_.erase(it);
      }
      else
      {
        ++it;
      }
    }
  }

  void Master::remove_dead_subscriptions() noexcept
  {
    // for (auto& [sub_handle, produced_tags] : subscriptions_)
    for (auto iter = subscriptions_.begin(); iter != subscriptions_.cend();)
    {
      const auto& [sub_handle, produced_tags] = *iter;
      if (sub_handle.is_disconnected())
      {
        for (auto& [name, job] : jobs_)
        {
          (void)name;
          for (const auto& tag : produced_tags)
          {
            Job::Accessor::report_dead_tag(job, tag);
          }
        }
        iter = subscriptions_.erase(iter);
      }
      else
      {
        ++iter;
      }
    }
  }

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

  bool Master::subscribe_is_done(const std::vector<TagID>& required_tags) noexcept
  {
    // TODO: This is probably kind of slow, use maybe an unordered map or something
    // if this becomes an issue
    std::unordered_set<std::string> remaining_tags(required_tags.cbegin(), required_tags.cend());
    for (const auto& subscription : subscriptions_)
    {
      if (!subscription.subscription.is_disconnected())
      {
        for (const auto& tag : subscription.produced_tags)
        {
          remaining_tags.erase(tag);
        }
      }
    }
    if (remaining_tags.empty())
    {
      SKYNET_TRACE_LOG("\"{}\" subscription for tags {} finished.", id_, required_tags);
    }
    return remaining_tags.empty();
  }

  auto Master::subscribe(const std::vector<TagID>& tag_ids) noexcept
    -> Future<void, internal::MasterSubscribeIsDone, internal::FutureGetNoOp>
  {
    SKYNET_TRACE_LOG("\"{}\" looking for subscription information for {}", id_, tag_ids);
    std::vector<TagID> tags_to_search_for;
    for (const auto& tag_id : tag_ids)
    {
      // Check if already subscribed, skip if so
      const bool already_subscribed = [this, &tag_id]() {
        for (const auto& sub : subscriptions_)
        {
          if (!sub.subscription.is_disconnected())
          {
            auto& prod_tags = sub.produced_tags;
            if (std::find(prod_tags.cbegin(), prod_tags.cend(), tag_id) != prod_tags.cend())
            {
              return true;
            }
          }
        }
        return false;
      }();
      if (already_subscribed)
      {
        SKYNET_TRACE_LOG("\"{}\" already subscribed for tag \"{}\"", id_, tag_id);
        continue;
      }
      // If the tag is produced locally just subscribe to self
      if (produced_tags_.find(tag_id) != produced_tags_.cend())
      {
        // Self-subscription is more complicated than I had thought, since it essentially requires either
        // a seperate thread for accepting the request or "simulating" a connection to self
        // (This could also be solved with a non-blocking connect or similar probably)
        // So, for now, just prohibit it
        // if (!try_to_subscribe(local_publishing_address(), {produced_tags_.cbegin(), produced_tags_.cend()}))
        // {
        std::cerr
          << std::quoted(id_) << " tried to subscribe to itself for tag " << std::quoted(tag_id) << "\n";
        std::terminate();
        // }
        // continue;
      }
      // Otherwise, mark the tag as pending
      pending_tags_.push_back(tag_id);
      tags_to_search_for.push_back(tag_id);
    }
    if (!tags_to_search_for.empty())
    {
      SKYNET_TRACE_LOG("\"{}\" looking for new publishers for tags {}", id_, tags_to_search_for);
      for (auto& neighbor : neighbors_)
      {
        // Presume that the cache is okay
        neighbor.second.find_publishers_for_tags(tags_to_search_for, false);
      }
    }
    return internal::make_future(
      job_mut_,
      new_subscription_cv_,
      internal::MasterSubscribeIsDone{*this, tag_ids}
    );
  }

  void Master::handle_get_publishers(
    const internal::GetPublishers& msg,
    internal::ExternalMaster& from
  ) noexcept
  {
    // If all of the tag requirements are fulfilled then
    const auto remaining_tags = remove_tags_with_known_publishers(msg);
    if (remaining_tags.empty())
    {
      SKYNET_TRACE_LOG(
        "\"{}\" sending \"{}\" publisher information for {}, all tags have been fulfilled",
        id_,
        from.id(),
        msg.tags()
      );
      // Send the information back now
      from.send_message(make_known_tag_publisher_message());
    }
    else
    {
      // Mark all tags from the message in the cache so that they will be
      // sent back so that the recieving end no longer thinks they are pending
      // Also clear them if the cache is being ignored, as it is assumed that
      // they are now invalid
      for (const auto& tag : remaining_tags)
      {
        const auto& [iter, inserted] = publishers_for_tag_.try_emplace(tag);
        (void)inserted;
        if (msg.ignore_cache())
        {
          iter->second.clear();
        }
      }
      // If there are no other neighbors, just answer right away so
      // it doesn't stall
      if (neighbors_.size() == 1)
      {
        SKYNET_TRACE_LOG(
          "\"{}\" sending \"{}\" publisher information for {}, no neighbors to ask",
          id_,
          from.id(),
          [&]() {
            std::vector<TagID> known_tags;
            for (const auto& [tag, publishers] : publishers_for_tag_)
            {
              if (!publishers.empty())
              {
                known_tags.push_back(tag);
              }
            }
            return known_tags;
          }()
        );
        from.send_message(make_known_tag_publisher_message());
        return;
      }
      bool ask_neighbors = false;
      // Mark the information as needing to be propagated
      for (const auto& tag : remaining_tags)
      {
        auto [iter, dummy1] = send_publisher_information_to_.try_emplace(tag);
        auto [dummy2, inserted] = iter->second.emplace(from.id());
        (void)dummy1;
        (void)dummy2;
        // If the insert worked, there are new tags to look for, so ask neighbors
        // about them
        // If there aren't any new tags, just respond with what is known for now
        // as otherwise the entire system will deadlock when all of the wanted
        // tags can't be found
        if (inserted) { ask_neighbors = true; }
      }
      if (!ask_neighbors)
      {
        SKYNET_TRACE_LOG(
          "\"{}\" returning early for request for tags {} from \"{}\" to avoid potential deadlock",
          id_,
          msg.tags(),
          from.id()
        );
        from.send_message(make_known_tag_publisher_message());
      }
      else
      {
        SKYNET_TRACE_LOG(
          "\"{}\" asking neighbors for tags {} for \"{}\"",
          id_,
          msg.tags(),
          from.id()
        );
        for (auto& neighbor : neighbors_)
        {
          if (&neighbor.second != &from)
          {
            neighbor.second.find_publishers_for_tags(remaining_tags, false);
          }
        }
      }
    }
  }

  std::vector<TagID> Master::remove_tags_with_known_publishers(const internal::GetPublishers& msg) noexcept
  {
    auto tags_left = msg.tags();
    // Remove tags that either have a known producer or are known locally
    tags_left.erase(
      std::remove_if(
        tags_left.begin(),
        tags_left.end(),
        [&](const TagID& id) {
          const auto loc = publishers_for_tag_.find(id);
          return loc == publishers_for_tag_.cend()
            ? produced_tags_.find(id) != produced_tags_.cend()
            : !loc->second.empty();
        }
      ),
      tags_left.end()
    );
    return tags_left;
  }

  bool Master::add_publishers_and_propagate(
    const internal::ReportPublishers& msg,
    const internal::ExternalMaster& from
  ) noexcept
  {
    const auto tags = msg.tags();
    const auto publishers_list = msg.addresses();
    // These should always match sizes; just ignore the message if they don't
    // TODO: Actually handle this
    if (tags.size() != publishers_list.size())
    {
      SKYNET_WARN_LOG("\"{}\" recieved tag/publisher list size mismatch from \"{}\"", id_, from.id());
      return false;
    }
    // Add the information to what is locally known
    for (std::size_t i = 0; i < tags.size(); ++i)
    {
      const auto& tag = tags[i];
      const auto& publishers = publishers_list[i];
      // Find or create the tag
      const auto loc = publishers_for_tag_.find(tag);
      if (loc == publishers_for_tag_.end())
      {
        publishers_for_tag_.try_emplace(tag, publishers.cbegin(), publishers.cend());
      }
      else
      {
        loc->second.insert(publishers.cbegin(), publishers.cend());
      }
    }
    // Add the tags that the external master produced
    const auto external_tags = msg.locally_produced_tags();
    for (const auto& tag : external_tags)
    {
      const auto loc = publishers_for_tag_.find(tag);
      const auto address = tag[0] == internal::publish_tag_marker
        ? from.publisher_address()
        : from.two_way_address();
      if (loc == publishers_for_tag_.end())
      {
        publishers_for_tag_.emplace(
          tag,
          std::initializer_list<std::string>{address}
        );
      }
      else
      {
        loc->second.insert(address);
      }
    }
    // Propagate to any machines that need this information, marking them
    // as no longer needing propagation as well
    std::unordered_set<MachineID> machines_to_send_to;
    for (const auto& [tag, data] : publishers_for_tag_)
    {
      (void)data;
      const auto loc = send_publisher_information_to_.find(tag);
      if (loc != send_publisher_information_to_.cend())
      {
        internal::merge_associative_containers(machines_to_send_to, loc->second);
        send_publisher_information_to_.erase(loc);
      }
    }
    if (!machines_to_send_to.empty())
    {
      // Produce vectors for the machines and tags
      std::vector<TagID> tags_to_send;
      std::vector<std::vector<std::string>> addresses_to_send;
      for (const auto& [tag, addresses] : publishers_for_tag_)
      {
        // Don't send data for tags that don't have any known publishers
        if (!addresses.empty())
        {
          tags_to_send.push_back(tag);
          addresses_to_send.emplace_back(addresses.cbegin(), addresses.cend());
        }
      }
      const std::vector<TagID> local_tags(produced_tags_.cbegin(), produced_tags_.cend());
      const auto to_send = internal::make_report_publishers(
        tags_to_send,
        addresses_to_send,
        local_tags
      );
      // Send to the machines if they are present
      for (const auto& send_to : machines_to_send_to)
      {
        const auto loc = neighbors_.find(send_to);
        if (loc != neighbors_.end())
        {
          SKYNET_TRACE_LOG(
            "\"{}\" propagating back to \"{}\" for remote tags {} and local tags {}",
            id_,
            send_to,
            tags_to_send,
            local_tags
          );
          loc->second.send_message(to_send);
        }
      }
    }
    return try_connections_for_pending_tags();
  }

  std::vector<std::byte> Master::make_known_tag_publisher_message() const noexcept
  {
    std::vector<TagID> tags;
    std::vector<std::vector<std::string>> addresses;
    for (const auto& [tag, publishers] : publishers_for_tag_)
    {
      if (!publishers.empty())
      {
        tags.push_back(tag);
        addresses.emplace_back(publishers.cbegin(), publishers.cend());
      }
    }
    const std::vector<TagID> local_tags(produced_tags_.cbegin(), produced_tags_.cend());
    return internal::make_report_publishers(tags, addresses, local_tags);
  }

  bool Master::try_to_subscribe(
    const std::string_view address,
    std::vector<std::string> remote_tags_produced
  ) noexcept
  {
    if (auto sub = internal::Subscription::try_to_create(address))
    {
      subscriptions_.emplace_back(SubscriptionData{std::move(*sub), remote_tags_produced});
      return true;
    }
    return false;
  }

  void Master::read_data_from_subscriptions() noexcept
  {
    // TODO: Actually handle things when reading fails... which if it requires searching for
    // another publisher could be very expensive, not really sure what else could be done though.
    // Just ignore failures for now.
    for (auto& [sub, tags] : subscriptions_)
    {
      (void)tags;
      std::array<std::byte, sizeof(NetworkSizeType)> size_buffer;
      // I... honestly don't know what to do about all of these if statements
      // I guess that's what happens when many optionals are used
      if (sub.read_message(size_buffer.data(), size_buffer.size()) == internal::ConnectionError::no_error)
      {
        const auto bytes_to_read = internal::from_network_bytes(size_buffer);
        SKYNET_TRACE_LOG(
          "\"{}\" recieved a publication of {} bytes from \"{}\"",
          id_,
          bytes_to_read,
          sub.ip_address_and_port()
        );
        // Then read the actual message and parse it
        if (const auto message_buffer = sub.read_chunked(bytes_to_read); !message_buffer.empty())
        {
          SKYNET_TRACE_LOG(
            "\"{}\" successfully read publication message of {} bytes from \"{}\"",
            id_,
            bytes_to_read,
            sub.ip_address_and_port()
          );
          if (const auto msg = internal::PublishMessageHandler::try_to_create(message_buffer))
          {
            if (const auto data = msg->data())
            {
              if (const auto variant = data->value().get_variant())
              {
                for (auto& [job_id, job] : jobs_)
                {
                  SKYNET_TRACE_LOG(
                    "\"{}\" recieved data on tag \"{}\", version {}, data: {}",
                    id_,
                    data->tag_id(),
                    data->version(),
                    *variant
                  );
                  (void)job_id;
                  Job::Accessor::process_data(
                    job,
                    data->tag_id(),
                    *variant,
                    data->version()
                  );
                }
              }
            }
          }
        }
      }
    }
  }

  std::vector<std::string> Master::get_tags_for_publisher(const std::string_view publisher_address) const noexcept
  {
    std::vector<std::string> tags_produced;
    for (const auto& [tag, publishers] : publishers_for_tag_)
    {
      if (std::find(publishers.cbegin(), publishers.cend(), publisher_address) != publishers.cend())
      {
        tags_produced.push_back(tag);
      }
    }
    return tags_produced;
  }

  void Master::report_new_publish_tags(const std::vector<TagID>& tags) noexcept
  {
    SKYNET_TRACE_LOG("\"{}\" adding tags produced: {}", id_, tags);
    // Mark the tags produced by this job
    for (const auto& tag : tags)
    {
      const auto [iter, inserted] = produced_tags_.insert(tag);
      (void)iter;
      if (!inserted)
      {
        // Two jobs on the same master can't produce the same tag; fail loudly
        std::cerr << "The tag " << std::quoted(tag) << " was reported for publication more than once!\n";
        std::terminate();
      }
    }
  }

  auto Master::create_reduce_group(
    const TagID& group_id,
    const TagID& tag_produced,
    const internal::ReduceGroupNeighbors& tags_to_find,
    const std::uint8_t expected_type
  ) noexcept
    -> Future<internal::ReduceGroupBase&, internal::MasterReduceGroupIsCreated, internal::MasterGetReduceGroup>
  {
    // Create an entry for the group
    const auto [tag_iter, tag_inserted] = produced_tags_.insert(tag_produced);
    (void)tag_iter;
    if (!tag_inserted)
    {
      std::cerr
        << "The tag " << std::quoted(tag_produced) << " was attempted to be produced for more than one reduce group!\n";
      std::terminate();
    }
    const auto [iter, inserted] = reduce_tag_data_.try_emplace(
      group_id,
      tags_to_find,
      *this,
      group_id,
      tag_produced,
      expected_type
    );
    if (!inserted)
    {
      std::cerr
        << "The reduce group " << std::quoted(group_id) << " was attempted to be created twice!\n";
      std::terminate();
    }
    const auto& parent_tag = iter->second.group.tag_neighbors().parent();
    if (!parent_tag.empty())
    {
      pending_tags_.push_back(parent_tag);
      for (auto& neighbor : neighbors_)
      {
        neighbor.second.find_publishers_for_tags({parent_tag}, false);
      }
    }
    return internal::make_future(
      job_mut_,
      reduce_group_cv_,
      internal::MasterReduceGroupIsCreated{*this, group_id},
      internal::MasterGetReduceGroup{*this, group_id}
    );
  }

  bool Master::reduce_group_is_created(const TagID& group_id) noexcept
  {
    // See if the parent has a connection
    const auto group_iter = reduce_tag_data_.find(group_id);
    assert(group_iter != reduce_tag_data_.cend());
    const auto& reduce_data = group_iter->second;
    const auto& parent_tag = reduce_data.group.tag_neighbors().parent();
    if (!parent_tag.empty() && reduce_data.parent_machines.empty())
    {
      SKYNET_TRACE_LOG(
        "\"{}\" - reduce group \"{}\" is not yet created as there is no parent connection",
        id_,
        group_id
      );
      return false;
    }
    // Check that the children have joined the group
    for (std::size_t i = 0; i < reduce_data.child_machines.size(); ++i)
    {
      // Ignore empty tags
      if (!reduce_data.group.tag_neighbors().tags[i + 1].empty() && reduce_data.child_machines[i].empty())
      {
        SKYNET_TRACE_LOG(
          "\"{}\" - reduce group \"{}\" is not yet created as the {} child has no connections",
          id_,
          group_id,
          i == 0 ? "left" : "right"
        );
        return false;
      }
    }
    SKYNET_TRACE_LOG("\"{}\" - reduce group \"{}\" is ready", id_, group_id);
    return true;
  }

  bool Master::handle_join_reduce_group(
    const internal::JoinReduceGroup& msg,
    const internal::ExternalMaster& from
  ) noexcept
  {
    // Check if the reduce group exists
    const auto reduce_group_loc = reduce_tag_data_.find(msg.reduce_tag());
    if (reduce_group_loc == reduce_tag_data_.cend())
    {
      return false;
    }
    // Now check against the children tags, and add to them if they match,
    // making sure it doesn't already exist
    auto& reduce_group = reduce_group_loc->second;
    auto& child_machines = reduce_group.child_machines;
    for (std::size_t i = 0; i < child_machines.size(); ++i)
    {
      // See if the tag matches
      if (msg.tag_produced() == reduce_group.group.tag_neighbors().tags[i + 1])
      {
        // Add it, unless it's already in there; that's an error
        auto& existing_conns = child_machines[i];
        const auto tag_loc = std::find(existing_conns.cbegin(), existing_conns.cend(), msg.tag_produced());
        if (tag_loc != existing_conns.cend())
        {
          SKYNET_WARN_LOG(
            "\"{}\" recieved join group from \"{}\" for tag \"{}\" for reduce group \"{}\", but it already existed in the group.",
            id_,
            from.id(),
            msg.tag_produced(),
            msg.reduce_tag()
          );
          // Remove it from the container as the connection will be killed
          existing_conns.erase(tag_loc);
          return false;
        }
        else
        {
          // otherwise just add it and mark this as a success
          existing_conns.push_back(from.id());
          notify_reduce_group_ = true;
          return true;
        }
      }
    }
    SKYNET_WARN_LOG(
      "\"{}\" recieved join group from \"{}\" for tag \"{}\" for reduce group \"{}\", but such a group does not exist.",
      id_,
      from.id(),
      msg.tag_produced(),
      msg.reduce_tag()
    );
    return false;
  }

  internal::ReduceGroupBase& Master::get_reduce_group(const TagID& group_id) noexcept
  {
    const auto loc = reduce_tag_data_.find(group_id);
    assert(loc != reduce_tag_data_.cend());
    return loc->second.group;
  }

  void Master::send_reduce_data_to_parent(
    const TagID& group_id,
    const std::vector<std::byte>& reduce_message
  ) noexcept
  {
    const auto loc = reduce_tag_data_.find(group_id);
    assert(loc != reduce_tag_data_.cend());
    auto& parent_machines = loc->second.parent_machines;
    // Go through all of the parents, removing ones that don't exist
    for (auto parent_iter = parent_machines.begin(); parent_iter != parent_machines.end(); )
    {
      const auto parent_loc = neighbors_.find(*parent_iter);
      if (parent_loc == neighbors_.cend())
      {
        parent_iter = parent_machines.erase(parent_iter);
      }
      else
      {
        parent_loc->second.send_message(reduce_message);
        ++parent_iter;
      }
    }
  }

  void Master::send_reduce_data_to_children(
    const TagID& group_id,
    const std::vector<std::byte>& reduce_message
  ) noexcept
  {
    const auto loc = reduce_tag_data_.find(group_id);
    assert(loc != reduce_tag_data_.cend());
    auto child_machines = loc->second.child_machines;
    for (auto& children : child_machines)
    {
      for (auto child_iter = children.begin(); child_iter != children.end(); )
      {
        const auto child_loc = neighbors_.find(*child_iter);
        if (child_loc == neighbors_.cend())
        {
          child_iter = children.erase(child_iter);
        }
        else
        {
          child_loc->second.send_message(reduce_message);
          ++child_iter;
        }
      }
    }
  }

  // TODO: Handle when running out of children
  bool Master::handle_submit_reduce_value(
    const internal::SubmitReduceValue& msg,
    const internal::ExternalMaster& from
  ) noexcept
  {
    return handle_reduce_value(msg.reduce_tag(), msg.data(), from);
  }

  bool Master::handle_report_reduce_result(
    const internal::ReportReduceResult& msg,
    const internal::ExternalMaster& from
  ) noexcept
  {
    return handle_reduce_value(msg.reduce_tag(), msg.data(), from);
  }

  bool Master::handle_reduce_value(
    const TagID& reduce_group_id,
    const internal::PublishData& value,
    const internal::ExternalMaster& from
  ) noexcept
  {
    // Cast to void to avoid unused parameter warnings when the warn level isn't enabled.
    (void)from;
    // Make sure the group exists
    const auto group_loc = reduce_tag_data_.find(reduce_group_id);
    if (group_loc == reduce_tag_data_.cend())
    {
      SKYNET_WARN_LOG(
        "\"{}\" rejected reduce value from \"{}\" for reduce group \"{}\" for tag \"{}\" as the reduce group does not exist",
        id_,
        from.id(),
        reduce_group_id,
        value.tag_id()
      );
      return false;
    }
    const auto var_opt = value.value().get_variant();
    if (!var_opt)
    {
      SKYNET_WARN_LOG(
        "\"{}\" rejected reduce value from \"{}\" for reduce group \"{}\" for tag \"{}\" as the value could not be extracted",
        id_,
        from.id(),
        reduce_group_id,
        value.tag_id()
      );
      return false;
    }
    return group_loc->second.group.add_data(value.tag_id(), *var_opt, value.version());
  }

  bool Master::handle_report_reduce_disconnection(
    const internal::ReportReduceDisconnection& msg,
    const internal::ExternalMaster& from
  ) noexcept
  {
    // Cast to void to avoid unused parameter warnings when the warn level isn't enabled.
    (void)from;
    // Make sure the group exists
    const auto group_loc = reduce_tag_data_.find(msg.reduce_tag());
    if (group_loc == reduce_tag_data_.cend())
    {
      SKYNET_WARN_LOG(
        "\"{}\" rejected reduce disconnection from \"{}\", initiated by \"{}\", "
          "for reduce group \"{}\" for as the reduce group does not exist",
        id_,
        from.id(),
        msg.initiating_machine(),
        msg.reduce_tag()
      );
      return false;
    }
    group_loc->second.group.propagate_disconnection(msg.initiating_machine(), msg.id());
    return true;
  }

  bool Master::try_connections_for_pending_tags() noexcept
  {
    bool ignore_cache = false;
    for (auto iter = pending_tags_.begin(); iter != pending_tags_.end(); ++iter)
    {
      const auto& tag_id = *iter;
      // Check if there is a list of known producers for the tag
      const auto loc = publishers_for_tag_.find(tag_id);
      // Create the entry if it exists
      if (loc != publishers_for_tag_.cend())
      {
        auto& publishers = loc->second;
        // Now, if there are any known subscriptions, try to subscribe to them
        if (!publishers.empty())
        {
          for (auto pub_iter = publishers.begin(); pub_iter != publishers.end(); )
          {
            // Sub/pub connections
            const auto& subscribe_address = *pub_iter;
            if (tag_id[0] == internal::publish_tag_marker)
            {
              if (auto sub = internal::Subscription::try_to_create(subscribe_address))
              {
                subscriptions_.push_back(SubscriptionData{
                  std::move(*sub),
                  get_tags_for_publisher(subscribe_address)
                });
                // Managed to subscribe; remove the pending tag
                using std::swap;
                swap(*iter, pending_tags_.back());
                pending_tags_.pop_back();
                --iter;
                notify_new_subscriptions_ = true;
                break;
              }
              else
              {
                SKYNET_TRACE_LOG("\"{}\" failed to subscribe to \"{}\" for tag \"{}\"", id_, subscribe_address, tag_id);
                // Couldn't subscribe - remove this as a producer
                pub_iter = publishers.erase(pub_iter);
              }
            }
            else
            {
              // Reduce group connections
              // First find the group that the tag the parent is for.
              // If there's no matching tag then just ignore it
              // TODO: Keep a look-up map if this becomes a performance issue
              auto& [group_id, reduce_data] = [&]() -> decltype(reduce_tag_data_)::reference {
                for (auto& data_pair : reduce_tag_data_)
                {
                  const auto& group_data = data_pair.second;
                  if (group_data.group.tag_neighbors().parent() == tag_id)
                  {
                    return data_pair;
                  }
                }
                assert(false && "No group matching the produced tag found?");
                return *reduce_tag_data_.begin();
              }();
              const decltype(neighbors_)::iterator server_iter = [&]() {
                // Check if there is already a connection present, and just add the
                // ID if so
                // TODO: If this is a bottleneck, keep a mapping of the ip addresses to
                // the machine names as well
                const decltype(neighbors_)::iterator conn_iter = [&]() {
                  for (auto neighbor_iter = neighbors_.begin(); neighbor_iter != neighbors_.end(); ++neighbor_iter)
                  {
                    const auto& neighbor = *neighbor_iter;
                    if (subscribe_address == neighbor.second.two_way_address())
                    {
                      return neighbor_iter;
                    }
                  }
                  return neighbors_.end();
                }();
                if (conn_iter != neighbors_.cend())
                {
                  SKYNET_TRACE_LOG(
                    "\"{}\" already has connection to \"{}\" ({}) for tag \"{}\" for reduce groups",
                    id_,
                    conn_iter->second.id(),
                    subscribe_address,
                    tag_id
                  );
                  return conn_iter;
                }
                else
                {
                  const auto [port, addr] = internal::split_address(subscribe_address);
                  SKYNET_TRACE_LOG(
                    "\"{}\" attempting to connect to {}:{} for tag \"{}\" for reduce group",
                    id_,
                    addr,
                    port,
                    tag_id
                  );
                  return connect_impl(addr.c_str(), port);
                }
              }();
              if (server_iter != neighbors_.cend())
              {
                SKYNET_TRACE_LOG(
                  "\"{}\" using \"{}\" for tag \"{}\" for reduce group",
                  id_,
                  server_iter->first,
                  tag_id
                );
                const auto& tag_produced = reduce_data.group.produced_tag();
                server_iter->second.send_message(internal::make_join_reduce_group(group_id, tag_produced));
                reduce_data.parent_machines.push_back(server_iter->second.id());
                notify_reduce_group_ = true;
                // Managed to create a connection; remove the pending tag
                using std::swap;
                swap(*iter, pending_tags_.back());
                pending_tags_.pop_back();
                --iter;
                break;
              }
              else
              {
                SKYNET_TRACE_LOG(
                  "\"{}\" failed to connect to \"{}\" for tag \"{}\" for reduce groups",
                  id_,
                  subscribe_address,
                  tag_id
                );
                pub_iter = publishers.erase(pub_iter);
              }
            }
          }
        }
        // Check if the producers are now empty (happens if all subscription attempts failed)
        if (publishers.empty())
        {
          // Need to get original producers for tags now, so have to ignore caches
          ignore_cache = true;
        }
      }
    }
    return ignore_cache;
  }
} // namespace skynet

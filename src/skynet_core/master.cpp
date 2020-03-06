#include "skynet_core/master.hpp"

#include "skynet_core/internal/utility/algorithms.hpp"
#include "skynet_core/internal/utility/logging.hpp"

#include <iomanip>
#include <iostream>
#include <limits>

namespace skynet
{
  namespace internal
  {
    ExternalMaster::ExternalMaster(
      SocketCommunicator conn,
      const MachineID& id,
      const std::vector<MachineID>& neighbors,
      Master& master,
      const std::uint16_t port
    ) noexcept
      : conn_{std::move(conn)}
      , id_{id}
      , last_heard_{std::chrono::steady_clock::now()}
      , neighbors_{neighbors}
      , master_{&master}
      , port_{port}
    {}

    void ExternalMaster::get_and_handle_messages() noexcept
    {
      if (dead_)
      {
        return;
      }
      while (auto handler = try_to_get_message())
      {
        // Update the last time something was heard
        last_heard_ = std::chrono::steady_clock::now();
        // Handle the message
        handle_message(*handler);
      }
    }

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

    MachineID ExternalMaster::id() const noexcept { return id_; }

    bool ExternalMaster::is_dead() const noexcept { return dead_; }

    void ExternalMaster::mark_as_dead() noexcept { dead_ = true; }

    void ExternalMaster::ignore_cache_on_next_request() noexcept { ignore_cache_on_next_request_ = true; }

    bool ExternalMaster::is_subscribed_to(const TagID& tag) const noexcept
    {
      return remote_subscriptions_.find(tag) != remote_subscriptions_.cend();
    }

    bool ExternalMaster::should_ask_for_tags() const noexcept
    {
      return !pending_tag_request_ && std::chrono::steady_clock::now() > request_tags_time_;
    }

    bool ExternalMaster::has_pending_tag_request() const noexcept
    {
      return pending_tag_request_;
    }

    void ExternalMaster::reset_backoff_counter() noexcept
    {
      backoff_counter_ = 0;
      request_tags_time_ = calc_next_request_time();
    }

    void ExternalMaster::increase_backoff_counter() noexcept
    {
      ++backoff_counter_;
      request_tags_time_ = calc_next_request_time();
    }

    bool ExternalMaster::has_neighbor(const MachineID& id) const noexcept
    {
      const auto loc = std::lower_bound(neighbors_.cbegin(), neighbors_.cend(), id);
      return loc != neighbors_.cend() && *loc == id;
    }

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

    void ExternalMaster::find_publishers_for_tags(const std::vector<TagID>& tags) noexcept
    {
      SKYNET_TRACE_LOG(
        "\"{}\" asking \"{}\" for tags {}{}",
        master_->id(),
        id_,
        tags,
        pending_tag_request_ ? ", but ignored due to already pending request" : ""
      );
      if (!pending_tag_request_)
      {
        send_message(make_get_publishers(tags, ignore_cache_on_next_request_));
        ignore_cache_on_next_request_ = false;
        pending_tag_request_ = true;
      }
    }

    std::string ExternalMaster::address() const noexcept
    {
      const auto [ip_address, dummy] = conn_.ip_address_and_port();
      (void)dummy;
      return ip_address + ':' + std::to_string(port_);
    }

    AddrPortPair ExternalMaster::address_pair() const noexcept
    {
      const auto [ip_address, dummy] = conn_.ip_address_and_port();
      (void)dummy;
      return {ip_address, port_};
    }

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

    std::optional<MessageHandler> ExternalMaster::try_to_get_message() noexcept
    {
      const auto bytes_to_read_or_error = read_network_size(conn_);
      if (std::holds_alternative<NetworkSizeType>(bytes_to_read_or_error))
      {
        const auto bytes_to_read = *std::get_if<NetworkSizeType>(&bytes_to_read_or_error);
        // Then read the actual message and parse it
        if (const auto message_buffer = read_chunked(conn_, bytes_to_read); !message_buffer.empty())
        {
          return MessageHandler::try_to_create(message_buffer);
        }
        else
        {
          // Couldn't read the size bytes - bad message
          dead_ = true;
          return {};
        }
      }
      else
      {
        const auto err = *std::get_if<ConnectionError>(&bytes_to_read_or_error);
        if (err != ConnectionError::would_block)
        {
          dead_ = true;
        }
        return {};
      }
    }

    // Handle status messages
    void ExternalMaster::handle_message(MessageHandler& handle) noexcept
    {
      const auto okay = handle.do_callback(
        [&](const Greeting&) {
          // shouldn't be seeing a greeting here
          SKYNET_WARN_LOG(
            "\"{}\" received an unexpected greeting from \"{}\"",
            master_->id(),
            id_
          );
          return false;
        },
        [&](const Goodbye&) {
          SKYNET_TRACE_LOG(
            "\"{}\" received goodbye from \"{}\"",
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
            "\"{}\" received new neighbor from \"{}\" with id \"{}\"",
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
            "\"{}\" received remove neighbor from \"{}\" with id \"{}\"",
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
            "\"{}\" received heartbeat from \"{}\"",
            master_->id(),
            id_
          );
          return true;
        },
        [&](const ReportPublishers& msg) {
          SKYNET_TRACE_LOG(
            "\"{}\" received report publishers from \"{}\" with remote tags \"{}\" and local tags \"{}\"",
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
          Master::ExternalMasterAccessor::add_publishers_and_propagate(*master_, msg, *this);
          // Mark there as not being a request out there and update the time to send out
          pending_tag_request_ = false;
          request_tags_time_ = calc_next_request_time();
          return true;
        },
        [&](const GetPublishers& msg) {
          SKYNET_TRACE_LOG(
            "\"{}\" received get publishers from \"{}\" requesting tags {}",
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
            "\"{}\" received join reduce group from \"{}\" for group \"{}\", producing tag \"{}\"",
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
            "\"{}\" received submit reduce value from \"{}\" for group \"{}\", tag \"{}\", version {}",
            master_->id(),
            id_,
            msg.reduce_tag(),
            msg.data().tag_id(),
            msg.data().version()
          );
          if (!tag_name_okay(msg.reduce_tag()) || !tag_name_okay(msg.data().tag_id()))
          {
            return false;
          }
          return Master::ExternalMasterAccessor::handle_submit_reduce_value(*master_, msg, *this);
        },
        [&](const ReportReduceResult& msg) {
          SKYNET_TRACE_LOG(
            "\"{}\" received report reduce value from \"{}\" for group \"{}\", tag \"{}\", version {}",
            master_->id(),
            id_,
            msg.reduce_tag(),
            msg.data().tag_id(),
            msg.data().version()
          );
          if (!tag_name_okay(msg.reduce_tag()) || !tag_name_okay(msg.data().tag_id()))
          {
            return false;
          }
          return Master::ExternalMasterAccessor::handle_report_reduce_result(*master_, msg, *this);
        },
        [&](const ReportReduceDisconnection& msg) {
          if (!tag_name_okay(msg.reduce_tag()))
          {
            return false;
          }
          return Master::ExternalMasterAccessor::handle_report_reduce_disconnection(*master_, msg, *this);
        },
        [&](const PublishData& msg) {
          if (!tag_name_okay(msg.tag_id()))
          {
            return false;
          }
          return Master::ExternalMasterAccessor::handle_publish_data(*master_, msg, *this);
        },
        [&](const SubscriptionNotice& msg) {
          SKYNET_TRACE_LOG(
            "\"{}\" received subscription notice from \"{}\" for tags {}, is unsubscribe: {}",
            master_->id(),
            id_,
            msg.tags(),
            msg.is_unsubscribe()
          );
          for (const auto& tag : msg.tags())
          {
            if (!tag_name_okay(tag))
            {
              return false;
            }
            const auto [iter, inserted] = remote_subscriptions_.emplace(tag);
            (void)iter;
            // Shouldn't receive multiple subscriptions to the same tag
            if (!inserted)
            {
              return false;
            }
          }
          if (!Master::ExternalMasterAccessor::subscription_tags_are_produced(*master_, msg))
          {
            // TODO: Send a cancellation notice instead for the tags that aren't there
            // when this happens
            return false;
          }
          SKYNET_TRACE_LOG(
            "\"{}\" accepted subscription notice from \"{}\"",
            master_->id(),
            id_
          );
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

    std::chrono::steady_clock::time_point ExternalMaster::calc_next_request_time() const noexcept
    {
      using namespace std::chrono_literals;
      static constexpr std::array<std::chrono::milliseconds, 10> backoff_times{
        20ms, 40ms, 80ms, 160ms, 320ms, 500ms, 750ms, 1000ms, 2000ms, 5000ms
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
    // , pub_channel_{static_cast<std::uint16_t>(port + publisher_port_offset)}
    , port_{port}
  {
    if (server_socket_.set_to_listen(port) != internal::ConnectionError::no_error)
    {
      std::cerr << "Master::Master failed to set port to listening!\n";
      std::exit(1);
    }
  }

  // Master::Master(const BuildMasterInfo& info) noexcept
  //   : Master{info.port, info.name, std::chrono::milliseconds{info.heartbeat_interval_in_ms}}
  // {
  //   // TODO: This blocks until it is ready.  I guess that's fine though?
  //   // Connect to the other machines now
  //   auto connections_left = info.to_connect_to;
  //   while (!connections_left.empty())
  //   {
  //     for (auto iter = connections_left.begin(); iter != connections_left.end(); /* nothing */)
  //     {
  //       const bool already_has_connection = [&]() {
  //         for (const auto& neighbor : neighbors_)
  //         {
  //           if (neighbor.second.address() == *iter)
  //           {
  //             return true;
  //           }
  //         }
  //         return false;
  //       }();
  //       if (already_has_connection || connect_to_server(*iter))
  //       {
  //         iter = connections_left.erase(iter);
  //       }
  //       else
  //       {
  //         ++iter;
  //       }
  //     }
  //     std::this_thread::sleep_for(std::chrono::milliseconds(10));
  //   }
  // }

  Master::~Master()
  {
    send_to_neighbors(internal::make_goodbye());
  }

  auto Master::connect_to_server(const char* const address, const std::uint16_t port) noexcept
    -> Waiter<internal::MasterConnectionIsComplete, internal::MasterGetConnectionSuccess>
  {
    std::lock_guard<std::mutex> lock{job_mut_};
    const auto [iter, inserted] = pending_conns_.try_emplace(
      AddrPortPair{address, port},
      PendingInfo{
        internal::SocketCommunicator{},
        ConnStatus::waiting_for_conn,
        ConnType::user_requested,
        ""
      }
    );
    if (!inserted)
    {
      std::cerr << "Address " << address << ':' << port << " attempted to be connected to twice!\n";
      std::exit(1);
    }
    const auto status = iter->second.conn.connect_non_blocking(address, port);
    // Ignore status - if this initially fails it will be handled later
    (void)status;
    return internal::make_waiter(
      job_mut_,
      connection_cv_,
      internal::MasterConnectionIsComplete{*this, address, port},
      internal::MasterGetConnectionSuccess{*this, address, port}
    );
  }

  auto Master::connect_to_server(std::string_view address) noexcept
    -> Waiter<internal::MasterConnectionIsComplete, internal::MasterGetConnectionSuccess>
  {
    const auto [addr, port] = internal::split_address(address);
    return connect_to_server(addr.c_str(), port);
  }

  void Master::accept_pending_connections() noexcept
  {
    while (auto conn = server_socket_.accept())
    {
      // This feels gross since it's basically the same thing as above, but I'm not
      // sure how to condense them as they are slightly different
      const auto& [address, port] = conn->ip_address_and_port();
      auto info = PendingInfo{
        std::move(*conn),
        ConnStatus::waiting_for_conn,
        ConnType::user_requested,
        ""
      };
      // Accept seems to re-use ports, and the actual address doesn't matter, so keep shuffling
      // until it manages to get in
      auto inc_port = port;
      while (true)
      {
        const auto [iter, inserted] = pending_conns_.try_emplace(
          AddrPortPair{address, inc_port},
          std::move(info)
        );
        (void)iter;
        ++inc_port;
        if (inserted)
        {
          SKYNET_TRACE_LOG("\"{}\" accepted connection from {}", id_, iter->second.conn.ip_address_and_port());
          break;
        }
      }
      // No need for waiters or anything
    }
  }

  int Master::number_of_neighbors() const noexcept
  {
    std::lock_guard<std::mutex> lock{job_mut_};
    return static_cast<int>(neighbors_.size());
  }

  bool Master::submit_job(
    JobID name,
    std::function<void(Job&, MasterHandle)> to_run
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
        process_pending_conns();
        accept_pending_connections();
        handle_neighbor_messages();
        remove_dead_neighbors();
        for (auto&& neighbor : neighbors_)
        {
          neighbor.second.send_heartbeat_if_past_interval(heartbeat_interval_);
          if (!pending_tags_.empty() && neighbor.second.should_ask_for_tags())
          {
            std::vector<TagID> tags_to_ask_for;
            // Only ask for tags without known producers
            std::copy_if(
              pending_tags_.cbegin(),
              pending_tags_.cend(),
              std::back_inserter(tags_to_ask_for),
              [&](const TagID& tag) {
                const auto iter = publishers_for_tag_.find(tag);
                if (iter == publishers_for_tag_.cend()) { return true; }
                return iter->second.empty();
              }
            );
            neighbor.second.increase_backoff_counter();
            neighbor.second.find_publishers_for_tags(tags_to_ask_for);
          }
        }
      }
      // Mutex has been released - notify CV's if requested
      using cv_ref_pair = std::pair<bool&, std::condition_variable&>;
      std::array<cv_ref_pair, 3> cv_array{
        cv_ref_pair{notify_new_subscriptions_, new_subscription_cv_},
        cv_ref_pair{notify_reduce_group_, reduce_group_cv_},
        cv_ref_pair{notify_connection_, connection_cv_}
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

  const std::string& Master::id() const noexcept
  {
    return id_;
  }

  int Master::num_subscriptions(const internal::PublishTagBase& tag) const noexcept
  {
    std::lock_guard<std::mutex> lock{job_mut_};
    return std::accumulate(
      neighbors_.cbegin(),
      neighbors_.cend(),
      0,
      [&](const int sum, const auto& neighbor_pair) noexcept {
        return sum + neighbor_pair.second.is_subscribed_to(tag.id());
      }
    );
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
    gsl::span<PublishValueVariant> value
  ) noexcept
  {
    const auto msg = internal::make_publish(version, tag_id, value);
    (void)msg;
    SKYNET_TRACE_LOG(
      "\"{}\" publishing on tag \"{}\", version \"{}\", data {}",
      id_,
      tag_id,
      version,
      value
    );
    send_to_neighbors_if(msg, [&](const auto& neighbor) {
      return neighbor.is_subscribed_to(tag_id);
    });
  }

  bool Master::add_data_to_queue(const internal::PublishData& msg) noexcept
  {
    for (auto& [name, job] : jobs_)
    {
      (void)name;
      auto msg_var = msg.value();
      if (!msg_var) { return false; }
      if (!Job::Accessor::process_data(job, msg.tag_id(), *msg_var, msg.version()))
      {
        return false;
      }
    }
    return true;
  }

  void Master::notify_of_new_neighbor(const MachineID& id) noexcept
  {
    send_to_neighbors_if(internal::make_new_neighbor(id), [&](const internal::ExternalMaster& neighbor) {
      return neighbor.id() != id;
    });
  }

  void Master::remove_dead_neighbors() noexcept
  {
    bool new_tags = false;
    for (auto it = neighbors_.begin(); it != neighbors_.end(); /* nothing */)
    {
      if (it->second.is_dead())
      {
        SKYNET_TRACE_LOG("\"{}\" removing dead neighbor \"{}\"", id_, it->first);
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
              SKYNET_TRACE_LOG("\"{}\" reporting disconnection in reduce group \"{}\"", id_, tag);
              internal::ReduceGroupBase::Accessor::report_disconnection(*info.group);
            }
          }
        }
        // Remove corresponding address
        const auto erase_addr = [&](auto& erase_from, const auto& on_erase) {
          const auto addr_matches = [&](const auto& pair) {
            return pair.second == std::addressof(it->second);
          };
          const auto find_next = [&](const auto& iter) {
            return std::find_if(iter, erase_from.end(), addr_matches);
          };
          for (auto iter = find_next(erase_from.begin()); iter != erase_from.end(); iter = find_next(iter))
          {
            on_erase(*iter);
            iter = erase_from.erase(iter);
          }
        };
        erase_addr(addr_to_machine_, [](const auto&){});
        // Need to re-look for the subscription tags, if any
        erase_addr(tag_to_machine_, [&](const auto& tag_pair) {
          new_tags = true;
          for (auto& job_pair : jobs_)
          {
            Job::Accessor::report_dead_tag(job_pair.second, tag_pair.first);
          }
          pending_tags_.emplace_back(tag_pair.first);
        });
        it = neighbors_.erase(it);
      }
      else
      {
        ++it;
      }
    }
    // Do this after removing the neighbors so that the dead neighbors won't be considered
    if (new_tags)
    {
      find_publishers_for_pending_tags();
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

  void Master::send_to_neighbors(const std::vector<std::byte>& to_send) noexcept
  {
    send_to_neighbors_if(to_send, [](const internal::ExternalMaster&) { return true; });
  }

  bool Master::subscribe_is_done(const std::vector<TagID>& required_tags) const noexcept
  {
    for (const auto& tag : required_tags)
    {
      const auto iter = tag_to_machine_.find(tag);
      if (iter == tag_to_machine_.cend()) { return false; }
    }
    SKYNET_TRACE_LOG("\"{}\" subscription for tags {} finished.", id_, required_tags);
    return true;
  }

  auto Master::subscribe(const std::vector<TagID>& tag_ids) noexcept
    -> Waiter<internal::MasterSubscribeIsDone, internal::WaiterGetNoOp>
  {
    SKYNET_TRACE_LOG("\"{}\" initializing subscription for tags {}", id_, tag_ids);
    std::copy(tag_ids.cbegin(), tag_ids.cend(), std::back_inserter(pending_tags_));
    for (auto& [name, neighbor] : neighbors_)
    {
      neighbor.reset_backoff_counter();
      neighbor.find_publishers_for_tags(tag_ids);
    }
    return internal::make_waiter(
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
      // sent back so that the receiving end no longer thinks they are pending
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
      // Mark the information as needing to be propagated
      for (const auto& tag : remaining_tags)
      {
        auto [iter, dummy] = send_publisher_information_to_.try_emplace(tag);
        iter->second.emplace(from.id());
        (void)dummy;
      }
      // If there's already a pending request another one can't be sent, so
      // just return now
      if (from.has_pending_tag_request())
      {
        SKYNET_TRACE_LOG(
          "\"{}\" returning early for request for tags {} from \"{}\" to avoid potential deadlock",
          id_,
          msg.tags(),
          from.id()
        );
        from.send_message(make_known_tag_publisher_message());
        // No longer need to propagate information to this neighbor, as it
        // is being sent now
        send_publisher_information_to_.erase(from.id());
      }
      else
      {
        SKYNET_TRACE_LOG(
          "\"{}\" asking neighbors {} for tags {} for \"{}\"",
          id_,
          make_neighbor_vector(),
          msg.tags(),
          from.id()
        );
        for (auto& neighbor : neighbors_)
        {
          if (&neighbor.second != &from)
          {
            neighbor.second.reset_backoff_counter();
            neighbor.second.find_publishers_for_tags(remaining_tags);
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

  void Master::add_publishers_and_propagate(
    const internal::ReportPublishers& msg,
    const internal::ExternalMaster& from
  ) noexcept
  {
    const auto tags = msg.tags();
    const auto publishers_list = msg.addresses();
    if (tags.size() != publishers_list.size())
    {
      SKYNET_WARN_LOG("\"{}\" received tag/publisher list size mismatch from \"{}\"", id_, from.id());
      return;
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
      if (loc == publishers_for_tag_.end())
      {
        publishers_for_tag_.emplace(
          tag,
          std::initializer_list<std::string>{from.address()}
        );
      }
      else
      {
        loc->second.insert(from.address());
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
    init_connections_for_pending_tags();
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
    std::unique_ptr<internal::ReduceGroupBase> group_ptr
  ) noexcept
    -> Waiter<internal::MasterReduceGroupIsCreated, internal::MasterGetReduceGroup>
  {
    const auto& tag_produced = internal::ReduceGroupBase::Accessor::produced_tag(*group_ptr);
    const auto& group_id = internal::ReduceGroupBase::Accessor::group_id(*group_ptr);
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
      std::move(group_ptr)
    );
    if (!inserted)
    {
      std::cerr
        << "The reduce group " << std::quoted(group_id) << " was attempted to be created twice!\n";
      std::terminate();
    }
    const auto& parent_tag = internal::ReduceGroupBase::Accessor::tag_neighbors(*iter->second.group).parent();
    if (!parent_tag.empty())
    {
      pending_tags_.push_back(parent_tag);
      for (auto& neighbor : neighbors_)
      {
        neighbor.second.reset_backoff_counter();
        neighbor.second.find_publishers_for_tags({parent_tag});
      }
    }
    return internal::make_waiter(
      job_mut_,
      reduce_group_cv_,
      internal::MasterReduceGroupIsCreated{*this, group_id},
      internal::MasterGetReduceGroup{*this, group_id}
    );
  }

  auto Master::rebuild_reduce_group(
    const TagID& group_id
  ) noexcept
    -> Waiter<internal::MasterReduceGroupIsCreated, internal::WaiterGetNoOp>
  {
    SKYNET_TRACE_LOG("\"{}\" rebuilding reduce group \"{}\"", id_, group_id);
    const auto iter = reduce_tag_data_.find(group_id);
    assert(iter != reduce_tag_data_.cend());
    const auto& parent_tag = internal::ReduceGroupBase::Accessor::tag_neighbors(*iter->second.group).parent();
    if (!parent_tag.empty())
    {
      // Don't bother searching for machines that already have connections
      if (iter->second.parent_machines.empty())
      {
        pending_tags_.push_back(parent_tag);
        for (auto& neighbor : neighbors_)
        {
          neighbor.second.reset_backoff_counter();
          neighbor.second.find_publishers_for_tags({parent_tag});
        }
      }
    }
    return internal::make_waiter(
      job_mut_,
      reduce_group_cv_,
      internal::MasterReduceGroupIsCreated{*this, group_id}
    );
  }

  bool Master::reduce_group_is_created(const TagID& group_id) noexcept
  {
    // See if the parent has a connection
    const auto group_iter = reduce_tag_data_.find(group_id);
    assert(group_iter != reduce_tag_data_.cend());
    const auto& reduce_data = group_iter->second;
    const auto& parent_tag = internal::ReduceGroupBase::Accessor::tag_neighbors(*reduce_data.group).parent();
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
      const auto& neighbors = internal::ReduceGroupBase::Accessor::tag_neighbors(*reduce_data.group);
      if (!neighbors.tags[i + 1].empty() && reduce_data.child_machines[i].empty())
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
      const auto& tag_neighbors = internal::ReduceGroupBase::Accessor::tag_neighbors(*reduce_group.group);
      if (msg.tag_produced() == tag_neighbors.tags[i + 1])
      {
        // Add it, unless it's already in there; that's an error
        auto& existing_conns = child_machines[i];
        const auto tag_loc = std::find(existing_conns.cbegin(), existing_conns.cend(), msg.tag_produced());
        if (tag_loc != existing_conns.cend())
        {
          SKYNET_WARN_LOG(
            "\"{}\" received join group from \"{}\" for tag \"{}\" for reduce group \"{}\", but it already existed in the group.",
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
      "\"{}\" received join group from \"{}\" for tag \"{}\" for reduce group \"{}\", but such a group does not exist.",
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
    return *loc->second.group;
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
    auto var_opt = value.value();
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
    return internal::ReduceGroupBase::Accessor::add_data(*group_loc->second.group, value.tag_id(), *var_opt, value.version());
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
          "for reduce group \"{}\" as the reduce group does not exist",
        id_,
        from.id(),
        msg.initiating_machine(),
        msg.reduce_tag()
      );
      return false;
    }
    internal::ReduceGroupBase::Accessor::propagate_disconnection(*group_loc->second.group, msg.initiating_machine(), msg.id());
    return true;
  }

  void Master::init_connections_for_pending_tags() noexcept
  {
    if (!pending_tags_.empty())
    {
      SKYNET_TRACE_LOG("\"{}\" is initiating connections for tags {}", id_, pending_tags_);
    }
    // A single connection can supply multiple tags, so look through all the pending tags
    // first so that multiple connections to the same machine aren't started
    std::unordered_map<std::string, std::string> to_conn;
    std::vector<decltype(pending_tags_)::iterator> to_delete;
    for (auto tag_iter = pending_tags_.begin(); tag_iter != pending_tags_.end();)
    {
      const auto& tag = *tag_iter;
      const auto iter = publishers_for_tag_.find(tag);
      if (iter == publishers_for_tag_.cend())
      {
        SKYNET_TRACE_LOG("\"{}\" know no publishers for tag {}", id_, tag);
        ++tag_iter;
        continue;
      }
      auto& publishers = iter->second;
      if (publishers.empty())
      {
        SKYNET_TRACE_LOG("\"{}\" know no publishers for tag {}", id_, tag);
        ++tag_iter;
      }
      else
      {
        const auto& addr = *publishers.begin();
        // Check if the machine is already a neighbor, and handle it if so
        const auto neighbor_iter = addr_to_machine_.find(internal::split_address(addr));
        if (neighbor_iter != addr_to_machine_.cend())
        {
          SKYNET_TRACE_LOG("\"{}\" already has connection for tag \"{}\"", id_, tag);
          assert(neighbor_iter->second);
          if (tag[0] == internal::publish_tag_marker)
          {
            finalize_subscription(
              tag,
              *neighbor_iter->second
            );
          }
          else
          {
            // Reduce group
            finalize_reduce_group(
              neighbor_iter->second->id(),
              group_from_parent_tag(tag).first
            );
          }
          tag_iter = pending_tags_.erase(tag_iter);
        }
        else
        {
          auto [conn_iter, inserted] = to_conn.try_emplace(addr, tag);
          // Append tag to "list" if already there
          if (!inserted)
          {
            auto& tag_list = conn_iter->second;
            if (!tag_list.empty())
            {
              tag_list.push_back('\0');
            }
            tag_list += tag;
          }
          to_delete.push_back(tag_iter);
        }
        publishers.erase(publishers.begin());
      }
    }
    for (const auto& [addr, tag] : to_conn)
    {
      internal::SocketCommunicator conn{};
      const auto err = conn.connect_non_blocking(addr);
      if (err == internal::ConnectionError::connection_in_progress
        || err == internal::ConnectionError::no_error)
      {
        // Port can be recycled, so have to iterate until it gets inserted
        // Ignore the address as the IP isn't initialized until the connection is complete
        auto [ignore, port] = conn.ip_address_and_port();
        (void)ignore;
        SKYNET_TRACE_LOG("\"{}\" connecting to \"{}\" for tag \"{}\"", id_, addr, tag);
        while (true)
        {
          const auto [iter, inserted] = pending_conns_.try_emplace(
            AddrPortPair{addr.substr(0, addr.find(':')), port},
            PendingInfo{
              std::move(conn),
              ConnStatus::waiting_for_conn,
              tag[0] == internal::publish_tag_marker ? ConnType::subscription : ConnType::reduce_group,
              tag
            }
          );
          (void)iter;
          if (inserted)
          {
            break;
          }
          ++port;
        }
      }
    }
    std::for_each(to_delete.rbegin(), to_delete.rend(), [&](const auto& iter) { pending_tags_.erase(iter); });
  }

  bool Master::conn_is_complete(const AddrPortPair& address) noexcept
  {
    return pending_conns_.find(address) == pending_conns_.cend();
  }

  bool Master::addr_is_connected(const AddrPortPair& address) const noexcept
  {
    const auto iter = addr_to_machine_.find(address);
    if (iter == addr_to_machine_.cend()) { return false; }
    return !iter->second->is_dead();
  }

  void Master::process_pending_conns() noexcept
  {
    bool new_pending_tags = false;
    // TODO: Move this into its own function?  It isn't used anywhere else...
    const auto handle_error = [&](PendingInfo& info) {
      const auto handle_tag = [&](const std::string& pub_tag, const std::string& base_tag) {
        new_pending_tags = true;
        const auto pub_iter = publishers_for_tag_.find(pub_tag);
        assert(pub_iter != publishers_for_tag_.cend());
        auto& publishers = pub_iter->second;
        // Set to ignore cache if there are no more publishers
        if (publishers.empty())
        {
          SKYNET_TRACE_LOG(
            "\"{}\" ran out of publishers for tag \"{}\", look for new ones.",
            id_,
            info.tag
          );
          for (auto&& neighbor : neighbors_)
          {
            neighbor.second.ignore_cache_on_next_request();
          }
        }
        else
        {
          SKYNET_TRACE_LOG(
            "\"{}\" still has publishers for tag \"{}\", going to next one",
            id_,
            info.tag
          );
        }
        // Just replace the tag to re-init the connection
        pending_tags_.emplace_back(std::string{base_tag});
      };
      switch (info.type)
      {
      case ConnType::by_accept:
      case ConnType::user_requested:
        // nothing special needs to happen
        break;

      case ConnType::subscription:
        {
          const auto tags = internal::split(info.tag, '\0');
          for (const auto& tag : tags)
          {
            const std::string tag_str{tag};
            handle_tag(tag_str, tag_str);
          }
        }
        break;

      case ConnType::reduce_group:
        {
          auto& [group_id, reduce_data] = group_from_parent_tag(info.tag);
          (void)group_id;
          const auto& parent_tag =
            internal::ReduceGroupBase::Accessor::tag_neighbors(*reduce_data.group).parent();
          handle_tag(parent_tag, info.tag);
        }
        break;
      }
    };
    for (auto iter = pending_conns_.begin(); iter != pending_conns_.end(); )
    {
      // I don't like this okay variable, but I can't think of a better way
      bool okay = true;
      auto& info = iter->second;
      if (info.status == ConnStatus::waiting_for_conn)
      {
        const auto init_status = info.conn.connection_progress_status();
        switch (init_status)
        {
        case internal::ConnectionError::connection_in_progress:
          break;

        case internal::ConnectionError::no_error:
          {
            SKYNET_TRACE_LOG(
              "\"{}\" sending greeting to {} for tag \"{}\"",
              id_,
              info.conn.ip_address_and_port(),
              info.tag
            );
            // Send message and mark as waiting
            const auto message = make_handshake();
            if (info.conn.send_message(message.data(), message.size()) != internal::ConnectionError::no_error)
            {
              notify_connection_ = true;
              iter = pending_conns_.erase(iter);
              continue;
            }
            info.status = ConnStatus::waiting_for_resp;
          }
          break;

        // Anything else is an error
        default:
          SKYNET_WARN_LOG(
            "\"{}\" errored trying to connect to {}",
            id_,
            iter->first
          );
          handle_error(info);
          notify_connection_ = true;
          iter = pending_conns_.erase(iter);
          okay = false;
          break;
        }
      }
      else if (info.status == ConnStatus::waiting_for_resp)
      {
        // TODO: Add timeout here?
        // Try to read message from the connection
        const auto bytes_to_read_or_error = read_network_size(info.conn);
        if (std::holds_alternative<internal::ConnectionError>(bytes_to_read_or_error))
        {
          const auto err = *std::get_if<internal::ConnectionError>(&bytes_to_read_or_error);
          if (err != internal::ConnectionError::would_block)
          {
            okay = false;
          }
        }
        else
        {
          const auto bytes_to_read = *std::get_if<NetworkSizeType>(&bytes_to_read_or_error);
          if (const auto message_buffer = internal::read_chunked(info.conn, bytes_to_read); !message_buffer.empty())
          {
            if (const auto msg = internal::MessageHandler::try_to_create(message_buffer))
            {
              decltype(neighbors_)::iterator new_neighbor_iter;
              okay &= msg->do_callback(
                [&](const internal::Greeting& greeting) {
                  // add connection to active list / remove from pending list
                  auto [neighbor_iter, inserted] = neighbors_.try_emplace(
                    greeting.from(),
                    std::move(info.conn),
                    greeting.from(),
                    greeting.neighbors(),
                    *this,
                    greeting.port()
                  );
                  new_neighbor_iter = neighbor_iter;
                  if (!inserted)
                  {
                    SKYNET_TRACE_LOG(
                      "\"{}\" rejected greeting from \"{}\" due to the name already being present",
                      id_,
                      neighbor_iter->first
                    );
                    return false;
                  }
                  addr_to_machine_.try_emplace(new_neighbor_iter->second.address_pair(), &neighbor_iter->second);
                  SKYNET_TRACE_LOG(
                    "\"{}\" received greeting from \"{}\"",
                    id_,
                    neighbor_iter->first
                  );
                  return true;
                },
                [&](...) {
                  SKYNET_WARN_LOG(
                    "\"{}\" received unexpected message from \"{}\", expected greeting",
                    id_,
                    iter->first
                  );
                  return false;
                }
              );
              if (okay)
              {
                SKYNET_TRACE_LOG("\"{}\" finalizing connection to \"{}\" for tag \"{}\"", id_, iter->first, info.tag);
                switch (info.type)
                {
                case ConnType::by_accept:
                case ConnType::user_requested:
                  break;

                case ConnType::reduce_group:
                  finalize_reduce_group(
                    new_neighbor_iter->first,
                    group_from_parent_tag(info.tag).first
                  );
                  break;

                case ConnType::subscription:
                  finalize_subscription(
                    info.tag,
                    new_neighbor_iter->second
                  );
                  break;
                }
                // These will always happen at the end
                notify_of_new_neighbor(new_neighbor_iter->first);
                find_publishers_for_pending_tags();
                // Finally, remove the pending connection and re-loop
                notify_connection_ = true;
                iter = pending_conns_.erase(iter);
                continue;
              }
            }
            else
            {
              okay = false;
            }
          }
        }
        if (!okay)
        {
          SKYNET_WARN_LOG(
            "\"{}\" failed connecting to {} for tag \"{}\"",
            id_,
            info.conn.ip_address_and_port(),
            info.tag
          );
          notify_connection_ = true;
          handle_error(info);
          iter = pending_conns_.erase(iter);
        }
      }
      if (okay)
      {
        ++iter;
      }
    }
    // Find publishers if there are new pending tags
    if (new_pending_tags)
    {
      find_publishers_for_pending_tags();
      init_connections_for_pending_tags();
    }
  }

  std::vector<std::byte> Master::make_handshake() const noexcept
  {
    return internal::make_greeting(id_, make_neighbor_vector(), port_);
  }

  void Master::finalize_reduce_group(const MachineID& parent_machine_id, const TagID& group_tag) noexcept
  {
    const auto iter = reduce_tag_data_.find(group_tag);
    assert(iter != reduce_tag_data_.cend());
    auto& group = *iter->second.group;
    const auto& tag_produced = internal::ReduceGroupBase::Accessor::produced_tag(group);
    const auto& parent_id = iter->second.parent_machines.emplace_back(parent_machine_id);
    const auto neighbor_iter = neighbors_.find(parent_id);
    assert(neighbor_iter != neighbors_.cend());
    neighbor_iter->second.send_message(internal::make_join_reduce_group(group_tag, tag_produced));
    notify_reduce_group_ = true;
  }

  bool Master::subscription_tags_are_produced(const internal::SubscriptionNotice& msg) const noexcept
  {
    const auto& tags = msg.tags();
    for (const auto& tag : tags)
    {
      if (produced_tags_.find(tag) == produced_tags_.cend())
      {
        return false;
      }
    }
    return true;
  }

  bool Master::handle_publish_data(
    const internal::PublishData& msg,
    const internal::ExternalMaster& from
  ) noexcept
  {
    (void)from;
    if (auto value = msg.value())
    {
      SKYNET_TRACE_LOG(
        "\"{}\" received data on tag \"{}\" from \"{}\", version {}, data: {}",
        id_,
        msg.tag_id(),
        from.id(),
        msg.version(),
        *value
      );
      bool okay = true;
      for (auto& [job_id, job] : jobs_)
      {
        (void)job_id;
        okay &= Job::Accessor::process_data(
          job,
          msg.tag_id(),
          *value,
          msg.version()
        );
      }
      return okay;
    }
    else
    {
      return false;
    }
  }

  void Master::finalize_subscription(const std::string& tags, internal::ExternalMaster& source) noexcept
  {
    const auto tags_str_view = internal::split(tags, '\0');
    SKYNET_TRACE_LOG(
      "\"{}\" finalizing subscription for tags {} with machine {}",
      id_,
      tags_str_view,
      source.id()
    );
    std::vector<TagID> tags_to_sub_to;
    std::transform(
      tags_str_view.cbegin(),
      tags_str_view.cend(),
      std::back_inserter(tags_to_sub_to),
      [](const std::string_view v) { return std::string{v}; }
    );
    for (const auto& tag : tags_to_sub_to)
    {
      tag_to_machine_[tag] = &source;
    }
    const auto msg = internal::make_subscription_notice(tags_to_sub_to, false);
    source.send_message(msg);
    notify_new_subscriptions_ = true;
  }

  void Master::find_publishers_for_pending_tags() noexcept
  {
    const auto copy_criteria = [&](const TagID& tag) noexcept {
      if (tag_to_machine_.find(tag) != tag_to_machine_.cend()) { return false; }
      const auto iter = publishers_for_tag_.find(tag);
      if (iter == publishers_for_tag_.cend()) { return true; }
      return iter->second.empty();
    };
    std::vector<TagID> to_ask_for;
    std::copy_if(
      pending_tags_.cbegin(),
      pending_tags_.cend(),
      std::back_inserter(to_ask_for),
      copy_criteria
    );
    if (!to_ask_for.empty())
    {
      for (auto& neighbor : neighbors_)
      {
        neighbor.second.reset_backoff_counter();
        neighbor.second.find_publishers_for_tags(to_ask_for);
      }
    }
  }
} // namespace skynet

#include "skynet/master.hpp"

#include "spdlog/spdlog.h"
      #include <iostream>
      #include <iomanip>

// TODO: Support other types of communicators

// Macro to wrap logging since I don't know if we're doing runtime or what and this
// can easily be searched for or changed later on
#define TRACE_LOG(...) SPDLOG_TRACE(__VA_ARGS__)

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

    void ExternalMaster::find_publishers_for_tags(
      const std::vector<TagID>& tags,
      const bool ignore_cache
    ) noexcept
    {
      bool work_to_do = false;
      for (const auto& tag_id : tags)
      {
        // The tag is not pending, mark this as not needing to be propagated
        // and ask the neighbor for details
        if (pending_tags_.find(tag_id) == pending_tags_.cend())
        {
          pending_tags_.emplace(tag_id);
          work_to_do = true;
        }
      }
      if (work_to_do)
      {
        send_message(make_get_publishers(tags, ignore_cache));
      }
      else
      {
        // std::stringstream ss;
        // ss << "Request to find publishers for " << tags[0] << " from " << master_->id() << " ignored locally by " << id_ << '\n';
        // std::cerr << ss.str();
      }
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
      // std::stringstream ss;
      // ss << master_->id() << " sending greeting to " << conn_.ip_address_and_port().second << '\n';
      // std::cerr << ss.str();
      return !dead_;
    }

    // Wait until the greeting is sent
    bool ExternalMaster::wait_for_greeting() noexcept
    {
      // Wait for the greeting
      // TODO: Probably want a time-out?
      // std::stringstream ss;
      // ss << master_->id() << " waiting for greeting from " << conn_.ip_address_and_port().second << '\n';
      // std::cerr << ss.str();
      while (!dead_)
      {
        if (auto handle = try_to_get_status_message())
        {
          return handle->do_callback(
            [&](const Greeting& msg) {
              neighbors_ = msg.neighbors();
              id_ = msg.from();
              base_port_ = msg.base_port();
              // std::stringstream ss;
              // ss << master_->id() << " got greeting from " << conn_.ip_address_and_port().second << '\n';
              // std::cerr << ss.str();
              return true;
            },
            // Any other kind of message is an error
            [&](...) { return false; }
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
        [&](const ReportPublishers& msg) {
          // Remove any produced tags that were marked as pending
          for (const auto& tag_list : {msg.tags(), msg.locally_produced_tags()})
          {
            for (const auto& tag : tag_list)
            {
              pending_tags_.erase(tag);
            }
          }
          Master::ExternalMasterAccessor::add_publishers_and_propagate(*master_, msg, *this);
          return true;
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
    , pub_channel_{static_cast<std::uint16_t>(port + publisher_port_offset)}
    , comm_port_{port}
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
    // std::stringstream ss;
    // ss << id_ << " connecting to " << port << '\n';
    // std::cerr << ss.str();
    if (to_connect.connect_to_server(address, port) != internal::ConnectionError::no_error)
    {
      // std::stringstream ss;
      // ss << id_ << " failed to connect to " << port << '\n';
      // std::cerr << ss.str();
      // internal::on_error("Master::connect_to_server failed!");
      return false;
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
        // std::stringstream ss;
        // ss << id_ << " rejected " << port << " due to already existing neighbor.\n";
        // std::cerr << ss.str();
        return false;
      }
      notify_of_new_neighbor(new_id);
      neighbors_.emplace(new_id, std::move(*new_neighbor));
      return true;
    }
    else
    {
      return false;
    }
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
    const auto res = jobs_.try_emplace(
      name,
      Job::Accessor::AllowConstruction{},
      name,
      *this,
      std::move(tags_produced),
      std::move(to_run)
    );
    if (res.second)
    {
      // Mark the tags produced by this job
      for (const auto& tag : res.first->second.tags_produced())
      {
        const auto [iter, inserted] = produced_tags_.insert(tag);
        (void)iter;
        if (!inserted)
        {
          // Two jobs on the same master can't produce the same job; fail loudly
          std::cerr << "Two jobs are attempting to publish on the tag " << std::quoted(tag) << '\n';
          std::terminate();
        }
      }
    }
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
      // Remove any finished jobs
      for (auto iter = jobs_.cbegin(); iter != jobs_.cend(); )
      {
        if (iter->second.is_finished())
        {
          iter = jobs_.erase(iter);
        }
        else
        {
          ++iter;
        }
      }
      {
        // Ensure there's no data race with jobs
        std::unique_lock lock{job_mut_};
        accept_pending_connections();
        // Handle any subscription requests
        pub_channel_.accept_subscriptions();
        // Read data from subscriptions
        read_data_from_subscriptions();
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
    return pub_channel_.num_subscriptions();
  }

  const std::string& Master::id() const noexcept
  {
    return id_;
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

  void Master::publish(
    const VersionID version,
    const TagID& tag_id,
    const PublishValueVariant& value
  ) noexcept
  {
    const auto msg = internal::make_publish(version, tag_id, value);
    std::stringstream ss;
    // ss << id_ << " publishing on " << tag_id << " to " << num_subscribers() << " subscribers\n";
    pub_channel_.send_message(msg.data(), msg.size());
    // ss << '\t' << id_ << " after publishing on " << tag_id << ", has " << num_subscribers() << " subscribers\n";
    // std::cerr << ss.str();
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
    // std::cerr << id_ << " knows:\n";
    // for (const auto [tag, data] : publishers_for_tag_)
    // {
    //   std::cerr << '\t' << tag << "\n\t\t";
    //   std::copy(data.cbegin(), data.cend(), std::ostream_iterator<std::string>(std::cerr, " "));
    //   std::cerr << '\n';
    // }
    // std::stringstream sstr;
    // sstr << id_ << " looking for tags: ";
    // for (const auto& tag : tag_ids) { sstr << tag << ", "; }
    // sstr << '\n';
    // std::cerr << sstr.str();
    std::vector<TagID> tags_to_find;
    bool ignore_cache = false;
    for (const auto tag_id : tag_ids)
    {
      // Check if already subscribed, skip if so
      const bool already_subscribed = [this, &tag_id]() {
        for (const auto& sub : subscriptions_)
        {
          auto& prod_tags = sub.produced_tags;
          if (std::find(prod_tags.cbegin(), prod_tags.cend(), tag_id) != prod_tags.cend())
          {
            return true;
          }
        }
        return false;
      }();
      if (already_subscribed) { continue; }
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
      // Check if there is a list of known producers for the tag
      const auto loc = publishers_for_tag_.find(tag_id);
      // Create the entry if required
      if (loc == publishers_for_tag_.end())
      {
        // std::stringstream ss;
        // ss << id_ << " failed to find publishers in the cache for " << tag_id << '\n';
        // std::cerr << ss.str();
        publishers_for_tag_.try_emplace(tag_id);
        tags_to_find.push_back(tag_id);
      }
      else
      {
        auto& publishers = loc->second;
        // Now, if there are any known subscriptions, try to subscribe to them
        if (!publishers.empty())
        {
          for (auto it = publishers.begin(); it != publishers.end(); )
          {
            const auto& subscribe_address = *it;
            if (auto sub = internal::Subscription::try_to_create(subscribe_address))
            {
              subscriptions_.push_back(SubscriptionData{
                std::move(*sub),
                get_tags_for_publisher(subscribe_address)
              });
              break;
            }
            else
            {
              // std::stringstream ss;
              // ss << id_ << " failed to subscribe to " << subscribe_address << '\n';
              // std::cerr << ss.str();
              // Couldn't subscribe - remove this as a producer
              it = publishers.erase(it);
            }
          }
        }
        // Check if the producers are now empty (can happen if all known publishers
        // fail to be subscribed to)
        if (publishers.empty())
        {
          // std::stringstream ss;
          // ss << id_ << " looking for new publishers for " << tag_id << '\n';
          // std::cerr << ss.str();
          // Need to get original producers for tags now, so have to ignore caches
          ignore_cache = true;
          // Couldn't do this subscription; need to look for a producer for the tag
          tags_to_find.push_back(tag_id);
        }
      }
    }
    // Find producers for any tags that need it
    if (!tags_to_find.empty())
    {
      for (auto& neighbor : neighbors_)
      {
        neighbor.second.find_publishers_for_tags(tags_to_find, ignore_cache);
      }
    }
    // Nothing to find - successfully subscribed
    return tags_to_find.empty();
  }

  void Master::handle_get_publishers(
    const internal::GetPublishers& msg,
    internal::ExternalMaster& from
  ) noexcept
  {
    // std::stringstream ss;
    // ss << id_ << " recieved get_publishers from " << from.id() << '\n';
    // std::cerr << ss.str();
    // If all of the tag requirements are fulfilled then
    const auto remaining_tags = remove_tags_with_known_publishers(msg);
    if (remaining_tags.empty())
    {
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
        from.send_message(make_known_tag_publisher_message());
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
        from.send_message(make_known_tag_publisher_message());
      }
      else
      {
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

  void Master::add_publishers_and_propagate(
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
      // std::stringstream ss;
      // ss << id_ << " recieved tag/publisher list size mismatch from " << from.id() << '\n';
      // std::cerr << ss.str();
      return;
    }
    // std::stringstream ss;
    // ss << id_ << " recieved ReportPublishers from " << from.id() << '\n';
    // Add the information to what is locally known
    for (std::size_t i = 0; i < tags.size(); ++i)
    {
      const auto& tag = tags[i];
      const auto& publishers = publishers_list[i];
      // ss << '\t' << tag << ": ";
      // for (const auto& p : publishers) { ss << p << ", "; }
      // ss << '\n';
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
    // ss << "\tlocal tags: ";
    // for (const auto& tag : msg.locally_produced_tags())
    // {
    //   ss << tag << ", ";
    // }
    // ss << '\n';
    // std::cerr << ss.str();
    // Add the tags that the external master produced
    const auto external_tags = msg.locally_produced_tags();
    for (const auto& tag : external_tags)
    {
      const auto loc = publishers_for_tag_.find(tag);
      if (loc == publishers_for_tag_.end())
      {
        publishers_for_tag_.emplace(
          tag,
          std::initializer_list<std::string>{from.publisher_address()}
        );
      }
      else
      {
        loc->second.insert(from.publisher_address());
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
        machines_to_send_to.merge(loc->second);
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
        // Intentionally send tag data with no known publishers to mark that
        // the recieving end should no longer wait for those tags to appear
        tags_to_send.push_back(tag);
        addresses_to_send.emplace_back(addresses.cbegin(), addresses.cend());
      }
      const std::vector<TagID> local_tags(produced_tags_.cbegin(), produced_tags_.cend());
      const auto to_send = internal::make_report_publishers(tags_to_send, addresses_to_send, local_tags);
      // Send to the machines if they are present
      for (const auto& send_to : machines_to_send_to)
      {
        const auto loc = neighbors_.find(send_to);
        if (loc != neighbors_.end())
        {
          loc->second.send_message(to_send);
        }
      }
    }
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
      subscriptions_.emplace_back(SubscriptionData{std::move(*sub), std::move(remote_tags_produced)});
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
        // Then read the actual message and parse it
        if (const auto message_buffer = sub.read_chunked(bytes_to_read); !message_buffer.empty())
        {
          if (const auto msg = internal::PublishMessageHandler::try_to_create(message_buffer))
          {
            if (const auto data = msg->data())
            {
              if (const auto variant = data->value().get_variant())
              {
                // std::stringstream ss; ss << id_ << " read data for tag " << data->tag_id() << '\n';
                // std::cerr << ss.str();
                for (auto& [job_id, job] : jobs_)
                {
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
} // namespace skynet

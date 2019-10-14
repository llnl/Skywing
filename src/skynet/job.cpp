#include "skynet/job.hpp"

#include "skynet/master.hpp"

#include <iomanip>
#include <iostream>

namespace skynet
{
  Job::Job(const JobID& id, Master& master, std::function<void(Job&)> to_run) noexcept
    : id_{id}
    , master_{&master}
    , to_run_{std::move(to_run)}
  {
    Master::Accessor::add_job(*master_, id_, *this);
  }

  bool Job::is_finished() const noexcept
  {
    return is_finished_;
  }

  /** \brief Processes the raw information sent from a job on another instance
   *
   * \param tag The id of the tag the data was sent with
   * \param data The data sent on the tag
   * \return True if processing went fine, false if there was an error
   */
  bool Job::process_data(const TagID& tag_id, BroadcastDataVariant data) noexcept
  {
    auto buf_pair = bufs_.get();
    auto& buf = buf_pair.first;
    auto& expected_types = buf.expected_types_;
    auto& values = buf.values_;
    const auto loc = expected_types.find(tag_id);
    // Not subscribed; don't do anything, but not an error
    if (loc == expected_types.cend()) { return true; }
    // If the type is wrong then something went wrong
    if (data.index() != loc->second) { return false; }
    // Otherwise just make it the current value
    values.insert_or_assign(tag_id, std::move(data));
    return true;
  }

  /** \brief Broadcasts a value on a tag to all nodes in the network
   */
  void Job::global_broadcast(const TagID& tag_id, BroadcastDataVariant to_send) noexcept
  {
    Master::Accessor::broadcast(
      *master_,
      message_id_,
      tag_id,
      0,
      std::move(to_send)
    );
    ++message_id_;
  }

  /** \brief Broadcasts a value on a tag to all neighbors
   */
  void Job::local_broadcast(const TagID& tag_id, BroadcastDataVariant to_send) noexcept
  {
    Master::Accessor::broadcast(
      *master_,
      message_id_,
      tag_id,
      1,
      std::move(to_send)
    );
    ++message_id_;
  }

  // Implementation of public functions
  std::optional<BroadcastDataVariant> Job::get_impl(const TagID& tag_id) noexcept
  {
    auto buf_pair = bufs_.get();
    auto& buf = buf_pair.first;
    auto& values = buf.values_;
    // Then check if there's been anything seen on the tag and return it if so
    if (const auto loc = values.find(tag_id); loc != values.cend())
    {
      return loc->second;
    }
    return {};
  }

  bool Job::has_data_impl(const TagID& tag_id) noexcept
  {
    auto buf_pair = bufs_.get();
    auto& buf = buf_pair.first;
    auto& values = buf.values_;
    // If there's anything in the values_ map then there's data
    return (values.find(tag_id) != values.cend());
  }

  void Job::subscribe_impl(const TagID& tag_id, std::uint8_t expected_type) noexcept
  {
    auto buf_pair = bufs_.get();
    auto& buf = buf_pair.first;
    auto& expected_types = buf.expected_types_;
    // Already subscribed - hard error
    if (expected_types.find(tag_id) != expected_types.cend())
    {
      std::cerr << "Job " << std::quoted(id_) << " subscribed to tag " << std::quoted(tag_id) << " after already being subscribed to it.\n";
      std::terminate();
    }
    // Then add the expected type; marking the tag as watched
    expected_types.try_emplace(
      tag_id,
      expected_type
    );
  }

  void Job::unsubscribe_impl(const TagID& tag_id) noexcept
  {
    auto buf_pair = bufs_.get();
    auto& buf = buf_pair.first;
    // Just remove any the expected types and data maps
    buf.expected_types_.erase(tag_id);
    buf.values_.erase(tag_id);
  }
} // namespace skynet

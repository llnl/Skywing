#ifndef SKYNET_INTERNAL_MESSAGE_CREATORS_HPP
#define SKYNET_INTERNAL_MESSAGE_CREATORS_HPP

#include "skynet/types.hpp"

#include <cstddef>
#include <vector>

namespace skynet::internal
{
  /** \brief Create data for a publish
   */
  std::vector<std::byte> make_publish(
    const VersionID message_id,
    const TagID& tag_id,
    const MachineID& origin,
    const std::uint8_t hops_left_p1,
    const PublishValueVariant& data
  ) noexcept;

  /** \brief Create data to signify that a publication channel is closing
   */
  std::vector<std::byte> make_close_publish() noexcept;

  /** \brief Create data for a greeting
   */
  std::vector<std::byte> make_greeting(
    const MachineID& from,
    const std::vector<MachineID>& neighbors
  ) noexcept;

  /** \brief Create data for a goodbyte
   */
  std::vector<std::byte> make_goodbye() noexcept;

  /** \brief Create data for a new neighbor notification
   */
  std::vector<std::byte> make_new_neighbor(const MachineID& neighbor) noexcept;

  /** \brief Create data for a removed neighbor notification
   */
  std::vector<std::byte> make_remove_neighbor(const MachineID& neighbor) noexcept;

  /** \brief Create data for a heartbeat
   */
  std::vector<std::byte> make_heartbeat() noexcept;

  /** \brief Create data for returning information on tag publishers
   *
   * TODO: This can be made more efficient by directly iterating over the map
   * and just grabbing the information from there; not sure how to make it
   * not horribly ugly though, so put it off for now.
   */
  std::vector<std::byte> make_tag_publishers(
    const std::vector<TagID>& tags,
    const std::vector<std::vector<std::string>>& addresses,
    const std::vector<TagID>& locally_produced_tags
  ) noexcept;

  /** \brief Create data for a request for producers of a tag
   */
  std::vector<std::byte> make_get_publishers(
    const std::vector<TagID>& tags
  ) noexcept;
} // namespace skynet::internal

#endif // SKYNET_INTERNAL_MESSAGE_CREATORS_HPP

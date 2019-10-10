#ifndef SKYNET_INTERNAL_MESSAGE_CREATORS_HPP
#define SKYNET_INTERNAL_MESSAGE_CREATORS_HPP

#include "skynet/types.hpp"

#include <cstddef>
#include <vector>

namespace skynet::internal
{
  /** \brief Create data for a broadcast
   */
  std::vector<std::byte> make_broadcast(
    const MessageID message_id,
    const TagID& tag_id,
    const MachineID& origin,
    const std::uint8_t hops_left_p1,
    const BroadcastDataVariant& data
  ) noexcept;

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
} // namespace skynet::internal

#endif // SKYNET_INTERNAL_MESSAGE_CREATORS_HPP

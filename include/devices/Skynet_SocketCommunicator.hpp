#ifndef SKYNET_SOCKETCOMMUNICATOR_HPP__
#define SKYNET_SOCKETCOMMUNICATOR_HPP__

#include "Skynet_DeviceCommunicator.hpp"
#include <cstdint>

namespace skynet
{
  class SocketCommunicator : public DeviceCommunicator
  {
  public:

    SocketCommunicator(uint16_t port, int type);

    SocketCommunicator(const char * ip_address, uint16_t port, int type);

    const bool success() const
    { return success_; }

  private:

    void do_send_to_(const void* data, std::size_t data_size) const override;

    std::vector<char> do_receive_from_() const override;

    bool success_;
    int sockfd_;

  }; // class SocketCommunicator
} // namespace skynet

#endif /* SKYNET_MPICOMMUNICATOR_HPP__ */

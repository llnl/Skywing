#ifndef SKYNET_SOCKETGATEKEEPER_HPP__
#define SKYNET_SOCKETGATEKEEPER_HPP__

#include <arpa/inet.h>
#include "Skynet_DeviceReference.hpp"
#include "Skynet_Gatekeeper.hpp"
#include "Skynet_SocketCommunicatorFactory.hpp"
#include "Skynet_SocketDeviceListener.hpp"

namespace skynet
{
  /** \class SocketGatekeeper
   * \brief Object responsible for creating and maintaining a gatekeeper
   * SocketCommunicator.
   *
   */
  class SocketGatekeeper : public Gatekeeper
  {
  public:

    /** \brief Construct a new SocketGatekeeper.
     *
     * \param type Specifies the address type to be used (IPv4).
     * \param skynet_port The specific port that all SocketGatekeepers will be
     * listening on
     */
    SocketGatekeeper(int type, uint16_t skynet_port) :
      type_(type), skynet_port_(skynet_port)
    {
      gatekeeper_ = std::make_unique<SocketDeviceListener>(type, skynet_port, false);
    }

    /** \brief Collect all connection requests that are in the gatekeeper queue.
     *
     * \return Vector of SocketCommunicatorFactory objects, one for each
     * connection request in the gatekeeper queue
     */
    std::vector<DeviceReference> collect_new_connections()
    {
      int count;
      std::vector<DeviceReference> new_factories;
      std::unique_ptr<CommunicatorFactory> factory;
      std::unique_ptr<DeviceCommunicator> handshake;
      do
      {
        count = gatekeeper_->count_pending_clients();
        if (count > 0)
        {
          // create a handshake SocketCommunicator to accept the next client
          handshake = gatekeeper_->connect_communicator_to_client();
          // create a new listening socket
          SocketDeviceListener gateway(type_, skynet_port_+1, true);
          // create new server SocketCommunicatorFactory with bound gateway socket
          factory = std::make_unique<SocketCommunicatorFactory>(type_, std::move(gateway));
          // create DeviceReference using new server SocketCommunicatorFactory
          new_factories.push_back(DeviceReference(std::move(factory)));
          // inform client of port the server SocketCommunicatorFactory is using
          handshake->send_to<uint16_t>(gateway.get_port());
        }
      }while (count > 0);
      return new_factories;
    }

  private:

    int type_;
    uint16_t skynet_port_;

  }; // class SocketGateKeeper
} // namespace ns3

#endif /* SKYNET_SOCKETGATEKEEPER_HPP__ */

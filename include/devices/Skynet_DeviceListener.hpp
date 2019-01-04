#ifndef SKYNET_DEVICELISTENER_HPP__
#define SKYNET_DEVICELISTENER_HPP__

#include "Skynet_DeviceCommunicator.hpp"

namespace skynet
{

  /** \class DeviceListener
   * \brief Abstract class, DeviceListener to listen for new client Devices.
   *
   */
  class DeviceListener
  {
  public:

    /** \brief Count number of client Devices requesting to connect
     *
     * \return number of clients
     */
    virtual int count_pending_clients() = 0;

    /** \brief Connect a communicator in a pending Device
     *
     * \return a connect DeviceCommunicator
     */
    virtual std::unique_ptr<DeviceCommunicator> connect_communicator_to_client() = 0;

  }; // class DeviceListener
} // namespace skynet

#endif /* SKYNET_DEVICELISTENER_HPP__ */

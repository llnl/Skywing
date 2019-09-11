#ifndef SKYNET_COMMUNICATORFACTORY_HPP__
#define SKYNET_COMMUNICATORFACTORY_HPP__

#include <vector>
#include <string>
#include <memory>
#include "Skynet_DeviceCommunicator.hpp"

namespace skynet
{

  /** \class CommunicatorFactory
   * \brief Abstract class, factory for creating DeviceCommunicators.
   *
   * Every DeviceReference object will hold one of these. When a new
   * communication channel between this device and that referred to by
   * a DeviceReference is needed (e.g. because a new SkynetJob is
   * created), the CommunicatorFactory object held by that
   * DeviceReference is called upon to create a new
   * DeviceCommunicator. At this time, any needed handshaking occurs
   * (e.g. creating a new socket through which to communicate) is
   * done, and whoever asked for the new point-to-point communication
   * channel receives it.
   */
  class CommunicatorFactory
  {
  public:

    /** \brief Create a new DeviceCommunicator, blocks until this can be done.
     *
     * \param comm_config_info Configuration info for this new communicator.
     */
    virtual std::unique_ptr<DeviceCommunicator>
      create_new_communicator(const std::vector<std::string>& comm_config_info) = 0;

    /** \brief Creates a communicator if one is requested
     *
     * \return A DeviceCommunicator if one was requested, nullptr otherwise
     */
    virtual std::unique_ptr<DeviceCommunicator> create_requested_communicator() = 0;

    virtual ~CommunicatorFactory() = default;

  }; // class CommunicatorFactory
} // namespace skynet

#endif /* SKYNET_COMMUNICATORFACTORY_HPP__ */

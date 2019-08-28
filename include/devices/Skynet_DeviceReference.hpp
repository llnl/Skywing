#ifndef SKYNET_DEVICEREFERENCE_HPP__
#define SKYNET_DEVICEREFERENCE_HPP__

#include <memory>
#include <cstddef>
#include <type_traits>
#include <vector>
#include <string>
#include "Skynet_DeviceCommunicator.hpp"
#include "Skynet_CommunicatorFactory.hpp"

namespace skynet
{
  /** \class DeviceReference

      A DeviceReference object represents some other participating
      device in the Skynet instance. This object contains information
      about the device such as how to communicate with it and its
      computational capabilities.
  */
  class DeviceReference
  {

  public:
    //Some types that will be used by DeviceReference
    //NOTE: For the == operator to work below, id_t must be a type for
    // which comparison is defined.
    using id_t = unsigned int; //Device ID type


  public:
    // using id_t = unsigned int;

  public:
    /** \brief Construct a new \c DeviceReference.
     *
     * \param comm_factory A CommuncatorFactory representing this
     * DeviceReference's communications policy.
     */
    // Is id_t expect to change to a heavier weight type?  It's an
    // integer, but it's currently being moved from.
    DeviceReference(id_t device_id,
		    std::unique_ptr<CommunicatorFactory> comm_factory)
      : is_believed_live_(true), device_id_(std::move(device_id)),
        comm_factory_(std::move(comm_factory))
    { }

    /** \brief Get if we believe the referred device to be live. */
    bool get_is_believed_live() const
    { return is_believed_live_; }

    /** \brief Update is_believed_live when new information is gained
     *   about the device status
     */
    void set_is_believed_live(const bool status)
    {
      is_believed_live_ = status;
    }

    /** Get device ID */
    id_t get_id() const
    {
      return device_id_;
    }

    bool operator ==(const DeviceReference& other_device) const
    { return device_id_ == other_device.get_id(); }

    /** \brief Request a new DeviceCommunicator for this DeviceReference.
     * \return A new DeviceCommunicator. */
    std::unique_ptr<DeviceCommunicator> create_new_communicator()
    {
      return comm_factory_->create_new_communicator(comm_config_info_);
    }

  private:
    bool is_believed_live_;
    const id_t device_id_;
    std::unique_ptr<CommunicatorFactory> comm_factory_;
    std::vector<std::string> comm_config_info_;

  }; // class DeviceReference
} // namespace skynet


#endif /* SKYNET_DEVICEREFERENCE_HPP__ */

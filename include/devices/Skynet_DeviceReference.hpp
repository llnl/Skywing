#ifndef SKYNET_DEVICEREFERENCE_HPP__
#define SKYNET_DEVICEREFERENCE_HPP__

#include <memory>
#include <cstddef>
#include <type_traits>
#include <vector>
#include <string>
#include "Skynet_DeviceCommunicator.hpp"
#include "Skynet_CommunicatorFactory.hpp"

namepsace skynet
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
    /** \brief Construct a new \c DeviceReference.
     *
     * \param comm A DeviceCommunicator representing this
     * DeviceReference's communications policy.
     */
    DeviceReference(std::unique_ptr<CommunicatorFactory> comm)
      : comm(comm_), is_believed_live_(true)
    { }
    
    /** \brief Get the DeviceCommunicator policy object. */
    const DeviceCommunicator& get_comm() const
    { return *comm_; }

    /** \brief Get if we believe the referred device to be live. */
    bool get_is_belived_live() const
    { return is_live_; }


    /** \brief Request a new DeviceCommunicator for this DeviceReference. 
     * \return A new DeviceCommunicator. */
    std::unique_ptr<DeviceCommunicator> create_new_communicator()
    {
      return comm_factory_->create_new_communicator(comm_config_info_);
    }
    
  private:
    bool is_believed_live;
    id_t device_id_;
    std::unique_ptr<CommunicatorFactory> comm_factory_;
    std::vector<std::string> comm_config_info_;
    
  }; // class DeviceReference
} // namespace skynet


#endif /* SKYNET_DEVICEREFERENCE_HPP__ */

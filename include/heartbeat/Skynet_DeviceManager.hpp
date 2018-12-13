#ifndef SKYNET_DEVICEMANANGER_HPP__
#define SKYNET_DEVICEMANANGER_HPP__

#include <utility> //Is this used?
#include <vector>
#include "devices/Skynet_DeviceCommunicator.hpp"
#include "devices/Skynet_DeviceReference.hpp"

namespace skynet
{
  /** \class DeviceManager
   *  \brief Abstract class for keeping track of devices
   *
   * This abstract class provides an interface for keeping
   * track of neighboring devices and the communication type
   * associated with these devices.
   */
   class DeviceManager
   {
   public:

     /** \brief Construct a new DeviceManager
      *
      *	\param nearby_devices a vector of devices that this device
      *		will keep track of.
      * QUESTION: Before Colin had std::move(nearby_devices) here.
      * 	I'm not sure why. Should we do that instead?
      */
     template<typename T>
     DeviceManager(const T& config) const
     {
       set_device_list(config);
     }
     
     virtual ~DeviceManager() = default;

     template<typename T>
     virtual void set_device_list(const T& config) const = 0;
     
     //Get number of nearby devices
     unsigned long get_number_of_devices() const
     {
       return do_get_number_of_devices();
     }

     //Get set of nearby devices
     std::vector<DeviceReference> get_device_list() const
     {
       return nearby_devices_;
     }

     //Add a device to the nearby_device list
     //Question: should we have do_add_device and do_remove device
     // functions in the private section?
     virtual void add_device(DeviceReference new_device) const = 0;

     //Remove a device from the nearby_device list
     virtual void remove_device(DeviceReference old_device) const = 0;

     //Get list of communicator types for nearby devices
     /**NOTE: Changed get_communicator_type to get_com_list since
      * (I believe) the communicator type doesn't have to be the same 
      * for all devices in your device list. Also DeviceReference already
      * has a function to get the device communicator, so I'm questioning
      * whether we need this here at all.
      */ 
     DeviceCommunicator get_comm_list() const
     {
       return do_get_comm_list();
     }

   private:

     virtual unsigned long do_get_number_of_devices() const = 0;

     std::vector<DeviceCommunicator&> do_get_comm_list() const
     {
       std::vector<DeviceReference> device_list = get_device_list();

       std::vector<DeviceCommunicator> comm_list;
       
       for(std::vector<DeviceReference>::iterator it = device_list.begin(); it != device_list.end(); ++it){
	 comm_list.push_back(it.get_comm());
       }

       return comm_list;
     }


   private:
     std::vector<DeviceReference> nearby_devices_;

   };// class 


}// namespace skynet

#endif /* SKYNET_DEVICEMANANGER_HPP__ */

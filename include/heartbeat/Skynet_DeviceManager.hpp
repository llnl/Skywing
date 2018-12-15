A#ifndef SKYNET_DEVICEMANANGER_HPP__
#define SKYNET_DEVICEMANANGER_HPP__

#include <vector>
#include <algorithm> //For remove function used to remove devices from device used

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
      *	\param nearby_devices a vector of devices that DeviceManager
      *	       will keep track of.
      */
     DeviceManager(const std::vector<DeviceReference> nearby_devices)
       : nearby_devices(nearby_devices_) const
     {
     }

     /** \brief DeviceManager destructor
      */
     virtual ~DeviceManager() = default;

     /** \brief Get devices that the DeviceManager is currently keeping
      *	track of
      *
      * The complete set of known devices  may include dead devices if 
      *	the DeviceManager continues to track devices that have been
      * pronounced dead.
      *
      * \return a vector of DeviceReferences
      */
     std::vector<DeviceReference> get_known_devices() const
     {
       return nearby_devices_;
     }

     /** \brief Get devices that DeviceManager currently believes to be live
      *
      * \return a vector of DeviceReferences
      */
     std::vector<DeviceReference> get_live_devices() const
     {
       return do_get_live_devices();
     }

     /** Add a device to the nearby devices list
      *
      * \param new_device Device to add to the nearby devices list
      */
     void add_device(DeviceReference new_device) const
     {
       nearby_devices_.push_back(new_device);
     }

     /*Remove a device from the nearby devices list
      *
      * \param old_device device remove from nearby devices list
      */
     void remove_device(DeviceReference old_device) const
     {
       //Use erase on top of remove to shorten vector after removing device
       nearby_devices.erase(std::remove(nearby_devices.begin(), nearby_devices.end(), old_device), nearby_devices_.end());
     }


   private:

     /** Get devices that the device manager believes to be live
      * If the DeviceManager only keeps track of live devices this function
      * will be equivalent to get_live_devices.
      */
     virtual std::vector<DeviceReference> do_get_live_devices() const = 0;
     

   private:
     std::vector<DeviceReference> nearby_devices_;

   };// class 


}// namespace skynet

#endif /* SKYNET_DEVICEMANANGER_HPP__ */

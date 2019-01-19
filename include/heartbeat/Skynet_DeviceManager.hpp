#ifndef SKYNET_DEVICEMANANGER_HPP__
#define SKYNET_DEVICEMANANGER_HPP__

#include <cstdint>
#include <vector>
#include <memory>
#include <algorithm> //For remove function used to remove devices from device used
#include <unordered_map>

#include "devices/Skynet_DeviceCommunicator.hpp"
#include "devices/Skynet_DeviceReference.hpp"
#include "devices/Skynet_Gateway.hpp"
#include "Skynet_BeatSender.hpp"
#include "Skynet_DeviceIdRegistry.hpp"

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
     // Some types that will be used by the DeviceManager
     using history_t = std::vector<BeatResponse>; //Device history
     using id_t = uint8_t; //Device ID


   public:

      DeviceManager(std::unique_ptr<Gateway> gateway):
        gateway_(std::move(gateway))
      { }


     /** \brief Construct a new DeviceManager
      *
      *	\param nearby_devices a vector of devices that DeviceManager
      *	       will keep track of.
      */
    /* DeviceManager(std::vector<DeviceReference> nearby_devices)
       : nearby_devices_(std::move(nearby_devices))
     {
       //Add empty vector of responses as initial value for each
       // device to the response_history
       for(auto device = begin(nearby_devices_); device!=end(nearby_devices_); ++device)
    	 {
    	   id_t device_id = device->get_id();
    	   history_t init_history;
    	   response_history_[device_id] = init_history;
    	 }
     }*/

     /** \brief DeviceManager destructor
      */
     //QUESTION: Is the constructor too complicated for the default
     // to work? Also is it okay to have virtual here?
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
      const std::vector<DeviceReference>& get_known_devices() const
     {
       return nearby_devices_;
     }


     /** \brief Listen for new devices to add to list of devices.
      *
      *  We expect this to be called as part of a new thread.
      */
      void listen_for_devices()
      {
        // collect new communicator factories from gateway
        std::vector<std::unique_ptr<CommunicatorFactory>> comm_factories =
          gateway_->collect_new_connections();
        // create a new device references
        for (unsigned i=0; i < comm_factories.size(); i++)
        {
          DeviceReference new_device(id_registry_.next_id(), std::move(comm_factories[i]));
          add_device(new_device);
        }
      }

     void establish_device_connections()
     {
      /*AF: This function was not compling, commented out for now */
      // for (auto device = begin(nearby_devices_); device!=end(nearby_devices_); ++device)
      // {
      //   if (not nearby_device_communicators_.count(device->get_id()))
      //   {
      //   std::pair<const id_t, std::unique_ptr<DeviceCommunicator> p(device->get_id(), device.create_new_communicator());
      //   nearby_device_communicators_.insert(std::move(p));
      //   }
      // }
    }

     /** Add a device to the nearby devices list and initialize its history
      *
      * \param new_device Device to add to the nearby devices list
      */
     void add_device(DeviceReference& new_device)
     {
        //Add new device to device list
        nearby_devices_.push_back(std::move(new_device));

        //Add empty history for new device
        id_t device_id = new_device.get_id();
        history_t init_history;
        response_history_[device_id] = init_history;
     }

     /** Remove a device from the nearby devices list and remove the history
      *  associated with this device
      *
      * \param old_device device remove from nearby devices list
      */
     //NOTE: We might want to think about what it means for a device to be
     // removed. For now I'm assuming that if a device is removed it's removed
     // from the nearby_device list completely, so its history should be removed too.
     // For DeviceManagers that continue to track dead devices, the history might
     // not be removed once the device is pronounced dead, but the status of the
     // device is stored in the DeviceReference, so in this case we wouldn't need
     // to do anything since the status would be updated within DeviceReference
     // and we would keep the dead device along with its  history in the
     // using the nearby_devices_ list in the DeviceManager.
     void remove_device(DeviceReference old_device)
     {
        /*AF: again commenting out, issue with types with the Device Reference
       Use erase on top of remove to shorten vector after removing device
       nearby_devices_.erase(std::remove(nearby_devices_.begin(),
					 nearby_devices_.end(), old_device),
			                 nearby_devices_.end());


       //Remove device history
       id_t device_id = old_deviceget_id();
       response_history_.erase(device_id);*/
     }

     /** \brief Add response to response_history for the given device
      *
      * \param device Device to add response for
      * \param response Response to add to history for the given device
      */
     // void add_response(const DeviceReference& device,
		 //       const BeatResponse& response)
     // {
     //   do_add_response_(device, response);
     // }

     /** \brief Get the response history for a device
     *
     * \param device Device to get history of
     * \return Response history for the input device
     */
     history_t get_history(const DeviceReference& device)
     {
       return response_history_[device.get_id()];
     }

     /** \brief Clear the response history for a device
     *
     * \param device Device to clear history of
     */
     void clear_history(const DeviceReference& device)
     {
       //Replace history with empty history
       id_t device_id = device.get_id();
       history_t empty_history;
       response_history_[device_id] = empty_history;
     }


   private:
      std::vector<DeviceReference> nearby_devices_;
      std::unordered_map<id_t, std::unique_ptr<DeviceCommunicator>>
        nearby_device_communicators_;
      std::unique_ptr<Gateway> gateway_;
      DeviceIdRegistry<id_t> id_registry_;

     /** A map storing the response history for all live devices */
     //QUESTION: Should response history be stored for live devices or known
     // devices? This might depend on the type of  DeviceManager.
     std::unordered_map<id_t, history_t> response_history_;

   };// class


}// namespace skynet

#endif /* SKYNET_DEVICEMANANGER_HPP__ */

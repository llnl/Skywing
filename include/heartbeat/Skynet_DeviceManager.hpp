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
   *  \brief Class for keeping track of devices
   *
   * This class provides an interface for keeping track
   * of neighboring devices and the communication type
   * associated with these devices.
   */
   class DeviceManager
   {
   public:
     // Some types that will be used by the DeviceManager
     using history_t = std::vector<BeatResponse>; //Device history
     using id_t = uint8_t; //Device ID


   public:
     /** \brief Construct a new DeviceManager
      *
      * \param gateway the devices gateway used for creating communications to
      *  new devices
      */
     DeviceManager(std::unique_ptr<Gateway> gateway):
        gateway_(std::move(gateway))
      { }

     /** \brief DeviceManager destructor
      */
     ~DeviceManager() = default;


     /** \brief Add a device to the list of neighboring devices and initialize its history
      *
      * \param new_device Device to add to the neighbors list
      */
     void add_device(DeviceReference& new_device)
     {
        //Add new device to neighbors list
        neighbors_.push_back(std::move(new_device));

        //Get device ID to use in setting history and communicator
        id_t device_id = new_device.get_id();

        //Add empty history for new device
        history_t init_history;
        response_history_[device_id] = init_history;

        //Add communicator for new device
        neighbor_communicators_[device_id] = new_device.create_new_communicator();
     }

     /** \brief Remove a device from the list of neighboring devices and remove the
      *  history associated with this device
      *
      * \param old_device device remove from neighbors list
      */
     //NOTE: We might want to think about what it means for a device to be
     // removed. For now I'm assuming that if a device is removed it's removed
     // from the neighbors list completely, so its history should be removed too.
     // For DeviceManagers that continue to track dead devices, the history might
     // not be removed once the device is pronounced dead, but the status of the
     // device is stored in the DeviceReference, so in this case we wouldn't need
     // to do anything since the status would be updated within DeviceReference
     // and we would keep the dead device along with its  history in the
     // using the neighbors list in the DeviceManager.
     void remove_device(DeviceReference old_device)
     {
        /*AF: again commenting out, issue with types with the Device Reference
       //Use erase on top of remove to shorten vector after removing device
       neighbors_.erase(std::remove(neighbors_.begin(),neighbors_.end(), old_device),
			                 neighbors_.end());


       //Remove device history
       id_t device_id = old_device.get_id();
       response_history_.erase(device_id);*/
     }

      /** \brief Get new devices that have connected from the gateway and
       *   add connections to these devices 
      */
      void add_new_connections()
      {
        // Collect communicator factories for new neighbors from gateway
        std::vector<std::unique_ptr<CommunicatorFactory>> comm_factories =
          gateway_->collect_new_connections();
	
        // Create a device references for new neighbors
        for (unsigned i=0; i < comm_factories.size(); i++)
        {
          DeviceReference new_device(id_registry_.next_id(),
				     std::move(comm_factories[i]));
          add_device(new_device);
        }
      }
     
     /** \brief Get neighbors to this device
      *
      * \return a vector of DeviceReferences representing this device's neighbors
      */
     const std::vector<DeviceReference>& get_neighbors() const
     {
       return neighbors_;
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
     //neighbors_ is the set of devices that this device can communicate
     // with directly (i.e. is adjacent to in the reference graph)
     std::vector<DeviceReference> neighbors_;
      std::unordered_map<id_t, std::unique_ptr<DeviceCommunicator>>
        neighbor_communicators_;
      std::unique_ptr<Gateway> gateway_;
      DeviceIdRegistry<id_t> id_registry_;

     //A map storing the response history for all neighboring devices
     std::unordered_map<id_t, history_t> response_history_;

   };// class


}// namespace skynet

#endif /* SKYNET_DEVICEMANANGER_HPP__ */

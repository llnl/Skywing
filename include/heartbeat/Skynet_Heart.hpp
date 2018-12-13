#ifndef SKYNET_HEART_HPP__
#define SKYNET_HEART_HPP__

#include <vector>
#include "Skynet_DeviceReference.hpp"
#include "Skynet_DeviceManager.hpp"
#include "Skynet_Heartbeat.hpp"
#include "Skynet_Pulse.hpp"

namespace skynet
{
    /** \class Heart
     *  \brief The center of a Skynet instance
     *
     * A Heart, collectively across the Skynet instance, runs the
     * heartbeat and manages the participating devices.
     */
    class Heart
    {
    public:
      /** \brief Construct a new Skynet Heart
       *
       *  \param device_heartbeat type of heartbeat that this device has
       *  \param device_pulse type of pulse that this device has
       *  \param nearby_devices neigboring devices that this devices is aware of
       * QUESTION: should we use std::move here? Colin used that in the initial 
       * heart constructor, but I'm not sure why.
       */
      Heart(Heartbeat device_heartbeat, Pulse device_pulse, std::vector<DeviceReference> nearby_devices)
	: device_heartbeat_(device_heartbeat), device_pulse_(device_pulse)
      {
	device_manager_ = DeviceManager::DeviceManager(nearby_devices);
	
      }
      
	/** \brief Begin the heartbeat. */
	void begin();

    private:


    private:
      Heartbeat device_heartbeat_;
      Pulse device_pulse_;
      DeviceManager device_manager_;
      //QUESTION: Should we also include a property list within the heart
    }; // class Heart

} // namespace skynet

#endif /* SKYNET_HEART_HPP__ */

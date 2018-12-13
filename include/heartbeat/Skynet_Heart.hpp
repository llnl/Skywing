#ifndef SKYNET_HEART_HPP__
#define SKYNET_HEART_HPP__

#include <vector>

#include "Skynet_DeviceReference.hpp"
#include "Skynet_DeviceManager.hpp"
#include "Skynet_Heartbeat.hpp"
#include "Skynet_Pulse.hpp"
#include "Skynet_InitializeHeart.hpp"
#include "Skynet_GraphProperty.hpp"

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
      //QUESTION: Should we use templates here, and should heart just take a configuration file as input
      template<typename T>
      Heart(const Heartbeat device_heartbeat, const Pulse device_pulse, const T& config)
	: device_heartbeat_(device_heartbeat), device_pulse_(device_pulse)
      {
	device_manager_ = DeviceManager::DeviceManager(config);
	
      }
      
	/** \brief Begin the heartbeat. */
      void IntializeHeart::begin_heartbeat();

    private:


    private:
      Heartbeat device_heartbeat_;
      Pulse device_pulse_;
      DeviceManager device_manager_;
      std::vector<GraphProperty> property_list_;
    }; // class Heart

} // namespace skynet

#endif /* SKYNET_HEART_HPP__ */

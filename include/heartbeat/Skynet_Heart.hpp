#ifndef SKYNET_HEART_HPP__
#define SKYNET_HEART_HPP__

#include <vector>

#include "Skynet_BeatSender.hpp"
#include "Skynet_BeatInterpreter.hpp"
#include "Skynet_PulseTimer.hpp"
#include "Skynet_DeviceManager.hpp"
#include "Skynet_PropertyChecker.hpp"

namespace skynet
{
    /** \class Heart
     *  \brief The center of a Skynet instance
     *
     * \param nearby_devices A vector of DeviceReferences
     * representing other Skynet devices.
     */
    class Heart
    {
    public:
      /** \brief Construct a new Skynet Heart
       *
       *  \param beat_sender type of BeatSender used by this device
       *  \param beat_interpreter type of BeatInterpreter used by this device
       *  \param pulse_timer type of PulseTimer used by this device
       *  \param device_manager type of DeviceManager used by this device
       *  \param property_checker type of PropertyChecker used by this device
       */
      Heart(const BeatSender beat_sender, const BeatInterpreter beat_interpreter,
	    const PulseTimer pulse_timer, const DeviceManager device_manager,
	    const PropertyChecker property_checker)
	: beat_sender_(std::move(beat_sender_)),
	  beat_interpreter_(std::move(beat_interpreter)),
	  pulse_timer_(std::move(pulse_timer)),
	  device_manager_(std::move(device_manager)),
	  property_checker_(std::move(property_checker)),
      {
	run_heartbeat();
      }

      /** \brief Begin the heartbeat and run until device dies. */
      void run_heartbeat() const
      {
	/*
	 * Steps:
	 * 1. Check graph properties and update graph if necesary (PropertyChecker)
	 * 2. Get nearby device list (DeviceManager). For each device in this list:
	 * 	a. Send beat (BeatSender)
	 * 	b. Record response and decide what to do with it (BeatInterpreter)
	 *	c. Re-check properties and update graph if necessary (PropertyChecker)
	 * 3. Get new nearby device list (DeviceManager), which could have been modified by BeatInterpreter and PropertyChecker. For each device in this list:
	 *	a. Check pulse to determine next time to send beat to that device (PulseTimer).
	 * 4. Keep a list of (time, device) pairs, ordered by time, where time is the next time to send a beat to the corresponding device.
	 * 5. Once first time in (time, device) list is reached:
	 *	a. Send a beat to that device (BeatSender)
	 * 	b. Record response and decide what to do with it (BeatInterpreter)
	 *	c. Re-check properties and update graph if necessary (PropertyChecker)
	 * 	d. Get new nearby device list (DeviceManager)
	 *	e. Check pulse of new devices in device list
	 *	f. Update (time, device) list by adding (time, device) pairs for new devices and removing (time, device) pairs for devices that were removed
	 * 6. Repeat step 5 each time a the next pulse time is reached.
	 *
	 * COMMENTS:
	 *	1. I'm not sure what the best way to store the (time, device) list is since we need to be able to add and remove devices from the list as well as update the times. I'm also not sure if this is something that should be included in the heart or if we should make a separate class for it. I'm thinking we should either make a separate class that stores this list and has functions to add and remove devices from the list as well as get the time of the next pulse and which device that pulse should be sent to, or that these capabilities should be added to the DeviceManager.
	 *	2. Need to figure out when/how new devices that come online can contact this device and be added to its  nearby device list.
	 */


	//Wait to send out next heartbeat
	  // (Reference: https://stackoverflow.com/questions/10073136/how-to-execute-a-particular-code-in-c-after-every-1-minute)
	//  std::this_thread::sleep_for(std::chrono::seconds(pulse));

      }

    private:


    private:
      BeatSender beat_sender_;
      BeatInterpreter beat_interpreter_;
      PulseTimer pulse_timer_;
      DeviceManager device_manager_;
      PropertyChecker property_checker_;
    }; // class Heart

} // namespace skynet

#endif /* SKYNET_HEART_HPP__ */

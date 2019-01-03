#ifndef SKYNET_HEART_HPP__
#define SKYNET_HEART_HPP__

#include <vector>
#include <thread>
#include <memory>
#include <chrono> //For timing pulse

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
     * A Heart, collectively across the Skynet instance, runs the
     * heartbeat and manages the participating devices.
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
      Heart(std::unique_ptr<BeatSender> beat_sender, 
	    std::unique_ptr<BeatInterpreter> beat_interpreter,
	    std::unique_ptr<PulseTimer> pulse_timer, 
	    std::unique_ptr<DeviceManager> device_manager,
	    std::unique_ptr<PropertyChecker> property_checker)
	: beat_sender_(std::move(beat_sender_)),
	  beat_interpreter_(std::move(beat_interpreter)),
	  pulse_timer_(std::move(pulse_timer)),
	  device_manager_(std::move(device_manager)),
	  property_checker_(std::move(property_checker))
      {}

      /** \brief Perform heartbeat initalization steps.
       *
       * These steps are the following:
       * 1. Start a thread to listen for new devices.
       * 2. Establish connections (DeviceCommunicators) with nearby devices.
       * 3. Check that reference graph properties are satisfied.
       * 4. While reference graph properties are not satisfied, modify
       *    nearby devices and repeat steps 2 and 3 until satisfied.
       */
      void initalize_heartbeat()
      {
	// Step 1
	std::thread t(&DeviceManager::listen_for_devices, std::ref(device_manager_));
	new_device_listener_thread_ = std::move(t);

	// Step 2
	device_manager_.establish_device_connections();

	// Step 3
	
      }

      /** \brief Begin the heartbeat and run until device dies. */
      void run_heartbeat() const
      {
	std::thread sending_thread(send_heartbeat);
      }

      /** \brief Function to send regular heartbeats
       */
      void send_heartbeat() const
      {
	//Run the heartbeat indefinitely
	while (true)
	{

	  //Get list of nearby devices
	  //NOTE: The DeviceManager also has a function get_known_devices, which
	  // gets all devices including those that have been pronounced dead.
	  // In later versions we may want to consider if devices that have been
	  // pronounced dead should ever be queried to see if they've come back online.
	  std::vector<DeviceReference>& device_list = device_manager_.get_live_devices()

	  //Send heartbeat to each device in device list and process responses
	  for (DeviceReference& device : device_list)
	  {
	   //Send beat and record response
	   BeatResponse& response = beat_sender_.send_heartbeat(device);

	   //Add response to response history for the given device
	   device_manager_.add_response(device, response);

	   //Get the complete history for the device including the new response
	   typename DeviceManager::history_t device_history = device_manager_.get_history(device);

	   //Decide what to do with the device based on its history
	   bool keep_device = beat_interpreter_.should_device_remain(device, device_history);

	   //If we have decided not to keep the device, remove it using the device manager
	   if (!keep_device)
	     { device_manager_.remove_device(device); }
	  }

	  //Check graph properties and update reference graph if necessary
	  property_checker_.validate_graph(device_manager_);

	  //Get pulse for all nearby devices and decide how long to wait to send next
	  // heartbeat based on responses.
	  // NOTE 1: Currently we wait the same amount of time to send the next heartbeat
	  //       to all nearby devices. We may want to modify this later to allow us
	  //       to send heartbeats at different times to different devices.
	  // NOTE 2: The wait time will actually be longer for some devices because we
	  //         will wait this long to start sending beats to devices again, but
	  //	     beats are sent sequentially some devices will have to wait longer.
	  double wait_time = 0; //Default time to wait until next heartbeart 
	  for (DeviceReference& device : device_list)
	  {
	    double temp_time = pulse_timer_.millisecs_to_next_beat(device);

	    // Use the minimum pulse as the time to send the next heartbeat
	    if (temp_time < wait_time)
	    {
	      wait_time = temp_time;
	    }
	  }

	  //Wait to send the next heartbeat
	  std::this_thread::sleep_for(std::chrono::milliseconds(pulse)); 
	}
      }

    private:
      std::unique_ptr<BeatSender> beat_sender_;
      std::unique_ptr<BeatInterpreter> beat_interpreter_;
      std::unique_ptr<PulseTimer> pulse_timer_;
      std::unique_ptr<DeviceManager> device_manager_;
      std::unique_ptr<PropertyChecker> property_checker_;

      std::thread new_device_listener_thread_;
    }; // class Heart

} // namespace skynet

#endif /* SKYNET_HEART_HPP__ */

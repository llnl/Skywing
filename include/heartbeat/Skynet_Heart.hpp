#ifndef SKYNET_HEART_HPP__
#define SKYNET_HEART_HPP__

#include <vector>
#include <thread>
#include <chrono> //For timing pulse

#include "Skynet_BeatSender.hpp"
#include "Skynet_BeatInterpreter.hpp"
#include "Skynet_PulseTimer.hpp"
#include "Skynet_DeviceManager.hpp"
#include "Skynet_PropertyChecker.hpp"
#include "devices/Skynet_DeviceCommunicator.hpp"

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
      /*AF: Commented the two statements below for compling, not sure yeat what is needed*/
      // using comm_list_t = typename;
      // DeviceManager:std::vector<std::unique_ptr<DeviceCommunicator>>;

    public:
      /** \brief Construct a new Skynet Heart
       *
       *  \param beat_sender type of BeatSender used by this device
       *  \param beat_interpreter type of BeatInterpreter used by this device
       *  \param pulse_timer type of PulseTimer used by this device
       *  \param device_manager type of DeviceManager used by this device
       *  \param property_checker type of PropertyChecker used by this device
       */
      /*AF: Commented out below statemnet, There are no constuctors for the below items */
      Heart(BeatSender beat_sender, BeatInterpreter beat_interpreter,
	    PulseTimer pulse_timer, DeviceManager device_manager,
	    PropertyChecker property_checker)
	     : beat_sender_(&beat_sender),
	  beat_interpreter_(&beat_interpreter),
	  pulse_timer_(&pulse_timer),
	  device_manager_(&device_manager),
	  property_checker_(&property_checker),
	  is_device_alive_(true)
      {}

      /** \brief Begin the heartbeat and run until device dies. */
      void run_heartbeat() const
      {
        /*AF:Commented out for Compling purposes
        std::thread sending_thread(send_heartbeat());
        std::thread listening_thread(listen_for_beats);*/
      }

      /** \brief Function to send regular heartbeats
       */
      void send_heartbeat() const
      {
        /*AF: Commented out whole function, too much to parse though for complingin issues.
      	//Run the heartbeat indefinitely
        while (is_device_alive_)
      	{

    	  //Get list of nearby devices
    	  //NOTE: The DeviceManager also has a function get_known_devices, which
    	  // gets all devices including those that have been pronounced dead.
    	  // In later versions we may want to consider if devices that have been
    	  // pronounced dead should ever be queried to see if they've come back online.
        std::unique_ptr<std::vector<DeviceReference>> device_list = device_manager_.get_live_devices()

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
            }// AF: This was a missing curly brace, not sure if this is where its suppose to be.
          }*/
        }

      /* brief Continuously loop through communicator list to check for incoming
       *   beats and send responses to those beats
       */
      //QUESTION: Do we need to add a delay between checks for beats?
      void listen_for_beats() const
      {
        /*AF: commented out for now, no function or type for comm_list_t
        while(is_device_alive_)
        {
          comm_list_t& comm_list = device_manager.get_comm_list();
          for (DeviceCommunicator& comm : comm_list)
          {
            //Check if a beat has been received
            auto message = comm_list->receive_from();

            //Respond to beat
          }
        }*/
      }

    void kill_device()
    {
    is_device_alive_ = false;
    }

    private:
      std::unique_ptr<BeatSender> beat_sender_;
      std::unique_ptr<BeatInterpreter> beat_interpreter_;
      std::unique_ptr<PulseTimer> pulse_timer_;
      std::unique_ptr<DeviceManager> device_manager_;
      std::unique_ptr<PropertyChecker> property_checker_;
      bool is_device_alive_; //Indicates if this device is allive
    }; // class Heart

} // namespace skynet

#endif /* SKYNET_HEART_HPP__ */

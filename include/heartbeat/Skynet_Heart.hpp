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


    // Declare free functions that will be friend functions of the Heart class
    class Heart;
    void start_task_cycle(Heart* heart);
    void task_cycle(Heart* heart);


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
       *  \param config The Configuration object for the Skynet instance
       */
      Heart(std::unique_ptr<BeatSender> beat_sender, std::unique_ptr<BeatInterpreter> beat_interpreter,
            std::unique_ptr<PulseTimer> pulse_timer, std::unique_ptr<DeviceManager> device_manager,
            std::unique_ptr<PropertyChecker> property_checker, KeyValueReader& config)
       : beat_sender_(std::move(beat_sender)),
         beat_interpreter_(std::move(beat_interpreter)),
         pulse_timer_(std::move(pulse_timer)),
         device_manager_(std::move(device_manager)),
         property_checker_(std::move(property_checker)),
         is_alive_(false)
      {
        // obtain task_cycle pause from configuration file
        task_cycle_pause_ = stoi(config.get_value("task_cycle_pause"));
      }

      /** \brief Activate the heart.
      */
      template<void (*start_task_cycle_function)(Heart*) = start_task_cycle>
      void activate()
      {
        is_alive_ = true;
        start_task_cycle_function(this);
      }

      // DEBUG: Remove this once no longer needed for testing
      int number_of_connections()
      { 
        return device_manager_->get_neighbors().size();
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

      void terminate()
      {
        is_alive_ = false;
        if (task_cycle_thread_.joinable())
          task_cycle_thread_.join();
      }

    private:

      /* \brief Start the task cycle, which cycles through all Heart tasks
       */
      friend void start_task_cycle(Heart* heart);

      /** \brief Cycle through all the tasks for the heart:
       *  1) have Device Manager respond to new Device connection requests
       *  ?) respond to all incoming requests
       *  ?) send heartbeat
       */
      friend void task_cycle(Heart* heart);

      std::unique_ptr<BeatSender> beat_sender_;
      std::unique_ptr<BeatInterpreter> beat_interpreter_;
      std::unique_ptr<PulseTimer> pulse_timer_;
      std::unique_ptr<DeviceManager> device_manager_;
      std::unique_ptr<PropertyChecker> property_checker_;
      bool is_alive_;
      std::thread task_cycle_thread_;
      int task_cycle_pause_;


    }; // class Heart

    /** \brief Default implementation of start_task_cycle
     */
    void start_task_cycle(Heart* heart)
    {
      heart->device_manager_->connect_to_existing_devices();
      heart->task_cycle_thread_ = std::thread(task_cycle, heart);
    }

    /** \brief Default implementation of task_cycle function
     */
    void task_cycle(Heart* heart)
    {
      while (heart->is_alive_)
      {
        heart->device_manager_->respond_to_connection_requests();
        std::this_thread::sleep_for(std::chrono::seconds(heart->task_cycle_pause_));
      }
    }
} // namespace skynet

#endif /* SKYNET_HEART_HPP__ */

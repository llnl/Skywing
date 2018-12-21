#ifndef SKYNET_BEATSENDER_HPP__
#define SKYNET_BEATSENDER_HPP__

#include "devices/Skynet_DeviceReference.hpp"

namespace skynet
{

  /** \brief Defines what a response from another device looks like */
  struct BeatResponse
  {
    double response_val;
  };
  
  /** \class BeatSender
   *  \brief Abstract class for operting the heartbeat.
   *	Defines the sequence of events that occurs each
   *	time the heart "beats."
   */
   class BeatSender
   {
   public:
     
     /** \brief Send a signal to a device from the nearby
     *   device list and receive response.
     */
     virtual BeatResponse send_heartbeat(const DeviceReference& device) = 0;

   private:

   };// class BeatSender


}// namespace skynet

#endif /* SKYNET_BEATSENDER_HPP__ */

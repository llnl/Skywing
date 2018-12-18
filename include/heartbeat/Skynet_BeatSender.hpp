#ifndef SKYNET_BEATSENDER_HPP__
#define SKYNET_BEATSENDER_HPP__

#include "devices/Skynet_DeviceReference.hpp"

namespace skynet
{
  /** \class BeatSender
   *  \brief Abstract class for operting the heartbeat.
   *	Defines the sequence of events that occurs each
   *	time the heart "beats."
   */
   class BeatSender
   {
   public:

     //QUESTION: Can/should this structure be virtual and can/should
     //	response_val be a template?
     struct BeatResponse
     {
       int response_val;
     };
     
     /** \brief Send a signal to a device from the nearby
     *   device list and receive response.
     */
     virtual BeatResponse send_heartbeat(const DeviceReference& device) = 0;

   private:

   };// class BeatSender


}// namespace skynet

#endif /* SKYNET_BEATSENDER_HPP__ */

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

     /** \brief Send a signal to a device from the nearby
     *   device list and receive response.
     */
     template<typename T>
     T send_heartbeat(const DeviceReference& device);

   private:

   };// class BeatSender


}// namespace skynet

#endif /* SKYNET_BEATSENDER_HPP__ */

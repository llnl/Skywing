#ifndef SKYNET_TRIVALBEATSENDER_HPP__
#define SKYNET_TRIVALBEATSENDER_HPP__

#include "heartbeat/Skynet_BeatSender.hpp"

namespace skynet
{

  /** \class TrivalBeatSender
   *  \brief class for operting a trivial heartbeat.
   */
   class TrivialBeatSender : public BeatSender
   {
   public:

     TrivialBeatSender()
     { }

   private:
     BeatResponse do_send_heartbeat_(const DeviceReference& device) const override
     {
       BeatResponse response;
       response.response_val = 0;
       return response;
     }
   };// class BeatSender


}// namespace skynet

#endif /* SKYNET_TRIVALBEATSENDER_HPP__ */

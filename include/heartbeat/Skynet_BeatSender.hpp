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
   *  Defines the sequence of events that occurs each
   *  time the heart "beats."
   */
  class BeatSender
  {
  public:

    /** \brief Send a signal to a device from the nearby
     *   device list and receive response.
     *
     * \param device Device to send beat to and get response from
     */
    BeatResponse send_heartbeat(const DeviceReference& device)
    {
      /*AF: Added a defalut response for compling issues for now */
      // do_send_heartbeat(device);
      // BeatResponse response;
      // response.response_val = 0;
      // return response;
      return do_send_heartbeat_(device);
    }

    virtual ~BeatSender() = default;

  private:
    virtual BeatResponse do_send_heartbeat_(const DeviceReference& device) const = 0;

  };// class BeatSender

}// namespace skynet

#endif /* SKYNET_BEATSENDER_HPP__ */

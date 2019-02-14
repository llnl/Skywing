#ifndef SKYNET_PULSETIMER_HPP__
#define SKYNET_PULSETIMER_HPP__

#include <vector>

#include "devices/Skynet_DeviceReference.hpp"

namespace skynet
{
    /** \class PulseTimer
     *  \brief Abstract class to define how pulses are sent
     *
     *	The simplest version of the pulse is a fixed-length time
     *	interval at which queries will be sent to all devices. A
     *	more complex pulse might be neighboring device specific
     *	and depend, for example, on if a response was received from
     *	the neighboring device the last time a pulse was sent.
     */
    class PulseTimer
    {
    public:

      //NOTE: Decided to use relative time for now to avoid the situation
      // where the next beat for this device comes before we've received a
      // response from the rest of the devices, also because it seems more
      // intuitive to define beats in this way. Currently beats are sent in
      // sequence. If beats are sent in parallel in future implementations
      // this could be changed.
      /** \brief Get the time to wait until the next heartbeat is sent to
       *   the input device
       *
       * \param device Nearby device to get the pulse (time to wait) for
       */
      double  millisecs_to_next_beat(const DeviceReference& device)
      {
	       return do_millisecs_to_next_beat_(device);
      }

      virtual ~PulseTimer() = default;

    private:

      virtual double do_millisecs_to_next_beat_(const DeviceReference& device) const = 0;

    }; // class PulseTimer

} // namespace skynet

#endif /* SKYNET_PULSETIMER_HPP__ */

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

      virtual double get_next_time(const DeviceReference& device) const = 0;
      
    private:

    }; // class PulseTimer

} // namespace skynet

#endif /* SKYNET_PULSETIMER_HPP__ */

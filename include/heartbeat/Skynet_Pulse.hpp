#ifndef SKYNET_PULSE_HPP__
#define SKYNET_PULSE_HPP__

#include <vector>

namespace skynet
{
    /** \class Pulse
     *  \brief Abstract class to define how pulses are sent
     *
     *	The simplest version of the pulse is a fixed-length time
     *	interval at which queries will be sent to all devices. A
     *	more complex pulse might be neighboring device specific
     *	and depend, for example, on if a response was received from
     *	the neighboring device the last time a pulse was sent.
     */
    class Pulse
    {
    public:

      virtual double get_next_time(const Device& device);
      
    private:

    }; // class Pulse

} // namespace skynet

#endif /* SKYNET_PULSE_HPP__ */

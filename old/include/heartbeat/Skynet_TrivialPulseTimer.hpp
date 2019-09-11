#ifndef SKYNET_TRIVIALPULSETIMER_HPP__
#define SKYNET_TRIVIALPULSETIMER_HPP__

#include <vector>

#include "Skynet_PulseTimer.hpp"

namespace skynet
{
  /** \class TrivialPulseTimer
   *  \brief Class to define how pulses are sent
   *
   *  The simplest version of the pulse is a fixed-length time
   *  interval at which queries will be sent to all devices. A
   *  more complex pulse might be neighboring device specific
   *  and depend, for example, on if a response was received from
   *  the neighboring device the last time a pulse was sent.
   */
  class TrivialPulseTimer : public PulseTimer
  {
  public:
    TrivialPulseTimer()
    { }

  private:
    double do_millisecs_to_next_beat_(const DeviceReference& /* device */) const override
    {
      return 0;
    }

  }; // class TrivialPulseTimer

} // namespace skynet

#endif /* SKYNET_TRIVIALPULSETIMER_HPP__ */

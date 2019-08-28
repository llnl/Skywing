#ifndef SKYNET_TRIVIALBEATINTERPRETER_HPP__
#define SKYNET_TRIVIALBEATINTERPRETER_HPP__

#include "Skynet_BeatInterpreter.hpp"

namespace skynet
{
  /** \class TrivialBeatInterpreter
   *  \brief Class for deciding what to do with history
   *  of heartbeats (e.g. if neighboring devices should be
   *  pronounced dead)
   */
  class TrivialBeatInterpreter : public BeatInterpreter
  {
  public:
    TrivialBeatInterpreter()
    { }

  private:
    bool do_should_device_remain_(const DeviceReference& /* device */,
      const std::vector<BeatResponse>& /* device_history */) const override
    {
      return true;
    }

   };// class TrivialBeatInterpreter

}// namespace skynet

#endif /* SKYNET_TRIVIALBEATINTERPRETER_HPP__ */

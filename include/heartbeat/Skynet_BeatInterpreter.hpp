#ifndef SKYNET_BEATINTERPRETER_HPP__
#define SKYNET_BEATINTERPRETER_HPP__

#include "devices/Skynet_DeviceReference.hpp"
#include "Skynet_BeatSender.hpp"

namespace skynet
{
  /** \class BeatInterpreter
   *  \brief Abstract class for deciding what to do with history
   *  of heartbeats (e.g. if neighboring devices should be
   *  pronounced dead)
   */
  class BeatInterpreter
  {
  public:

    /** \brief Determine the status of a device based on its
     * responses to the heartbeat
     *
     * \param device the device that we want to determine the status of
     * \param device_history Vector of responses from the device to
     *   be used in determining device status
     *
     * \return 1 if device is alive or at least we are not ready to
     *   pronounce it dead, 0 if we have decided device is dead
     */
    bool should_device_remain(const DeviceReference& device,
             const std::vector<BeatResponse>& device_history)
    {
      return do_should_device_remain_(device, device_history);
    }

    virtual ~BeatInterpreter() = default;

  private:
    virtual bool do_should_device_remain_(const DeviceReference& device,
      const std::vector<BeatResponse>& device_history) const = 0;

  };// class BeatInterpreter


}// namespace skynet

#endif /* SKYNET_BEATINTERPRETER */

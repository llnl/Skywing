#ifndef SKYNET_HEART_HPP__
#define SKYNET_HEART_HPP__

#include <utility>
#include <vector>
#include <thread>
#include "devices/Skynet_DeviceReference.hpp"

namespace skynet
{
  /** \class Heart
   *  \brief The center of a Skynet instance.
   *
   * A Heart, collectively across the Skynet instance, runs the
   * heartbeat and manages the participating devices.
   */
  class Heart
  {
  public:
    /** \brief Construct a new Skynet Heart
     *
     * \param nearby_devices A vector of DeviceReferences
     * representing other Skynet devices.
     */

    Heart()
    {}

    Heart(std::vector<DeviceReference> nearby_devices)
      : nearby_devices_(std::move(nearby_devices))
    {}

    ~Heart()
    { }

    const std::vector<DeviceReference>& get_device_references() const
    { return nearby_devices_; }

    /** \brief Begin the heartbeat. */
    void begin_heartbeat();

  private:
    /** \brief Send a heartbeat pulse to a Device and measure the
     *         response time.
     */
  private:
    void heartbeat_fun_();

  private:
    std::vector<DeviceReference> nearby_devices_;
    std::thread heartbeat_thread_;
  }; // class Heart

} // namespace skynet

#endif /* SKYNET_HEART_HPP__ */

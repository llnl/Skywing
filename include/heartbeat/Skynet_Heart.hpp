#ifndef SKYNET_HEART_HPP__
#define SKYNET_HEART_HPP__

#include <utility>
#include <vector>
#include "Skynet_DeviceReference.hpp"

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
	Heart(std::vector<DeviceReference> nearby_devices)
	    : nearby_devices_(std::move(nearby_devices))
	    {}

	/** \brief Begin the heartbeat. */
	void begin();

    private:
	/** \brief Send a heartbeat pulse to a Device and measure the
	 *         response time.
	 */
	double send_heartbeat_pulse(const Device& device);

	/** \brief Accept a new Skynet Device that has come online. */
	Device accept_new_device();

	/** \brief Pronounce a Skynet Device no longer online.
	 *
	 * This gets called either because a Device informs others it
	 * is going offline, or because it has failed to respond to
	 * enough heartbeat pulses.
	 */
	void pronounce_device_terminated(Device& device);

    private:
	std::vector<Device> nearby_devices_;
    }; // class Heart

} // namespace skynet

#endif /* SKYNET_HEART_HPP__ */

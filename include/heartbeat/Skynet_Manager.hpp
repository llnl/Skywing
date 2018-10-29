#ifndef SKYNET_MANAGER_HPP__
#define SKYNET_MANAGER_HPP__

#include <utility>
#include <vector>
#include "Skynet_Device.hpp"

namespace skynet
{
    /** \class Manager
     *  \brief Manages a Skynet instance.
     */
    class Manager
    {
    public:
	/** \brief Construct a new Skynet Manager
	 *
	 * \param nearby_devices A vector of Devices representing
	 * other Skynet devices.
	 */
	Manager(std::vector<Device> nearby_devices)
	    : nearby_devices_(std::move(nearby_devices))
	    {}

	/** \brief Begin the Skynet Manager.
	 *
	 * Starts the heartbeat.
	 */
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
    }; // class Manager

} // namespace skynet

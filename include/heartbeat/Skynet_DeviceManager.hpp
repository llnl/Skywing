#ifndef SKYNET_DEVICEMANANGER_HPP__
#define SKYNET_DEVICEMANANGER_HPP__

#include <utility>
#include <vector>

namespace skynet
{

	class DeviceManager
	{
	public:
	  virtual ~DeviceManager() = default;

	  unsigned long number_of_devices() const
	  {
	    return do_get_number_of_devices();
	  }

	  DeviceCommunicator get_comunication_type() const
	  {
	    return do_get_comunication_type();
	  }


	  std::vector<Device> get_device_list() const
	  {
	    return do_get_comunication_type();
	  }

	private:

	 virtual unsigned long do_get_number_of_devices() const = 0;

	 virtual DeviceCommunicator do_get_comunication_type() const = 0;

	
	private:
		std::vector<Device> nearby_devices_;


	};// class 


}// namespace skynet

#endif /* SKYNET_DEVICEMANANGER_HPP__ */
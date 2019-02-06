#ifndef SKYNET_TRIVIALDEVICEMANANGER_HPP__
#define SKYNET_TRIVIALDEVICEMANANGER_HPP__

#include <vector>
#include <memory>
#include <algorithm> //For remove function used to remove devices from device used
#include <unordered_map>

#include "Skynet_DeviceManager.hpp"

namespace skynet
{
  class TrivialDeviceManager : public DeviceManager
  {
    public:

      TrivialDeviceManager(std::unique_ptr<Gateway> gateway) :
        DeviceManager(std::move(gateway))
      { }

    private:


   };// class


}// namespace skynet

#endif /*SKYNET_TRIVIALDEVICEMANANGER_HPP__*/

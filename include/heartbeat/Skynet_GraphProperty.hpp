#ifndef SKYNET_GRAPHPROPERTY_HPP__
#define SKYNET_GRAPHPROPERTY_HPP__

#include "Skynet_DeviceManager.hpp"

namespace skynet
{
    /** \class GraphProperty
     *  \brief Abstract class to define properties required of 
     *	the reference graph
     */
    class GraphProperty
    {
    public:

      /** \brief Check if the reference graph satisfies this
       *   property using information from the device manager
       */
      virtual bool check_property(const DeviceManager& device_manager) const = 0; 
				      
    private:
      
    }; // class GraphProperty

} // namespace skynet

#endif /* SKYNET_GRAPHPROPERTY_HPP__ */

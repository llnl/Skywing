#ifndef SKYNET_PROPERTYCHECKUP_HPP__
#define SKYNET_PROPERTYCHECKUP_HPP__

#include <vector>

#include "Skynet_GraphProperty.hpp"
#include "Skynet_DeviceManager.hpp"

namespace skynet
{
    /** \class PropertyCheckUp
     *  \brief Abstract class, interface for storing graph properties
     *	that a device cares about and checking these properties.
     *	PropertyCheckUp will also be responsible for updating the
     *	reference graph if the required properties are not met.
     */
    class PropertyCheckUp
    {
    public:

      /** \brief Constructor for PropertyCheckUp
       *
       *  \param property_list List of properties to be checked
       *  \param device_manager Device manager for heart, which can be used
       *	to get list of nearby devices and update this list if properties
       *	aren't satisfied. 
       */
      PropertyCheckUp(std::vector<GraphProperty> property_list, const DeviceManager& device_manager) : poperty_list_(property_list), device_manager_(device_manager)
      {
      }

      /** \brief Get list of properties required by this device
       */
      virtual PropertyList get_property_list() const
      {
	return property_list_;
      }

      /** NOTE: Excluding this function for now because I think we might want
       * check the properties of the reference graph and update simultaneously
       * save time. Some implementations of PropertyCheckUp may separate this
       * from the graph update.
      virtual bool check_properties() const
      {
	for (std::vector<GraphProperty>::iterator it = property_list.begin(); it != property_list.end(); it++){

	  GraphProperty.check_property(device_manager);
	}
      }
      */

      /** \brief Update reference graph to satisfy properties
       * NOTE: This function will probably use the device manager to
       *       change the reference graph.
       */
      virtual void update_graph() const = 0;
      
    private:
      std::vector<GraphProperty> property_list_;
      DeviceManager& device_manager_;
      
    }; // class PropertyCheckUp

} // namespace skynet

#endif /* SKYNET_PROPERTYCHECKUP_HPP__ */

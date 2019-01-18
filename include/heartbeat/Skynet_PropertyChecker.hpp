#ifndef SKYNET_PROPERTYCHECKER_HPP__
#define SKYNET_PROPERTYCHECKER_HPP__

#include <vector>

#include "Skynet_GraphProperty.hpp"
#include "Skynet_DeviceManager.hpp"

namespace skynet
{
    /** \class PropertyChecker
     *  \brief Abstract class, interface for storing graph properties
     *	that a device cares about and checking these properties.
     *	PropertyChecker will also be responsible for updating the
     *	reference graph if the required properties are not met.
     */
    class PropertyChecker
    {
    public:

      /** \brief Constructor for PropertyChecker
       *
       *  \param property_list List of properties to be checked
       *  \param device_manager Device manager for heart, which can be used
       *	to get list of nearby devices and update this list if properties
       *	aren't satisfied.
       */
      //  PropertyChecker(std::vector<GraphProperty> property_list) :
      //  property_list_(std::move(property_list))
      //{
      //}

      /** \brief PropertyChecker destructor
       */
      virtual ~PropertyChecker() = default;

      /** \brief Get list of properties required by this device
       */
      const std::vector<GraphProperty>& get_property_list()
      {
          return property_list_;
      }

      /** NOTE: Combining check_properties and update_graph functions for now
       * because I think it could save time to perform the two functions
       * simultaneously and I expect anytime we checked the graph properties we
       * would also want to update the graph if they weren't satisfied. Change
       * this later if not true.
       */
//      /** \brief Check that reference graph satisfies the desired properties
//       *
//       * \param device_manager Device manager that will be used to get the list
//       *  of nearby devices, which is required to check the properties
//       */
//      virtual bool check_properties(const DeviceManager& device_manager) const
//      {
//	for (std::vector<GraphProperty>::iterator it = property_list.begin(); it != property_list.end(); it++){
//
//	  GraphProperty.check_property(device_manager);
//	}
//      }

//      /** \brief Update reference graph to satisfy properties
//       *
//       * \param device_manager Device manager that will be used to update nearby
//       *  device list if the properties are not satisfied
//       */
//      virtual void update_graph(const DeviceManager& device_manager) const = 0;


       /** \brief Check that reference graph satisfies properties and update
	*   as necessary if now.
	*
	* \param device_manager Device manager that will be used to get the nearby
	*  devices to check the properties and update the nearby device list if the
	*  properties are not satisfied
       */
      void validate_graph(DeviceManager& device_manager)
      {
	do_validate_graph(device_manager);
      }

    private:
      virtual void do_validate_graph(DeviceManager& device_manager) const = 0;

    private:
      std::vector<GraphProperty> property_list_;

    }; // class PropertyChecker

} // namespace skynet

#endif /* SKYNET_PROPERTYCHECKER_HPP__ */

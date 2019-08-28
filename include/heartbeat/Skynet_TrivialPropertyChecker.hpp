#ifndef SKYNET_TRIVIALPROPERTYCHECKER_HPP__
#define SKYNET_TRIVIALPROPERTYCHECKER_HPP__

#include <vector>

#include "Skynet_PropertyChecker.hpp"

namespace skynet
{
    /** \class PropertyChecker
     *  \brief Abstract class, interface for storing graph properties
     *	that a device cares about and checking these properties.
     *	PropertyChecker will also be responsible for updating the
     *	reference graph if the required properties are not met.
     */
    class TrivialPropertyChecker : public PropertyChecker
    {
    public:

      TrivialPropertyChecker()
      { }

      // const std::vector<GraphProperty>& get_property_list() const
      // {
      //     return property_list_;
      // }
      /** \brief Get list of properties required by this device
       */


    private:
      void do_validate_graph(DeviceManager& /* device_manager */) const override
      {

      }


    }; // class TrivialPropertyChecker

} // namespace skynet

#endif /* SKYNET_PROPERTYCHECKER_HPP__ */

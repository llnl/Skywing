#ifndef HEART_HELPER_HPP__
#define HEART_HELPER_HPP__

#include "ns3/application-container.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"
#include "Ns3_Heart.hpp"

namespace ns3
{
  class HeartHelper
  {
  public:
    HeartHelper()
    {
      factory_.SetTypeId(Heart::GetTypeId());
    }

    ApplicationContainer Install(NodeContainer c) const
    {
      ApplicationContainer apps;
      for (NodeContainer::Iterator i = c.Begin(); i != c.End(); i++)
      {
        Ptr<Node> node = *i;
        Ptr<Application> app = factory_.Create<Heart>();
        node->AddApplication(app);
        apps.Add(app);
      }
      return apps;
    }

  private:
    ObjectFactory factory_;
  }; // class HeartHelper
} // namespace ns3

#endif /* HEART_HELPER_HPP__ */

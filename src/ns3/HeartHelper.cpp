#include "HeartHelper.hpp"

#include "ns3/ptr.h"
#include "Ns3_Heart.hpp"

namespace ns3
{
  HeartHelper::HeartHelper()
  {
    factory_.SetTypeId(Heart::GetTypeId());
  }

  ApplicationContainer HeartHelper::Install(NodeContainer c) const
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
} // namespace ns3

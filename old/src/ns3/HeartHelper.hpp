#ifndef HEART_HELPER_HPP__
#define HEART_HELPER_HPP__

#include "ns3/application-container.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"
#include "skyns3/Ns3_Heart.hpp"

namespace ns3
{
  /** \class HeartHelper
   * \brief Provides helper routines for the Ns3_Heart class
   *
   * Contains installation method and an ObjectFactory for use  of Ns3_Heart
   * applications in ns3
   */
  class HeartHelper
  {
  public:
    /** \brief Construct a new HeartHelper.
     *
     */
    HeartHelper(uint16_t port)
    {
      factory_.SetTypeId(Heart::GetTypeId());
      SetAttribute("Port", UintegerValue(port));
    }

    HeartHelper(Address server1, uint16_t port)
    {
      factory_.SetTypeId(Heart::GetTypeId());
      SetAttribute("ServerAddress1", AddressValue(server1));
      SetAttribute("Port", UintegerValue(port));
    }

    HeartHelper(Address server1, Address server2, uint16_t port)
    {
      factory_.SetTypeId(Heart::GetTypeId());
      SetAttribute("ServerAddress1", AddressValue(server1));
      SetAttribute("ServerAddress2", AddressValue(server2));
      SetAttribute("Port", UintegerValue(port));
    }

    HeartHelper(Address server1, Address server2, Address server3, uint16_t port)
    {
      factory_.SetTypeId(Heart::GetTypeId());
      SetAttribute("ServerAddress1", AddressValue(server1));
      SetAttribute("ServerAddress2", AddressValue(server2));
      SetAttribute("ServerAddress3", AddressValue(server3));
      SetAttribute("Port", UintegerValue(port));
    }

    /** \brief Install an Ns3_Heart in each Node in a NodeContainer
     *
     * \param c NodeContainer containing Nodes to install Ns3_Hearts into
     */
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
    void SetAttribute (std::string name, const AttributeValue &value)
    {
      factory_.Set (name, value);
    }

    ObjectFactory factory_;
  }; // class HeartHelper
} // namespace ns3

#endif /* HEART_HELPER_HPP__ */

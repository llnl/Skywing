#ifndef HEART_HELPER_HPP__
#define HEART_HELPER_HPP__

#include "ns3/application-container.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"

namespace ns3
{
  class HeartHelper
  {
  public:
    HeartHelper();
    ApplicationContainer Install(NodeContainer c) const;
  private:
    ObjectFactory factory_;
  }; // class HeartHelper
} // namespace ns3

#endif /* HEART_HELPER_HPP__ */

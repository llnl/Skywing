#include "Ns3_Heart.hpp"

#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"

namespace ns3
{
  NS_LOG_COMPONENT_DEFINE("HeartApplication");
  NS_OBJECT_ENSURE_REGISTERED(Heart);

  TypeId Heart::GetTypeId()
  {
    static TypeId tid = TypeId("Heart")
      .SetParent<Application>()
      .SetGroupName("Applications")
      .AddConstructor<Heart>()
      .AddAttribute("LocalAddress",
                    "The source Address of the outbound packets",
                    Ipv4AddressValue(),
                    MakeIpv4AddressAccessor(&Heart::local_ip_),
                    MakeIpv4AddressChecker())
      .AddAttribute("RemoteAddress",
                    "The destination Address of the outbound packets",
                    Ipv4AddressValue(),
                    MakeIpv4AddressAccessor(&Heart::remote_ip_),
                    MakeIpv4AddressChecker())
      .AddAttribute("PortStart",
                    "The port number to start with",
                    UintegerValue(100),
                    MakeUintegerAccessor(&Heart::port_),
                    MakeUintegerChecker<uint16_t>())
    ;
    return tid;
  }

  Heart::Heart()
  {
    NS_LOG_FUNCTION(this);
  }

  Heart::~Heart()
  {
    NS_LOG_FUNCTION(this);
  }

  void Heart::DoDispose()
  {
    NS_LOG_FUNCTION(this);
    Application::DoDispose();
  }

  void Heart::StartApplication()
  {
    NS_LOG_FUNCTION(this);
    // schedule the begin_heartbeat event to occur at time 0
    begin_heartbeat_event_ = Simulator::Schedule(Seconds(0.0),
                                                 &Heart::begin_heartbeat, this);
  }

  void Heart::StopApplication()
  {
    NS_LOG_FUNCTION(this);
    Simulator::Cancel(begin_heartbeat_event_);
  }

  void Heart::begin_heartbeat()
  {
    NS_LOG_FUNCTION(this);
    NS_ASSERT(begin_heartbeat_event_.IsExpired());

    // begin the Skynet Heartbeat
    std::vector<const char *> ip_addresses(0);
    heart_.begin_heartbeat(ip_addresses, port_);
  }
} // namespace ns3

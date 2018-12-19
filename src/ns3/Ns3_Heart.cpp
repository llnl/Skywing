#include "Ns3_Heart.hpp"

#include <arpa/inet.h>
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
      .AddAttribute("ServerAddress1",
                    "The first server address",
                    AddressValue(),
                    MakeAddressAccessor(&Heart::server_address1_),
                    MakeAddressChecker())
      .AddAttribute("ServerAddress2",
                    "The second server address",
                    AddressValue(),
                    MakeAddressAccessor(&Heart::server_address2_),
                    MakeAddressChecker())
      .AddAttribute("ServerAddress3",
                    "The second server address",
                    AddressValue(),
                    MakeAddressAccessor(&Heart::server_address3_),
                    MakeAddressChecker())
      .AddAttribute("Port",
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

    // TODO: generalize this to other address formats
    std::vector<const char *> server_addresses;
    if (!server_address1_.IsInvalid())
    {
      if (Ipv4Address::IsMatchingType(server_address1_) )
      {
        // use arpa/inet to convert from uint32_t to const char *
        struct sockaddr_in sa;
        char str[INET_ADDRSTRLEN];
        sa.sin_addr.s_addr = (Ipv4Address::ConvertFrom(server_address1_)).Get();
        // add to server_addresses vector
        server_addresses.push_back(
          inet_ntop(AF_INET, &sa.sin_addr, str, INET_ADDRSTRLEN) );
      }
      else
        NS_FATAL_ERROR ("Unrecognized address format");
    }

    // begin the Skynet Heartbeat
    heart_.begin_heartbeat(server_addresses, port_);
  }
} // namespace ns3

#include "Ns3_Heart.hpp"

#include "ns3/log.h"

namespace ns3 
{
  NS_LOG_COMPONENT_DEFINE("HeartApplication");
  NS_OBJECT_ENSURE_REGISTERED(Heart)

  TypeId Heart::GetTypeId()
  {
    static TypeId tid = TypeId("Heart")
      .SetParent<Application>()
      .SetGroupName("Applications")
      .AddConstructor<Heart>()
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
    heart_ = skynet::Heart();
    begin_heartbeat_event_ = Simulator::Schedule(Seconds(0.0), &Heart::begin_heartbeat, this);
  }

  void Heart::StopApplication()
  {
    NS_LOG_FUNCTION(this);
    Simulator::Cancel(begin_heartbeat_event_);
  }

  void Heart::begin_heartbeat() const
  {
    NS_LOG_FUNCTION(this);
    NS_ASSERT(begin_heartbeat_event_.IsExpired());
    _heart.begin_heartbeat();
  }
} // namespace ns3

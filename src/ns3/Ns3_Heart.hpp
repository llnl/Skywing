#ifndef NS3_HEART_HPP__
#define NS3_HEART_HPP__

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "heartbeat/Skynet_Heart.hpp"

namespace ns3
{
  class Heart : public Application
  {
  public:
  
    static TypeId GetTypeId();

    Heart();

    virtual ~Heart();

  protected:
    virtual void DoDispose();

  private:
    virtual void StartApplication();
    virtual void StopApplication();
    void begin_heartbeat() const;
    EventId begin_heartbeat_event_;
    skynet::Heart heart_;
  }; // class Heart
} // namespace ns3

#endif /* NS3_HEART_HPP__ */

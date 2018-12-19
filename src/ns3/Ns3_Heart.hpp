#ifndef NS3_HEART_HPP__
#define NS3_HEART_HPP__

#include "ns3/application.h"
#include "ns3/event-id.h"
//TODO: swap these includes once Heart Module is developed
//#include "heartbeat/Skynet_Heart.hpp"
#include "tmp/Skynet_Heart.hpp"

namespace ns3
{
  /** \class (ns3) Heart
   * \brief An ns3 wrapper for the Skynet Heart class
   *
   * This wraps the Skynet Heart as an ns3 Application
   */
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
    void begin_heartbeat();
    EventId begin_heartbeat_event_;
    skynet::Heart heart_;
  }; // class Heart
} // namespace ns3

#endif /* NS3_HEART_HPP__ */

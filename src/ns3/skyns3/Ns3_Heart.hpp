#ifndef NS3_HEART_HPP__
#define NS3_HEART_HPP__

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ipv4-address.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "heartbeat/Skynet_Heart.hpp"
#include "devices/Skynet_DeviceCommunicator.hpp"
#include "Ns3_SocketCommunicatorFactory.hpp"

namespace ns3
{
  /** \class (ns3) Heart
   * \brief An ns3 wrapper for the Skynet Heart class
   *
   * This wraps the Skynet Heart as an ns3 Application
   */
  class Heart : public Application, public skynet::Heart
  {
  public:

    // from Application ?
    static TypeId GetTypeId();

    Heart();

    virtual ~Heart();

  protected:

    // from Application
    virtual void DoDispose();

  private:

    // from Application
    virtual void StartApplication (void);
    virtual void StopApplication (void);

    // from skynet::Heart
    virtual void begin_heartbeat();

    EventId begin_heartbeat_event_;
    Address server_address1_;
    Address server_address2_;
    Address server_address3_;
    uint16_t port_;

    // TODO: remove this at some point
    std::vector<std::unique_ptr<skynet::DeviceCommunicator>> comm_list;
  }; // class Heart
  
} // namespace ns3

#endif /* NS3_HEART_HPP__ */

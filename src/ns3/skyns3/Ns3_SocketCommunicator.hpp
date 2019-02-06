#ifndef NS3_SOCKETCOMMUNICATOR_HPP__
#define NS3_SOCKETCOMMUNICATOR_HPP__

#include "ns3/application.h"
#include "ns3/log.h"
#include "ns3/seq-ts-header.h"
#include "ns3/simulator.h"
#include "ns3/socket.h"
#include "ns3/uinteger.h"
#include "devices/Skynet_DeviceCommunicator.hpp"

namespace ns3
{
  class SocketCommunicator : public Application, public skynet::DeviceCommunicator
  {
  public:

    // from Application ?
    static TypeId GetTypeId();

    SocketCommunicator();

    virtual ~SocketCommunicator();

    const bool success() const;

  protected:

    virtual void DoDispose();

  private:

    void do_send_to_(const void* data, std::size_t data_size) const override;
    std::vector<char> do_receive_from_() const override;

    virtual void StartApplication(void);
    virtual void StopApplication(void);

    void HandleRead (Ptr<Socket> socket);

    void HandleSend(void);

    EventId sendEvent_;
    Ptr<Socket> socket_;
    Address address_;
    uint16_t port_;

    // TODO: replace this with error checking
    bool success_;

  }; // class SocketCommunicator

} // namespace ns3


#endif /* NS#_SOCKETCOMMUNICATOR_HPP__ */

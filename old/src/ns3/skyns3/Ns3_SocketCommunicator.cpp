#include "Ns3_SocketCommunicator.hpp"

namespace ns3
{

  NS_LOG_COMPONENT_DEFINE("SocketCommunicatorApplication");
  NS_OBJECT_ENSURE_REGISTERED(SocketCommunicator);

  TypeId SocketCommunicator::GetTypeId()
  {
    static TypeId tid = TypeId("ns3::SocketCommunicator")
      .SetParent<Application>()
      .SetGroupName("SkyNs3")
      .AddConstructor<SocketCommunicator>()
      .AddAttribute("Address",
                    "some comments here",
                    AddressValue(),
                    MakeAddressAccessor(&SocketCommunicator::address_),
                    MakeAddressChecker())
      .AddAttribute("Port",
                    "some comments here",
                    UintegerValue(100),
                    MakeUintegerAccessor(&SocketCommunicator::port_),
                    MakeUintegerChecker<uint16_t>())
    ;
    return tid;
  }

  SocketCommunicator::SocketCommunicator()
  {
    NS_LOG_FUNCTION(this);
  }

  SocketCommunicator::~SocketCommunicator()
  {
    NS_LOG_FUNCTION(this);
  }

  // Definition of Application::DoDispose
  void SocketCommunicator::DoDispose()
  {
    NS_LOG_FUNCTION(this);
    Application::DoDispose();
  }

  void SocketCommunicator::StartApplication()
  {
    NS_LOG_FUNCTION(this);
    success_ = false;
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    socket_ = Socket::CreateSocket(GetNode(), tid);
    if (address_.IsInvalid())
    {
      InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port_);
      if (socket_->Bind(local) == -1)
        NS_FATAL_ERROR ("Failed to bind server socket");
    }
    else
    {
      if (socket_->Bind () == -1)
        NS_FATAL_ERROR ("Failed to bind client socket");
      socket_->Connect(InetSocketAddress(Ipv4Address::ConvertFrom(address_), port_));
    }

    success_ = true;
    socket_->SetRecvCallback(MakeCallback(&SocketCommunicator::HandleRead, this));
    socket_->SetAllowBroadcast (true);
  }


  void SocketCommunicator::StopApplication()
  {
    Simulator::Cancel (sendEvent_);
    if (socket_ != 0)
      socket_->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
  }

  const bool SocketCommunicator::success() const
  { return success_; }

  void SocketCommunicator::HandleRead (Ptr<Socket> socket)
  {
    // do stuff
  }

  void SocketCommunicator::HandleSend(void)
  {
    NS_ASSERT(sendEvent_.IsExpired());
    SeqTsHeader seqTs;
    seqTs.SetSeq(0);
    int size = 10;
    Ptr<Packet> p = Create<Packet> (size-(8+4)); // 8+4 : the size of the seqTs header
    p->AddHeader (seqTs);

    if ((socket_->Send(p)) >= 0)
    {
      printf("I sort of sent stuff!\n");
      //NS_LOG_INFO ("TraceDelay TX " << m_size << " bytes to "
      //                          << peerAddressStringStream.str () << " Uid: "
      //                          << p->GetUid () << " Time: "
      //                          << (Simulator::Now ()).GetSeconds ());

    }
    else
    {
      //NS_LOG_INFO ("Error while sending " << m_size << " bytes to "
        //                              << peerAddressStringStream.str ());
    }
  }

  void SocketCommunicator::do_send_to_(const void* data, std::size_t data_size) const
  {
    printf("I sent stuff!\n");

  }

  std::vector<char> SocketCommunicator::do_receive_from_() const
  {
    printf("I received stuff!\n");
    return std::vector<char>(0);
  }

} // namespace ns3

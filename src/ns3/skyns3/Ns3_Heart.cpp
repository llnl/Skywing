#include "Ns3_Heart.hpp"

namespace ns3
{

  NS_LOG_COMPONENT_DEFINE("HeartApplication");
  NS_OBJECT_ENSURE_REGISTERED(Heart);

  // Definition of Application::TypeId
  TypeId Heart::GetTypeId()
  {
    static TypeId tid = TypeId("ns3::Heart")
      .SetParent<Application>()
      .SetGroupName("SkyNs3")
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

  // Definition of Application::DoDispose
  void Heart::DoDispose()
  {
    NS_LOG_FUNCTION(this);
    Application::DoDispose();
  }

  // Definition of Application::StartApplication
  void Heart::StartApplication()
  {
    NS_LOG_FUNCTION(this);
    // schedule the begin_heartbeat event to occur at time 0
    begin_heartbeat_event_ = Simulator::Schedule(Seconds(0.0),
                                                 &Heart::begin_heartbeat, this);
  }

  // Definition of Application::StopApplication
  void Heart::StopApplication()
  {
    NS_LOG_FUNCTION(this);
    Simulator::Cancel(begin_heartbeat_event_);

    // clear comm_list so all sockets are destroyed (and events canceled)
    comm_list.clear();
  }

  // Definition of skynet::Heart:begin_heartbeat
  void Heart::begin_heartbeat()
  {
    NS_LOG_FUNCTION(this);
    NS_ASSERT(begin_heartbeat_event_.IsExpired());

    std::cout << "I'm alive!!" << std::endl;
    std::vector<std::string> config(0);

    // for each ip_address, create a client communicator
    if (!server_address1_.IsInvalid())
    {
      // TODO: generalize this to other address formats
      if (Ipv4Address::IsMatchingType(server_address1_) )
      {
        // use arpa/inet to convert from uint32_t to const char *
        struct sockaddr_in sa;
        char str[INET_ADDRSTRLEN];
        sa.sin_addr.s_addr = (Ipv4Address::ConvertFrom(server_address1_)).Get();
        // add to server_addresses vector
        SocketCommunicatorFactory client_factory(
          SocketCommunicatorFactory::IPv4,
          inet_ntop(AF_INET, &sa.sin_addr, str, INET_ADDRSTRLEN),
          port_,
          GetNode());
        comm_list.push_back(client_factory.create_new_communicator(config));
      }
      else
        NS_FATAL_ERROR ("Unrecognized address format");
    }

    //TODO: implement server_address2_ and server_address3_

    // create listening communicator
    SocketCommunicatorFactory server_factory(SocketCommunicatorFactory::IPv4, port_, GetNode());
    comm_list.push_back(server_factory.create_new_communicator(config));
  }

} // namespace ns3

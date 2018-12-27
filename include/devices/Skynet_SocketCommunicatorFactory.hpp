#ifndef SKYNET_SOCKETCOMMUNICATORFACTORY_HPP__
#define SKYNET_SOCKETCOMMUNICATORFACTORY_HPP__

#include <arpa/inet.h>
#include "Skynet_CommunicatorFactory.hpp"
#include "Skynet_SocketCommunicator.hpp"


namespace skynet
{
  class SocketCommunicatorFactory : public CommunicatorFactory
  {
  public:
    using data_type = std::vector<std::unique_ptr<DeviceCommunicator>>;
    static const int IPv4 = AF_INET;
  public:
      SocketCommunicatorFactory() = default;
 /** \brief Create a new SocketCommunicatorFactory.
     *
     * The ip_address of the device we wish to communicated is needed.
     * For now the ip_addresses are a list of its that can be sorted easily.
     * This will need to change once we are not working on one machine.
     */
    SocketCommunicatorFactory(int type, uint16_t port_for_negotiator) :
      type_(type), server_address_{""}
    { 
      check_for_supported_type(); 
      // create and bind server port negotiator SocketCommunicator
      port_negotiator_ = std::make_unique<SocketCommunicator>(SocketCommunicator(type_));
      current_port_ = port_negotiator_->bind_communicator(port_for_negotiator);
      if (current_port_ != port_for_negotiator)
      {
        printf("Server SocketComunicatorFactory created on non-negotiator port\n");
        exit(-1);
      }
      printf("Server port negotiator created on port %d\n", port_for_negotiator);
      current_port_++;
      // wait for connection from client port negotiator socket
      client_address_ = port_negotiator_->wait_for_client(ipv4Buf_); //TODO: generalize from IPv4
      printf("Server port negotiator connected to %s on port %d\n", client_address_, port_for_negotiator);
    }

    SocketCommunicatorFactory(int type, const char * server_address, uint16_t port_for_negotiator) :
      type_(type), server_address_{server_address}
    { 
      check_for_supported_type(); 
      // create client port negotiator SocketCommunicator
      port_negotiator_ = std::make_unique<SocketCommunicator>(SocketCommunicator(type_));
      // wait for connection to server port negotiator SocketCommunicator
      port_negotiator_->connect_to_server(server_address, port_for_negotiator);
      printf("Client port negotiator connected to %s on port %d\n", server_address_, port_for_negotiator);
    }

    std::unique_ptr<DeviceCommunicator>
    create_new_communicator(std::vector<std::string> /* comm_config_info*/)
    {
      std::unique_ptr<SocketCommunicator> new_communicator;
      // if factory is server side
      if ( std::strcmp(server_address_ ,"") == 0)
      {
        // create and bind new server SocketCommunicator
        new_communicator = std::make_unique<SocketCommunicator>(SocketCommunicator(type_));
        current_port_ = new_communicator->bind_communicator(current_port_, client_address_);
        printf("Server created on port %d, listening for %s\n", current_port_, client_address_);
        // communicate port that server bound to over to client port negotiator
        port_negotiator_->send_to<uint16_t>(current_port_);
        new_communicator->wait_for_client(); 
        current_port_++;
      }
      // if factory is client side
      else
      {
        // create new client SocketCommunicator and connect 
        new_communicator = std::make_unique<SocketCommunicator>(SocketCommunicator(type_));
        current_port_ = port_negotiator_->receive_from<uint16_t>();
        new_communicator->connect_to_server(server_address_, current_port_);
        printf("Client created and connected to %s on port %d\n", server_address_, current_port_);
      }

      return new_communicator;
    }
    // data_type& get_as_nonconst_vector() { return communicators_; }

  private:

    void check_for_supported_type() const
    {
      // check that socket type is supported
      if (type_ != IPv4)
      {
        printf("incorrect socket type\n");
        exit(-1);
      }
    }

    int type_;
    const char * client_address_;
    const char * server_address_;
    uint16_t current_port_;
    std::unique_ptr<SocketCommunicator> port_negotiator_;
    char ipv4Buf_[INET_ADDRSTRLEN];
  }; // SocketCommunicatorFactory
} // namespace skynet

#endif /* SKYNET_SOCKETCOMMUNICATORFACTORY_HPP__ */

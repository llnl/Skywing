#ifndef SKYNET_SOCKETCOMMUNICATORSERVERFACTORY_HPP__
#define SKYNET_SOCKETCOMMUNICATORSERVERFACTORY_HPP__

#include <arpa/inet.h>
#include "Skynet_CommunicatorFactory.hpp"
#include "Skynet_SocketCommunicator.hpp"
#include "Skynet_SocketListener.hpp"

namespace skynet
{
  class SocketCommunicatorFactory : public CommunicatorFactory
  {
  public:
    using data_type = std::vector<std::unique_ptr<DeviceCommunicator>>;
  public:
    /** \brief Create a new server SocketCommunicatorFactory.
     *
     * create a SocketListener for this factory and then use Gateway listener
     * to create a handshake communicator to communicate factory listening port
     * back to client
     *
     * \param type Specifies the address type to be used (IPv4).
     * \param skynet_port Specifies the port used by the Gateway
     * \param listener_on_factory_port_  Specifies the port for the other
     *    device's SocketListener that is held in the respective SocketCommunicatorFactory
     */
     SocketCommunicatorFactory(const int type, const uint16_t port_listener_remote_factory, const uint16_t skynet_port) :
      type_(type), port_listener_remote_factory_(port_listener_remote_factory)

    {
      local_port_ = skynet_port;
      type_ = type;
      // create a new SocketListener to be used by SocketCommunicatorFactory
      listener_= std::make_unique<SocketListener>(type_, skynet_port+1, true);

    }

    /** \brief Create a new client SocketCommunicatorFactory.
     *
     * communicate with server SocketCommunicatorFactory to determine what port
     * the server SocketListener is listening on
     *
     * \param type Specifies the address type to be used (IPv4).
     * \param server_address The IP address of the server to connect to
     * \param skynet_port The specific port that all SocketGatekeepers will be
     * listening on
     *
     */
    SocketCommunicatorFactory(const int type, const char* const server_address,
      const uint16_t skynet_port_of_connecting_device,  const uint16_t skynet_port) : type_(type), server_address_(server_address)
    {
      // TODO: implement error catching if this process failes
      // connect to Gatekeeper on server device using handshake SocketCommunicator
      local_port_ = skynet_port;
      SocketCommunicator handshake(type_);
      handshake.connect_to_server(server_address_, skynet_port_of_connecting_device);
      port_listener_remote_factory_ = handshake.receive_from<uint16_t>();

      listener_ = std::make_unique<SocketListener>(type_, skynet_port+1, true);

      // obtain gateway port on server via server Gatekeeper, then close
      // handshake SocketCommunicator
      handshake.send_to<uint16_t>(listener_->get_socket_port());

    }



    void update_port_listener_remote_factory(const uint16_t port_listener_remote_factory)
    {
      port_listener_remote_factory_= port_listener_remote_factory;
    }


    /** \brief Create a new SocketCommunicator from Factory.
     *
     */
    std::unique_ptr<DeviceCommunicator>
    create_new_communicator(const std::vector<std::string>& /* comm_config_info*/) override
    {
      // connect to new Socket Communicator with Socket Factores using
      // handshake SocketCommunicator
      SocketCommunicator handshake(type_);
      handshake.connect_to_server(server_address_, port_listener_remote_factory_);

      // Wait till you have a response from other device factory listener
      // to get port number of new Socket Communicator
      uint16_t new_port=0;
      bool response = false;
      while(!response) {
        new_port = handshake.receive_from<uint16_t>();
        if(new_port != 0){
          response = true;
        }
      }

      //Create new communicator to the over devices Socket Communicator
      std::unique_ptr<SocketCommunicator> new_communicator =
        std::make_unique<SocketCommunicator>(type_);
      new_communicator->connect_to_server(server_address_, new_port);

      // Send response back that connection occured and close connection
      handshake.send_to<int>(-1);

      return new_communicator;
    }

    /** \brief Listen for requests to make a new SocketCommunicator from
     * Factory and then create Communicator.
     *
     */
    std::vector<std::unique_ptr<DeviceCommunicator>>
    create_requested_communicators()
    {
      std::vector<std::unique_ptr<DeviceCommunicator>> new_communicator;
      // connect to other Socket Factory using this factories listener and
      // see if there are any connection requests
      // handshake SocketCommunicator
      std::unique_ptr<SocketCommunicator> handshake =
        listener_->connect_communicator_to_client();

      // Create new listener to be the bridge and listen to new connections
      std::unique_ptr<SocketListener> new_listener =
        std::make_unique<SocketListener>(type_, 1, true);

      //send listener port
      handshake->send_to<uint16_t>(new_listener->get_socket_port());

      // Make sure they connect to the listern first
      uint16_t new_port=0;
      bool response = false;
      while(!response) {
        new_port= handshake->receive_from<int>();
        if(new_port != 0){
          response = true;
        }
      }

      // Conect to the client with a new communicator
      new_communicator.push_back(new_listener->connect_communicator_to_client());

      //close handshake
      return new_communicator;
    }

    uint16_t get_port_listener_remote_factory() const
    {
      // std::cout<<"local_port "<< local_port_<<" port = "<<listener_->get_socket_port()<<std::endl;
      return listener_->get_socket_port();
    }

  private:

    int type_;
    const char* server_address_ = " ";
    uint16_t local_port_;
    //The port number of the SocketListener in the corresponding device's SocketCommunicatorFactory
    uint16_t port_listener_remote_factory_;
    std::unique_ptr<SocketListener> listener_;
  }; // SocketCommunicatorFactory
} // namespace skynet

#endif /* SKYNET_SOCKETCOMMUNICATORFACTORY_HPP__ */

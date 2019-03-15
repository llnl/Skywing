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
     */
     SocketCommunicatorFactory(int type, std::unique_ptr<SocketListener> listener, uint16_t listener_on_factory_port, uint16_t skynet_port) :
      type_(type),skynet_port_(skynet_port), listener_on_factory_port_(listener_on_factory_port)

    {
      type_ = type;
      is_server = true;
      listener_ = std::make_unique<SocketListener>(type_, skynet_port+1, true);
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
    SocketCommunicatorFactory(int type, const char * server_address,
      uint16_t skynet_port_of_connecting_device,  uint16_t skynet_port) : type_(type), server_address_(server_address), skynet_port_(skynet_port)
    {
      // TODO: implement error catching if this process failes
      is_server = false;
      // connect to Gatekeeper on server device using handshake SocketCommunicator
      SocketCommunicator handshake(type_);
      handshake.connect_to_server(server_address_, skynet_port_of_connecting_device);
      listener_on_factory_port_ = handshake.receive_from<uint16_t>();

      listener_ = std::make_unique<SocketListener>(type_, skynet_port_+1, true);

      // obtain gateway port on server via server Gatekeeper, then close
      // handshake SocketCommunicator
      handshake.send_to<uint16_t>(listener_->get_socket_port());
      handshake.close_connection();

    }


    /** \brief Create a new SocketCommunicator from Factory.
     *
     */
    std::unique_ptr<DeviceCommunicator>
    create_new_communicator(std::vector<std::string> /* comm_config_info*/)
    {
      // connect to new Socket Communicator with Socket Factores using
      // handshake SocketCommunicator
      SocketCommunicator handshake(type_);
      handshake.connect_to_server(server_address_, listener_on_factory_port_);

      // Wait till you have a response from other device factory listener
      // to get port number of new Socket Communicator
      uint16_t new_port=0;
      bool response = false;
      while(!response) {
        new_port= handshake.receive_from<uint16_t>();
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
       handshake.close_connection();


        return new_communicator;
    }

    /** \brief Listen for requests to make a new SocketCommunicator from
     * Factory and then create Communicator.
     *
     */
    std::unique_ptr<DeviceCommunicator>
    listen_for_new_request()
    {
      // connect to other Socket Factory using this factories listener and
      // see if there are any connection requests
      // handshake SocketCommunicator
      std::unique_ptr<SocketCommunicator> handshake =
        listener_->connect_communicator_to_client();

        // Create new listener to be the bridge and listen to new connections
        std::unique_ptr<SocketListener> new_listener =
          std::make_unique<SocketListener>(type_, skynet_port_+1, true);

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
        std::unique_ptr<SocketCommunicator> new_communicator =
            new_listener->connect_communicator_to_client();

        //close handshake
        handshake->close_connection();
        return new_communicator;
      // }
    }

  private:

    int type_;
    const char * server_address_ = "127.0.0.1";
    bool is_server;
    uint16_t skynet_port_;
    uint16_t listener_on_factory_port_;
    std::unique_ptr<SocketListener> listener_;
  }; // SocketCommunicatorFactory
} // namespace skynet

#endif /* SKYNET_SOCKETCOMMUNICATORFACTORY_HPP__ */

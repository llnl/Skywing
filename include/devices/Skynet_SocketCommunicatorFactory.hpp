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
     * \param type Specifies the address type to be used (IPv4).
     * \param gateway The SocketListener for incoming connections
     */
    SocketCommunicatorFactory(int type, std::unique_ptr<SocketListener> listener, uint16_t server_port, uint16_t skynet_port) :
      type_(type),skynet_port_(skynet_port), server_port_(server_port)
    {
      listener_ = std::move(listener);
      is_server = true;
    }

    /** \brief Create a new client SocketCommunicatorFactory.
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
      std::cout<<"Creating Facotry to connect to Device " << skynet_port <<std::endl;
      // TODO: implement error catching if this process failes
      is_server = false;
      // connect to Gatekeeper on server device using handshake SocketCommunicator
      SocketCommunicator handshake(type_);
      handshake.connect_to_server(server_address_, skynet_port_of_connecting_device);
      server_port_ = handshake.receive_from<uint16_t>();
      std::cout<<"Connect to Device " << skynet_port <<" with new port "<< server_port_<<std::endl;
      listener_ = std::make_unique<SocketListener>(type_, skynet_port_+1, false);

      // obtain gateway port on server via server Gatekeeper, then close
      // handshake SocketCommunicator
      handshake.send_to<uint16_t>(listener_->get_port());
      // handshake.close_connection();
      std::cout<<"Connect to Device " << server_port_ <<" with new port "<< listener_->get_port()<<std::endl;

    }

    std::unique_ptr<DeviceCommunicator>
    create_new_communicator(std::vector<std::string> /* comm_config_info*/)
    {
      std::cout<<"Device " <<skynet_port_<< "wants to create new communicator......"<<std::endl;
      SocketCommunicator handshake(type_);
      handshake.connect_to_server(server_address_, server_port_);

      uint16_t new_port=0;
      bool response = false;
      while(!response) {
        new_port= handshake.receive_from<uint16_t>();
        if(new_port != 0){
          std::cout<< "new port number = "<<new_port<<std::endl;
          response = true;
        }
      }

      std::unique_ptr<SocketCommunicator> new_communicator =
        std::make_unique<SocketCommunicator>(type_);
        new_communicator->connect_to_server(server_address_, new_port);

       handshake.send_to<int>(-1);


        return new_communicator;
      // }
    }

    std::unique_ptr<DeviceCommunicator>
    listen_for_new_request()
    {
      std::cout<<"Devive "<< skynet_port_<< " is connecting to listen on port "<< server_port_<<std::endl;
      std::unique_ptr<SocketCommunicator> handshake =
        listener_->connect_communicator_to_client();

        std::unique_ptr<SocketListener> new_listener =
          std::make_unique<SocketListener>(type_, skynet_port_+1, true);

        //
        // std::unique_ptr<SocketListener> new_listener =
        //   std::make_unique<SocketListener>(type_, 1, true);
        //
        //   std::cout<< "listen_for_new_request new port number = "<<new_listener->get_port()<<std::endl;
          handshake->send_to<uint16_t>(new_listener->get_port());
        //
        //   std::unique_ptr<SocketCommunicator> new_communicator =
        //     new_listener->connect_communicator_to_client();

        uint16_t new_port=0;
        bool response = false;
        while(!response) {
          new_port= handshake->receive_from<int>();
          if(new_port != 0){
            std::cout<< "Got a response"<<std::endl;
            response = true;
          }
        }

            std::cout<<"Devive create connection!" << new_listener->count_pending_clients()<<std::endl;

              std::unique_ptr<SocketCommunicator> new_communicator =
                  new_listener->connect_communicator_to_client();
        return new_communicator;
      // }
    }

  private:

    int type_;
    const char * server_address_ = "127.0.0.1";
    bool is_server;
    uint16_t skynet_port_;
    uint16_t server_port_;
    std::unique_ptr<SocketListener> listener_;
  }; // SocketCommunicatorFactory
} // namespace skynet

#endif /* SKYNET_SOCKETCOMMUNICATORFACTORY_HPP__ */

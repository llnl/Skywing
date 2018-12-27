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
    SocketCommunicatorFactory(int type, uint16_t port_start) :
      type_(type), server_address_{""}, port_{port_start}
    { check_for_supported_type(); }

    SocketCommunicatorFactory(int type, const char * server_address,
      uint16_t port_start) : type_(type), server_address_{server_address}, port_{port_start}
    { check_for_supported_type(); }

    std::unique_ptr<DeviceCommunicator>
    create_new_communicator(std::vector<std::string> /* comm_config_info*/)
    {
      std::unique_ptr<SocketCommunicator> sc;
      // if factory is server side
      if ( std::strcmp(server_address_ ,"") == 0)
      {
        // create server socket by trying increasing port numbers
        do
        {
          printf("Attempting server port on %d of %d\n", port_, UINT16_MAX);
          sc = std::make_unique<SocketCommunicator>(SocketCommunicator(port_, type_));
          port_++;
          if (sc->success()) return sc; // TODO: replace this with exception catching
        }while(port_ < UINT16_MAX);
        printf("Could not find open port for server socket\n");
        exit(-1);
      }
      // if factory is client side
      else
      {
        // create client socket by trying increasing port numbers
        do
        {
            sc = std::make_unique<SocketCommunicator>(SocketCommunicator(server_address_, port_, type_));
            port_++;
            if (sc->success()) return sc; // TODO: replace this with exception catching
        }while(port_ < UINT16_MAX);
        printf("Could not connect to server socket on any ports\n");
        exit(-1);
      }
    }
    // data_type& get_as_nonconst_vector() { return communicators_; }

  protected:
    int type_;
    const char * server_address_;
    uint16_t port_;

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

    // std::vector<int> ip_address_;
    // std::vector<int> port_ref_;
    // std::vector<std::unique_ptr<DeviceCommunicator>> communicators_ = data_type();
  // std::string d1_ip_address_;
  // std::string d2_ip_address_
  // int sockfd_;
  // int connfd_ = -1 ;
  }; // SocketCommunicatorFactory
} // namespace skynet

#endif /* SKYNET_SOCKETCOMMUNICATORFACTORY_HPP__ */

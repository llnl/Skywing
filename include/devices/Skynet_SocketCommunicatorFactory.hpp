#ifndef SKYNET_SOCKETCOMMUNICATORFACTORY_HPP__
#define SKYNET_SOCKETCOMMUNICATORFACTORY_HPP__

// #include "mpi.h"
// #include <memory>
#include <vector>
#include "Skynet_CommunicatorFactory.hpp"
#include "Skynet_SocketCommunicator.hpp"
#include <thread>


namespace skynet
{
  class SocketCommunicatorFactory : public CommunicatorFactory
  {
  public:
    using data_type = std::vector<std::unique_ptr<DeviceCommunicator>>;
    static const int IPv4 = 1;
  public:
      SocketCommunicatorFactory() = default;
 /** \brief Create a new SocketCommunicatorFactory.
     *
     * The ip_address of the device we wish to communicated is needed.
     * For now the ip_addresses are a list of its that can be sorted easily.
     * This will need to change once we are not working on one machine.
     */
    SocketCommunicatorFactory(int type, const char * server_ip, uint16_t port_start):
        server_ip_{server_ip}, port_{port_start}
    {
      // check that type is supported
      switch (type)
      {
        case (IPv4): type_ = AF_INET; break;
        default: printf("incorrect socket type\n"); exit(-1); //TODO: error handling
      }
    }

    std::unique_ptr<DeviceCommunicator>
    create_new_communicator(std::vector<std::string> /* comm_config_info*/)
    {
          port_ +=1;
            // int port = 4000;
            const char * ip_address1 = "192.0.0.1";

            // AF: this is not universal for ip_address since we are still on one machine. This needs to be

            if(local_ip_<remote_ip_){

                // communicators_[k] = std::make_unique<SocketCommunicator>(SocketCommunicator(port_ref_[k])
                return std::make_unique<SocketCommunicator>(SocketCommunicator(port_, type_));
                // std::cout<<"Finsihed setting up Communication Server Side"<<std::endl;
              }
              else{

                // communicators_[k] = std::make_unique<SocketCommunicator>(SocketCommunicator(ip_address1,p
                return std::make_unique<SocketCommunicator>(SocketCommunicator(ip_address1, port_, type_));

              }
            // return std::make_unique<SocketCommunicator>(SocketCommunicator(ip_address1,port));
     }



    //AF: ToDo: discuss with group about return value
    std::unique_ptr<DeviceCommunicator>
    // SocketCommunicator
    create_new_server_communicator(std::vector<std::string> /* comm_config_info*/)
    {
      return std::make_unique<SocketCommunicator>(SocketCommunicator(port_, type_));
    }

    std::unique_ptr<DeviceCommunicator>
    create_new_client_communicator(std::vector<std::string> /* comm_config_info*/)
    {
      return std::make_unique<SocketCommunicator>(SocketCommunicator(server_ip_, port_, type_));
    }

    // data_type& get_as_nonconst_vector() { return communicators_; }


  private:
    const char * server_ip_;
    const char * local_ip_;
    const char * remote_ip_;
    uint16_t port_;
    int type_;

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

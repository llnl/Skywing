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
  public:
      SocketCommunicatorFactory() = default;
 /** \brief Create a new SocketCommunicatorFactory.
     *
     * The ip_address of the device we wish to communicated is needed.
     * For now the ip_addresses are a list of its that can be sorted easily.
     * This will need to change once we are not working on one machine.
     */
    SocketCommunicatorFactory(int local_ip, int remote_ip,int port_start): local_ip_{std::move(local_ip)},remote_ip_{std::move(remote_ip)},port_{std::move(port_start)}
    {

    }

    //AF: ToDo: discuss with group about return value
    // std::unique_ptr<SocketCommunicator>
    SocketCommunicator
    create_new_communicator(std::vector<std::string> /* comm_config_info*/)
    {
      port_ +=1;
        // int port = 4000;
        const char * ip_address1 = "192.0.0.1";

        // AF: this is not universal for ip_address since we are still on one machine. This needs to be updated

        if(local_ip_<remote_ip_){

            // communicators_[k] = std::make_unique<SocketCommunicator>(SocketCommunicator(port_ref_[k]));
            return SocketCommunicator(port_);
            // std::cout<<"Finsihed setting up Communication Server Side"<<std::endl;
          }
          else{

            // communicators_[k] = std::make_unique<SocketCommunicator>(SocketCommunicator(ip_address1,port_ref_[k]));
            return SocketCommunicator(ip_address1,port_);

          }
        // return std::make_unique<SocketCommunicator>(SocketCommunicator(ip_address1,port));
    }

    // data_type& get_as_nonconst_vector() { return communicators_; }


  private:
    int local_ip_;
    int remote_ip_;
    int port_;

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

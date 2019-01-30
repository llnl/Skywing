#ifndef SKYNET_SOCKETGATEWAY_HPP__
#define SKYNET_SOCKETGATEWAY_HPP__

#include <arpa/inet.h>
#include "Skynet_Gateway.hpp"
#include "Skynet_SocketCommunicatorFactory.hpp"
#include "Skynet_SocketListener.hpp"

namespace skynet
{
  /** \class SocketGateway
   * \brief Implementation of Gateway for SocketCommunicatorFactory
   *
   */
  class SocketGateway : public Gateway
  {
  public:

    /** \brief Construct a new SocketGateway.
     *
     * \param type Specifies the address type to be used (IPv4).
     * \param skynet_port The specific port that all SocketGateways will be
     * listening on
     */
    SocketGateway(Configuration& config) :
      config_(config)
    {
      skynet_port_ = stoi(config.get_value("skynet_port"));
      // TODO: maybe improve this
      std::string type = config.get_value("address_type");
      if (type.compare("IPv4") == 0)
        type_ = Socket::IPv4;
      else
      {
        std::cout << "Invalid address type in configuration value" << std::endl;
        exit(-1);
      }
      listener_ = std::make_unique<SocketListener>(type_, skynet_port_, false);
    }

    /** \brief Create ScoketCommunicatorFactory for each device in
     * configuration file.
     *
     * \return a vector of CommunicatorFactory objects that are associated with
     * Devices listed in configuration file.
     */
    std::vector<std::unique_ptr<CommunicatorFactory>> create_initial_connections()
    {
      std::string key;
      const char * ip_address;
      uint16_t port;
      std::vector<std::unique_ptr<CommunicatorFactory>> factories;
      int num_of_devices = stoi(config_.get_value("number_of_devices"));
      for (int i = 0; i < num_of_devices; i++)
      {
        key = "device" + std::to_string(i+1) + "_ip_address";
        ip_address = config_.get_value(key).c_str();
        key = "device" + std::to_string(i+1) + "_port";
        if (config_.has_key(key))
          port = stoi(config_.get_value(key));
        else
          port = skynet_port_;
        factories.push_back(std::make_unique<SocketCommunicatorFactory>(
          type_, ip_address, port));
      }
      return factories;
    }



  private:

    /** \brief Determine if there is a connection request
     *
     * \return whether there is a connection request
     */
    bool client_requesting_connection()
    { return listener_->count_pending_clients() > 0; }

    /** \brief Create new SocketCommunicatorFactory using a provided
     *  SocketDeviceCommunicator
     *
     * \return a unique_ptr to a new SocketCommunicatorFactory
     */
    std::unique_ptr<CommunicatorFactory> create_new_factory()
    {
      // create a new SocketListener to be used by SocketCommunicatorFactory
      SocketListener new_listener(type_, skynet_port_+1, true);
      // create a new SocketCommunicator connected to client and send
      // SocketListener port number back to the client
      std::unique_ptr<SocketCommunicator> handshake =
        listener_->connect_communicator_to_client();
      handshake->send_to<uint16_t>(new_listener.get_port());
      // create SocketCommunicatorFactory
      return std::make_unique<SocketCommunicatorFactory>(type_, std::move(new_listener));
    }

    int type_;
    uint16_t skynet_port_;
    std::unique_ptr<SocketListener> listener_;
    Configuration& config_;

  }; // class SocketGateway
} // namespace skynet

#endif /* SKYNET_SOCKETGATEWAY_HPP__ */

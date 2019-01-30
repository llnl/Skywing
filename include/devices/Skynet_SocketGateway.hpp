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
     * \param config The Configuration object for the Skynet instance
     */
    SocketGateway(Configuration& config) : config_(config)
    {
      // first verify configuration
      verify_configuration();
      // extract the skynet_port and address type from the configuration
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
      // create the SocketListener for this SocketGateway
      listener_ = std::make_unique<SocketListener>(type_, skynet_port_, false);
    }

    /** \brief Create ScoketCommunicatorFactory for each device listed in the
     * configuration
     *
     * \return a vector of CommunicatorFactory objects that are associated with
     * Devices listed in the configuration.
     */
    std::vector<std::unique_ptr<CommunicatorFactory>> create_initial_connections()
    {
      std::string key;
      const char * ip_address;
      uint16_t port;
      std::vector<std::unique_ptr<CommunicatorFactory>> factories;
      // obtain number of devices in configuration and iterate through them
      int num_of_devices = stoi(config_.get_value("number_of_devices"));
      for (int i = 0; i < num_of_devices; i++)
      {
        // obtain ip address
        key = "device" + std::to_string(i+1) + "_ip_address";
        ip_address = config_.get_value(key).c_str();
        // obtain port, if there is no port listed, use skynet_port
        key = "device" + std::to_string(i+1) + "_port";
        if (config_.has_key(key))
          port = stoi(config_.get_value(key));
        else
          port = skynet_port_;
        // create corresponding SocketCommunicatorFactory
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

    /** \brief Verify that all required keys are in the configuration
     *
     * \param key Specifies name of the key
     * \return Whether the key exists in the dictionary
     */
    void verify_configuration()
    {
      std::vector<std::string> keys;
      keys.push_back("skynet_port");
      keys.push_back("address_type");
      keys.push_back("number_of_devices");
      config_.verify_keys(keys);

      keys.clear();
      int num_of_devices = stoi(config_.get_value("number_of_devices"));
      for (int i = 0; i < num_of_devices; i++)
        keys.push_back("device" + std::to_string(i+1) + "_ip_address");
      config_.verify_keys(keys);
    }

    int type_;
    uint16_t skynet_port_;
    std::unique_ptr<SocketListener> listener_;
    Configuration& config_;

  }; // class SocketGateway
} // namespace skynet

#endif /* SKYNET_SOCKETGATEWAY_HPP__ */

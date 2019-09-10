#ifndef SKYNET_SOCKETGATEWAY_HPP__
#define SKYNET_SOCKETGATEWAY_HPP__

#include <arpa/inet.h>
#include "Skynet_Gateway.hpp"
#include "Skynet_SocketCommunicatorFactory.hpp"
#include "Skynet_SocketListener.hpp"
#include "data/Skynet_KeyValueReader.hpp"

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
    SocketGateway(const KeyValueReader& config)
      : config_(verify_configuration(config)),
        type_(get_type(config)),
        skynet_port_(std::stoi(config.get_value("skynet_port"))),
        listener_(type_, skynet_port_, false)
    {}

    /** \brief Create ScoketCommunicatorFactory for each device listed in the
     * configuration
     *
     * \return a vector of CommunicatorFactory objects that are associated with
     * Devices listed in the configuration.
     */
    std::vector<std::unique_ptr<CommunicatorFactory>> create_initial_connections() const override
    {
      std::vector<std::unique_ptr<CommunicatorFactory>> factories;
      // obtain number of devices in configuration and iterate through them
      const int num_of_devices = std::stoi(config_.get_value("number_of_devices"));
      for (int i = 0; i < num_of_devices; ++i)
      {
        // obtain ip address
        const std::string ip_key = "device" + std::to_string(i + 1) + "_ip_address";
        const std::string& ip_address = config_.get_value(ip_key);
        // obtain port, if there is no port listed, use skynet_port
        const std::string port_key = "device" + std::to_string(i + 1) + "_port";
        const uint16_t port = config_.key_exists(port_key)
          ? std::stoi(config_.get_value(port_key))
          : skynet_port_;

        // create corresponding SocketCommunicatorFactory
        factories.push_back(
          std::make_unique<SocketCommunicatorFactory>(
            type_,
            ip_address,
            port,
            skynet_port_
          )
        );
      }
      return factories;
    }

  private:

    /** \brief Determine if there is a connection request
     *
     * \return whether there is a connection request
     */
    bool client_requesting_connection() const override
    { return listener_.count_pending_clients() > 0; }

    /** \brief Create new SocketCommunicatorFactory
     *
     * \return a unique_ptr to a new SocketCommunicatorFactory
     */
    std::unique_ptr<CommunicatorFactory> create_new_factory() const override
    {
      // create SocketCommunicatorFactory
      auto new_commfactory = std::make_unique<SocketCommunicatorFactory>(type_, 1, skynet_port_);
      // create a new SocketCommunicator connected to client and send
      // SocketListener port number back to the client (buffer communication)
      auto handshake = listener_.connect_communicator_to_client();

      // Sendthe port_number of socket listener in this decives factory to
      // the gatway listener of the other devices
      handshake.send<uint16_t>(new_commfactory->get_port_listener_remote_factory());
      // Get the port number for the socket listener in other devices factory
      // and update comummication factory information
      new_commfactory->update_port_listener_remote_factory(handshake.blocking_receive<uint16_t>());

      return new_commfactory;
    }

    /** \brief Verify that all required keys are in the configuration
     *
     * \param to_check The configuration to check
     * \return The parameter to_check
     */
    static const KeyValueReader& verify_configuration(const KeyValueReader& to_check)
    {
      // Ensure that number_of_devices exists before accessing it
      to_check.verify_keys({"number_of_devices"});

      // Otherwise check everything else
      std::vector<std::string> keys{
        "skynet_port",
        "address_type"
      };
      const int num_of_devices = std::stoi(to_check.get_value("number_of_devices"));
      for (int i = 0; i < num_of_devices; ++i)
        keys.push_back("device" + std::to_string(i + 1) + "_ip_address");
      to_check.verify_keys(keys);

      return to_check;
    }

    /** \brief Returns the type of connection from the specified configuration
     *
     * \param config The configuration to read from
     */
    static int get_type(const KeyValueReader& config)
    {
      if (config.get_value("address_type") == "IPv4")
        return Socket::ipv4;
      else
      {
        std::cout << "Invalid address type in configuration value\n";
        exit(-1);
      }
    }

    const KeyValueReader& config_;
    int type_;
    uint16_t skynet_port_;
    SocketListener listener_;
  }; // class SocketGateway
} // namespace skynet

#endif /* SKYNET_SOCKETGATEWAY_HPP__ */

#include "catch2/catch.hpp"
#include "devices/Skynet_Socket.hpp"
#include "devices/Skynet_SocketGateway.hpp"
#include "heartbeat/Skynet_Heart.hpp"
#include "heartbeat/Skynet_TrivialBeatInterpreter.hpp"
#include "heartbeat/Skynet_TrivialBeatSender.hpp"
#include "heartbeat/Skynet_TrivialDeviceManager.hpp"
#include "heartbeat/Skynet_TrivialPropertyChecker.hpp"
#include "heartbeat/Skynet_TrivialPulseTimer.hpp"

TEST_CASE( "Single instantiation and connection", "[Skynet_Heart]" )
{
  const uint16_t SKYNET_PORT = 5000;
  int IPv4 = skynet::Socket::IPv4;
  // This device starts first and has nobody to connect to
  std::ofstream outfile("test_config.txt");
  outfile << "skynet_port\t" << SKYNET_PORT << std::endl;
  outfile << "number_of_devices\t0" << std::endl;
  outfile << "address_type\tIPv4" << std::endl;
  outfile.close();
  skynet::Configuration skynet_config("test_config.txt", "\t");

  std::cout << "Starting heartbeat on T1" << std::endl;
  skynet::Heart heart_T1(std::make_unique<skynet::TrivialBeatSender>(),
                          std::make_unique<skynet::TrivialBeatInterpreter>(),
                          std::make_unique<skynet::TrivialPulseTimer>(),
                          std::make_unique<skynet::TrivialDeviceManager>(
                            std::make_unique<skynet::SocketGateway>(skynet_config)
                           ),
                           std::make_unique<skynet::TrivialPropertyChecker>());
  heart_T1.run_heartbeat();

  std::cout << "Creating Communicator Factory to T1" << std::endl;
  skynet::SocketCommunicatorFactory comm_factory(IPv4, "127.0.0.1", SKYNET_PORT);

  std::cout << "Creating Device Communicator to T1" << std::endl;
  std::unique_ptr<skynet::DeviceCommunicator> communicator =
    comm_factory.create_new_communicator(std::vector<std::string>(0));
  std::cout << "Terminating T1" << std::endl;
  heart_T1.terminate_device();
}

#include "catch2/catch.hpp"
#include "devices/Skynet_Socket.hpp"
#include "devices/Skynet_SocketGateway.hpp"
#include "heartbeat/Skynet_Heart.hpp"
#include "heartbeat/Skynet_TrivialBeatInterpreter.hpp"
#include "heartbeat/Skynet_TrivialBeatSender.hpp"
#include "heartbeat/Skynet_TrivialDeviceManager.hpp"
#include "heartbeat/Skynet_TrivialPropertyChecker.hpp"
#include "heartbeat/Skynet_TrivialPulseTimer.hpp"

TEST_CASE( "Heart Instantiation", "[Skynet_Heart]" )
{
  const uint16_t SKYNET_PORT = 5000;
  int IPv4 = skynet::Socket::IPv4;

  std::cout << "Starting heartbeat on T1" << std::endl;
  skynet::Heart heart_T1(std::make_unique<skynet::TrivialBeatSender>(),
                          std::make_unique<skynet::TrivialBeatInterpreter>(),
                          std::make_unique<skynet::TrivialPulseTimer>(),
                          std::make_unique<skynet::TrivialDeviceManager>(
                            std::make_unique<skynet::SocketGateway>(IPv4, SKYNET_PORT)
                           ),
                           std::make_unique<skynet::TrivialPropertyChecker>());
  heart_T1.run_heartbeat();

  std::cout << "Creating Communicator Factory to T1" << std::endl;
  skynet::SocketCommunicatorFactory comm_factory(IPv4, "127.0.0.1", SKYNET_PORT);

  std::cout << "Creating Device Communicator to T1" << std::endl;
  std::unique_ptr<skynet::DeviceCommunicator> communicator =
    comm_factory.create_new_communicator(std::vector<std::string>(0));
  std::cout << "Creating Device Communicator on T1" << std::endl;
  heart_T1.create_device_communicator();
  std::cout << "Sending the number 10 to T1" << std::endl;
  communicator->send_to<int>(10);
  int received = heart_T1.receive_message();
  std::cout << "T1 received " << received << std::endl;
  REQUIRE(received == 10);
  std::cout << "Terminating T1" << std::endl;
  heart_T1.terminate_device();
}

#include "catch2/catch.hpp"
#include "devices/Skynet_SocketCommunicator.hpp"

#include <iostream>
#include <thread>

struct thread_client_msg {
  int number_msg;
  int test_int;
  double test_double;
  unsigned test_unsigned;
  bool test_bool;
  std::vector<int> intvec;
  std::vector<double> doublevec;
  std::vector<unsigned> unsignedvec;
};

void my_server(thread_client_msg& write_to)
{
  SocketCommunicator socketCom(5086);
  std::this_thread::sleep_for(std::chrono::seconds(10));

  write_to.test_int  = socketCom.receive<int>();
  write_to.test_double = socketCom.receive<double>();
  write_to.test_unsigned = socketCom.receive<unsigned>();
  write_to.test_bool = socketCom.receive<bool>();
  write_to.intvec = socketCom.receive<std::vector<int>>();
  write_to.doublevec = socketCom.receive<std::vector<double>>();
  write_to.unsignedvec = socketCom.receive<std::vector<unsigned>>();
}

void my_client(const thread_client_msg& send_message)
{
  std::this_thread::sleep_for(std::chrono::seconds(3));

  SocketCommunicator socketCom("127.0.0.1", 5086);

  std::this_thread::sleep_for(std::chrono::seconds(11));

  socketCom.blocking_send<int>(send_message.test_int);
  socketCom.blocking_send<double>(send_message.test_double);
  socketCom.blocking_send<unsigned>(send_message.test_unsigned);
  socketCom.blocking_send<bool>(send_message.test_bool);
  socketCom.blocking_send<std::vector<int>>(send_message.intvec);
  socketCom.blocking_send<std::vector<double>>(send_message.doublevec);
  socketCom.blocking_send<std::vector<unsigned>>(send_message.unsignedvec);
}

TEST_CASE("Communication methods work", "[Skynet_SocketCommunicator]")
{
  std::cout << "Creating threads for testing....\n";

  thread_client_msg client_msg;
  thread_client_msg server_msg;

  client_msg.number_msg = 6;
  client_msg.test_int = -9;
  client_msg.test_double = -11.11;
  client_msg.test_unsigned = 2;
  client_msg.test_bool = false;
  client_msg.intvec = std::vector<int>{11, -5, 6};
  client_msg.doublevec = std::vector<double>{9.5, -5.203, 8.4};
  client_msg.unsignedvec = std::vector<unsigned>{7, 5, 9};

  std::thread server_thread(my_server, server_msg);
  std::this_thread::sleep_for(std::chrono::seconds(3));
  std::thread client_thread(my_client, client_msg);

  server_thread.join();
  client_thread.join();

  std::cout << "Testing output....\n";

  REQUIRE(tcm.test_int == tsm.test_int);
  REQUIRE(tcm.test_double == tsm.test_double);
  REQUIRE(tcm.test_unsigned == tsm.test_unsigned);
  REQUIRE(tcm.test_bool == tsm.test_bool);
  REQUIRE(tcm.intvec == tsm.intvec);
  REQUIRE(tcm.doublevec == tsm.doublevec);
  REQUIRE(tcm.unsignedvec == tsm.unsignedvec);
}

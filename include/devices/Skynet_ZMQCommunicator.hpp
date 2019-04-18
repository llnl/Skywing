#ifndef SKYNET_ZMQCOMMUNICATOR_HPP__
#define SKYNET_ZMQCOMMUNICATOR_HPP__

#include <zmq.hpp>
#include "Skynet_DeviceCommunicator.hpp"

namespace skynet
{
  class ZMQCommunicator : public DeviceCommunicator
  {
  public:
    ZMQCommunicator(char const* address) :
      zmq_ctx_(1),
      zmq_sock_(zmq_ctx_, ZMQ_PAIR)
    {
      zmq_sock_.bind(address);
    }

    void connect_to_server(char const* address)
    {
      zmq_sock_.connect(address);
    }

  private:
    void do_send_to_(void const* data, std::size_t data_size) override
    {
      auto data_buf = std::make_unique<char[]>(data_size);
      std::memcpy(data_buf.get(), data, data_size);
      zmq::message_t msg(data_buf.release(), data_size, [](void* p, void*) { std::default_delete<char[]>()(static_cast<char*>(p)); });
      zmq_sock_.send(msg);
    }

    std::vector<char> do_receive_from_() override
    {
      zmq::message_t msg;
      zmq_sock_.recv(&msg);
      std::vector<char> vec(msg.size());
      std::memcpy(vec.data(), msg.data(), msg.size());
      return vec;
    }

  private:
    zmq::context_t zmq_ctx_;
    zmq::socket_t zmq_sock_;

  }; // class ZMQCommunicator
} // namespace skynet


#endif /* SKYNET_MPICOMMUNICATOR_HPP__ */

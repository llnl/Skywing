#ifndef SKYNET_DEVICECOMMUNICATOR_HPP__
#define SKYNET_DEVICECOMMUNICATOR_HPP__

#include <cstddef>
#include <thread>
#include <chrono>
#include <utility>
#include "Skynet_Serialize.hpp"

namespace skynet
{
  /** \class DeviceCommunicator
   *  \brief Abstract class, policy for communicating with a Device.
   *
   * This is an abstract class that provides a point-to-point
   * communications interface between devices.
   */
  class DeviceCommunicator
  {
  public:

    /** \brief Send a data through this communication channel.
     *
     * \return True if the send succeeded, false otherwise
     */
    template<typename T>
    bool send(const T& data)
    {
      const auto buf = serialize(data);

      return do_send(buf.data(), buf.size());
    }

    /** \brief Receive data through this communication channel.
     *
     * \return A bool indicating if a value is present and a value of type T if so
     */
    template<typename T>
    std::pair<bool, T> receive()
    {
      const auto buf = do_receive();

      if (buf.empty())
      {
        return std::pair<bool, T>(false, {});
      }
      return std::make_pair(true, deserialize<T>(buf));
    }

    /** \brief Blocks until data is recieved from the connection
     *
     * \return A value of type T
     */
    template<typename T>
    T blocking_receive()
    {
      using namespace std::chrono_literals;
      while (true)
      {
        const auto val = receive<T>();
        if (val.first)
        {
          return val.second;
        }
        std::this_thread::sleep_for(10us);
      }
    }

    virtual ~DeviceCommunicator() = default;

  private:

    /** \brief Send data to the associated Device.
     *
     * \param data Data to send.
     * \param data_size Number of bytes of data to send.
     * \return True if the send succeeded, false otherwise
     */
    virtual bool do_send(const void* data, std::size_t data_size) = 0;

    /** \brief Receive data from the associated Device.
     *
     * \return A vector holding the data, or an empty vector if the recieve
     *         would block
     */
    virtual std::vector<char> do_receive() = 0;
  }; // class DeviceCommunicator

} // namespace skynet


#endif /*  SKYNET_DEVICECOMMUNICATOR_HPP__ */

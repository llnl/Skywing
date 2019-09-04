#ifndef SKYNET_DEVICECOMMUNICATOR_HPP__
#define SKYNET_DEVICECOMMUNICATOR_HPP__

#include <cstddef>
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
     */
    template<typename T>
    void send_to(const T& data)
    {
      auto buf = serialize(data);

      do_send_to_(buf.data(), buf.size());
    }

    /** \brief Receive data through this communication channel.
     *
     * \return An object of type T.
     */
    template<typename T>
    T receive_from()
    {
      auto buf = do_receive_from_();

      return deserialize<T>(buf);
    }

    virtual ~DeviceCommunicator() = default;

  private:

    /** \brief Send data to the associated Device.
     *
     * \param data Data to send.
     * \param data_size Number of bytes of data to send.
     * \param tag A tag associated with the data.
     */
    virtual void do_send_to_(const void* data, std::size_t data_size) = 0;

    /** \brief Receive data from the associated Device.
     *
     * \param tag A tag associated with the expected data.
     *
     * \return A pair providing the data received and the size of
     * the data received.
     */
    virtual std::vector<char> do_receive_from_() = 0;
  }; // class DeviceCommunicator

} // namespace skynet


#endif /*  SKYNET_DEVICECOMMUNICATOR_HPP__ */

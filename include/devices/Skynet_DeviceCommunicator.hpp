#ifndef SKYNET_DEVICECOMMUNICATOR_HPP__
#define SKYNET_DEVICECOMMUNICATOR_HPP__

#include <cstddef>
#include "Skynet_Serializer.hpp"

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
    void send_to(const T& data) const
    {
      auto pData = serialize(data); // returns either a void* or a std::vector<char>
      do_send_to_(convert_if_vec(pData), get_serialized_size(data, pData));
    }

    /** \brief Receive data through this communication channel.
     *
     * \return An object of type T.
     */
    template<typename T>
    T receive_from() const
    {
      return deserialize<T>(do_receive_from_());
    }

  private:
      
    /** \brief Send data to the associated Device. 
     *
     * \param data Data to send.
     * \param data_size Number of bytes of data to send.
     * \param tag A tag associated with the data.
     */
    virtual void do_send_to_(const void* data, std::size_t data_size) const = 0;

    /** \brief Receive data from the associated Device.
     *
     * \param tag A tag associated with the expected data.
     *
     * \return A pair providing the data received and the size of
     * the data received.
     */
    virtual std::vector<char> do_receive_from_() const = 0;
  }; // class DeviceCommunicator

} // namespace skynet


#endif /*  SKYNET_DEVICECOMMUNICATOR_HPP__ */

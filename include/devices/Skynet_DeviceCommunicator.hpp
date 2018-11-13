#ifndef SKYNET_DEVICECOMMUNICATOR_HPP__
#define SKYNET_DEVICECOMMUNICATOR_HPP__

#include <cstddef>
#include "Skynet_Serializer.hpp"
#include "Skynet_Serializable.hpp"

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
     * This function is only active is the data being sent (of type T)
     * inherits from Serializable.
     *
     * \param data The data to send.
     */
    template<typename T>
    std::enable_if_t<std::is_base_of<Serializable, T>::value, void> 
    send_to(T data) const
    {
      void* pv_d = data.serialize();
      std::size_t pv_d_size = data.get_serialized_size();
      do_send_to_(pv_d, pv_d_size);
      data.clean_after_serialization();
    }

    /** \brief Send a data through this communication channel.
     *
     * This function is only active is the data being sent (of type T)
     * does NOT inherit from Serializable.
     */
    template<typename T>
    std::enable_if_t<not std::is_base_of<Serializable, T>::value, void> 
    send_to(T data) const
    {
      do_send_to_(serialize<T>(data), get_serialized_size<T>(data));
    }

    /** \brief Receive data through this communication channel.
     *
     * This function is only active if T inherits from Serializable.
     *
     * \return An object of type T.
     */
    template<typename T>
    std::enable_if_t<std::is_base_of<Serializable, T>::value, T>
    receive_from() const
    {
      std::pair<void*, std::size_t> ret = do_receive_from_();
      T t = T::deserialize(ret);
      delete ret.first;
      return t;
    }

    /** \brief Receive data through this communication channel.
     *
     * This function is only active if T does NOT inherit from Serializable.
     *
     * \return An object of type T.
     */
    template<typename T>
    std::enable_if_t<not std::is_base_of<Serializable, T>::value, T> 
    receive_from() const
    {
      return deserialize<T>(do_receive_from()_);
    }




  private:
      
    /** \brief Send data to the associated Device. 
     *
     * \param data Data to send.
     * \param data_size Number of bytes of data to send.
     * \param tag A tag associated with the data.
     */
    virtual void do_send_to_(void* data, std::size_t data_size) const = 0;

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

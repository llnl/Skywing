#ifndef SKYNET_DEVICE_HPP__
#define SKYNET_DEVICE_HPP__

#include <memory>
#include <cstddef>
#include <type_traits>
#include "Skynet_DeviceCommunicator.hpp"
#include "Skynet_Serializer.hpp"
#include "Skynet_Serializable.hpp"

namepsace skynet
{  
  /** \class Device

      A Device object represents a participating device in the Skynet
      instance. This object contains information about the device
      such as how to communicate with it and its computational
      capabilities.
  */
  class Device
  {
  public:
    /** \brief Construct a new \c Device.
     *
     * \param comm A DeviceCommunicator representing this Device's
     * communications policy.
     */
    Device(std::unique_ptr<DeviceCommunicator> comm)
      : comm(comm_), is_believed_live_(true)
    { }
    
    /** \brief Get the DeviceCommunicator policy object. */
    const DeviceCommunicator& get_comm() const
    { return *comm_; }

    /** \brief Get if we believe this Device to be live. */
    bool get_is_belived_live() const
    { return is_live_; }



    template<typename T>
    std::enable_if_t<std::is_base_of<Serializable, T>::value, void> 
    send_to(T data, int tag) const
    {
      void* pv_d = data.serialize();
      std::size_t pv_d_size = data.get_serialized_size();
      comm_->send_to(pv_d, pv_d_size, tag);
      data.clean_after_serialization();
    }

    template<typename T>
    std::enable_if_t<not std::is_base_of<Serializable, T>::value, void> 
    send_to(T data, int tag) const
    {
      comm_->send_to(serialize<T>(data), get_serialized_size<T>(data), tag);
    }



    template<typename T>
    std::enable_if_t<std::is_base_of<Serializable, T>::value, T>
    receive_from(int tag) const
    {
      std::pair<void*, std::size_t> ret = comm_->receive_from(tag);
      T t = T::deserialize(ret);
      delete ret.first;
      return t;
    }

    template<typename T>
    std::enable_if_t<not std::is_base_of<Serializable, T>::value, T> 
    receive_from(int tag) const
    {
      return deserialize<T>(comm_->receive_from(tag));
    }

    
  private:
    bool is_believed_live;
    std::unique_ptr<DeviceCommunicator> comm_;
    
  }; // class Device
} // namespace skynet


#endif /* SKYNET_DEVICE_HPP__ */

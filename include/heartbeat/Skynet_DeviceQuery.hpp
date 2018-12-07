#ifndef SKYNET_DEVICEQUERY_HPP__
#define SKYNET_DEVICEQUERY_HPP__

#include "devices/Skynet_DeviceReference.hpp"

namespace skynet
{
  /** \class DeviceQuery
   *  \brief Abstract class, interface for definining policies
   *	     to check liveness of nearby devices
   *
   * This class will be used by the DeviceManager to determine
   * if nearby devices are online.
   * QUESTION: Should it be used by the DeviceManager or by the
   *	HeartBeat?
   */
   class DeviceQuery
   {
   public:

     /** \brief Check if a neighboring device is live
       *
       * \param device Device to check liveness of
       * \param policy Policy for determining if the device is live
       */
     /** NOTE: DeviceReference already has a function get_is_believed_live
      * that I think checks the liveness of the current device, so we might
      * want to call this something else. I initially called it get_is_neighbor_live
      * but I thought that might make it seem like we wanted to check the
      * input device's neighbors instead of the input device itself.
      */
     template<typename T>
     bool get_is_live(const DeviceReference& device, const T& policy) const
     {
       return do_get_is_live(device, policy);
     }

   private:

     template<typename T>
     virtual bool do_get_is_live(const DeviceReference& device, const T& policy) const = 0;

     
   };// class 


}// namespace skynet

#endif /* SKYNET_DEVICEQUERY_HPP__ */

#ifndef SKYNET_DEVICEREFERENCE_HPP__
#define SKYNET_DEVICEREFERENCE_HPP__

#include <memory>
#include <cstddef>
#include <type_traits>
#include <vector>
#include <string>
#include "Skynet_DeviceCommunicator.hpp"
#include "Skynet_CommunicatorFactory.hpp"

namespace skynet
{
  /** \class DeviceReference

      A DeviceReference object represents some other participating
      device in the Skynet instance. This object contains information
      about the device such as how to communicate with it and its
      computational capabilities.
  */
  class DeviceReference
  {
  public:
    /** \brief Construct a new \c DeviceReference.
     *
     * \param comm A DeviceCommunicator representing this
     * DeviceReference's communications policy.
     */
    // TODO resolve compiler error and uncomment
    // error: use of deleted function ‘std::unique_ptr<_Tp, _Dp>&
    // std::unique_ptr<_Tp, _Dp>::operator=(const std::unique_ptr<_Tp, _Dp>&)
    // [with _Tp = skynet::CommunicatorFactory; _Dp =
    // std::default_delete<skynet::CommunicatorFactory>]’
    // { comm_factory_ = comm_factory}
    //                   ^~~~~~~~~~~~
    // In file included from /home/vogl2/local/gcc-6.5.0_tux/include/c++/6.5.0/memory:81:0,
    //             from /home/vogl2/local/gcc-6.5.0_tux/include/c++/6.5.0/thread:40,
    //             from /home/vogl2/workspace/skynet/include/heartbeat/Skynet_Heart.hpp:6,
    //             from HeartHelper.hpp:7,
    //             from skynet_startup.cpp:22:
    // /home/vogl2/local/gcc-6.5.0_tux/include/c++/6.5.0/bits/unique_ptr.h:362:19: note: declared here
    //   unique_ptr& operator=(const unique_ptr&) = delete;
    //
    DeviceReference(std::unique_ptr<CommunicatorFactory> comm_factory)
      //: is_believed_live_(true), comm_factory_(comm_factory)
    { }

    /** \brief Get the DeviceCommunicator policy object. */
    // TODO: resolve compiler error and uncomment
    // error: invalid initialization of reference of type
    // ‘const skynet::DeviceCommunicator&’ from expression of type
    // ‘skynet::CommunicatorFactory’
    // { return *comm_factory_; }
    //
    //const DeviceCommunicator& get_comm() const
    //{ return *comm_factory_; }

    /** \brief Get if we believe the referred device to be live. */
    bool get_is_belived_live() const
    { return is_believed_live_; }


    /** \brief Request a new DeviceCommunicator for this DeviceReference.
     * \return A new DeviceCommunicator. */
    std::unique_ptr<DeviceCommunicator> create_new_communicator()
    {
      return comm_factory_->create_new_communicator(comm_config_info_);
    }

  private:
    bool is_believed_live_;
    id_t device_id_;
    std::unique_ptr<CommunicatorFactory> comm_factory_;
    std::vector<std::string> comm_config_info_;

  }; // class DeviceReference
} // namespace skynet


#endif /* SKYNET_DEVICEREFERENCE_HPP__ */

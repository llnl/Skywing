#ifndef SKYNET_SKYNETPTR_HPP__
#define SKYNET_SKYNETPTR_HPP__

namespace skynet
{
    /** \class SkynetPtr
     *  \brief A smart pointer representing decentralized data.
     *
     * The data of a SkynetPtr exists in the Skynet instance, but may
     * or may not actually live on any given device.
     */
  template<typename T>
  class SkynetPtr
  {

  private:
      T* data;
  }; // class SkynetPtr
} // namespace skynet

#endif /* SKYNET_SKYNETPTR_HPP__ */

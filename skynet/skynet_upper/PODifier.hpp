#ifndef SKYNET_PODIFIER_HPP
#define SKYNET_PODIFIER_HPP

namespace skynet
{
  /** @brief Convert an object into a Plain Old Data type that Skynet can send through its pubsub system.
   *
   *  This is a default struct template that works when T is itself a
   *  Plain Old Data type that Skynet can send directly.
   *
   * @tparam T The type to convert into Plain Old Data.
   */
  template<typename T>
  struct PODifier
  {
    using POD_type = T;

    static POD_type PODify(const T& t) { return t; }
    static T dePODify(const POD_type& pod) { return pod; }
  }; // struct PODifier
}

#endif // SKYNET_PODIFIER_HPP

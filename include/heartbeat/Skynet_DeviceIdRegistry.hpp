#ifndef SKYNET_DEVICEIDREGISTRY_HPP__
#define SKYNET_DEVICEIDREGISTRY_HPP__

#include <cstdint>
#include <iostream>
#include <algorithm>

namespace skynet
{
  /** \class DeviceIdRegistry
   *  \brief Class for managing device IDs
   *
   * This templated class implements a registry of device IDs where the
   * template type specifies the ID value type.
   */
  template<typename id_t>
  class DeviceIdRegistry
  {

  public:

    DeviceIdRegistry()
    { set_bounds(); }

    /** \brief Obtain the next free ID in the registry
     *  \return The value of the next free ID
     */
    id_t next_id()
    {
      const id_t next_free_id_ = next_free_id();
      registered_ids_.push_back(next_free_id_);
      return next_free_id_;
    }

    /** \brief Free a registered ID so that it can be reused
     *  \param id_to_free The value of the ID to be freed
     */
    void free_id(const id_t id_to_free)
    {
      // TODO: Look at this more; can be rewritten
      for (typename std::vector<id_t>::iterator iter = registered_ids_.begin();
        iter != registered_ids_.end(); iter++)
      {
        if (*iter == id_to_free)
        {
          freed_ids_.push_back(*iter);
          registered_ids_.erase(iter);
          return;
        }
      }
      // The id was not found if the loop is exited
      std::cout << "Tried to free ID that was not registered in DeviceIdRegistry\n";
      exit(-1);
    }

  private:

    /** \brief Given a device ID value, obtain the next value
     *  \param id Current value of ID
     *  \return The value of the next ID, regardless of whether it is registered
     */
    id_t next_value(id_t id) const;

    /** \brief Set bounds of ID type (first_id_) and (last_id_)
     */
    void set_bounds();

    /** \brief Find the next free ID
     *  \return The value of the next free ID
     */
    id_t next_free_id()
    {
      // first, check if any ids have been registered
      if (registered_ids_.empty())
        return first_id_;
      // second, check if any ids have been freed
      else if (!freed_ids_.empty())
      {
        const auto free_id = freed_ids_.back();
        freed_ids_.pop_back();
        return free_id;
      }
      // third, use next_value to advance id value until a free one is found
      else
      {
        id_t next_id = registered_ids_.back();
        while (true)
        {
          if (next_id == last_id_)
          {
            std::cout << "No free ids in DeviceRegistry\n";
            exit(-1);
          }

          next_id = next_value(next_id);

          if (std::find(registered_ids_.begin(), registered_ids_.end(), next_id) == registered_ids_.end())
          {
            break;
          }
        }
        return next_id;
      }
    }

    id_t first_id_;
    id_t last_id_;
    std::vector<id_t> registered_ids_;
    std::vector<id_t> freed_ids_;

  }; // class DeviceIdRegistry

  // Implementation for uint8_t IDs
  template<> inline void DeviceIdRegistry<uint8_t>::set_bounds()
  { first_id_ = 0; last_id_ = UINT8_MAX; }
  template<> inline uint8_t DeviceIdRegistry<uint8_t>::next_value(const uint8_t id) const
  { return id+1; }

} // namespace skynet
#endif /* SKYNET_DEVICEIDREGISTRY_HPP__ */

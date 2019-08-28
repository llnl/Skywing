#ifndef SKYNET_DEVICEIDREGISTRY_HPP__
#define SKYNET_DEVICEIDREGISTRY_HPP__

#include <cstdint>

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
      bool found = false;
      for (typename std::vector<id_t>::iterator iter = registered_ids_.begin();
        iter != registered_ids_.end(); iter++)
      {
        if (*iter == id_to_free)
        {
          freed_ids_.push_back(*iter);
          registered_ids_.erase(iter);
          found = true;
          break;
        }
      }
      if (!found)
      {
        printf("Tried to free ID that was not registered in DeviceIdRegistry\n");
        exit(-1);
      }
    }

  private:

    /** \brief Given a device ID value, obtain the next value
     *  \param id Current value of ID
     *  \return The value of the next ID, regardless of whether it is registered
     */
    id_t next_value(id_t id);

    /** \brief Set bounds of ID type (first_id_) and (last_id_)
     */
    void set_bounds();

    /** \brief Find the next free ID
     *  \return The value of the next free ID
     */
    const id_t next_free_id()
    {
      id_t next_free_id_;
      // first, check if any ids have been registered
      if (registered_ids_.empty())
        next_free_id_ = first_id_;
      // second, check if any ids have been freed
      else if (!freed_ids_.empty())
      {
        next_free_id_ = freed_ids_.back();
        freed_ids_.pop_back();
      }
      // third, use next_value to advance id value until a free one is found
      else
      {
        next_free_id_ = registered_ids_.back();
        bool found = false;
        while (!found)
        {
          if (next_free_id_ == last_id_)
          {
            printf("No free ids in DeviceRegistry\n");
            exit(-1);
          }
          else
            next_free_id_ = next_value(next_free_id_);
          found = true;
          for (id_t id : registered_ids_)
          {
            if (id == next_free_id_)
            {
              found = false;
              break;
            }
          }
        }
      }
      return next_free_id_;
    }

    id_t first_id_;
    id_t last_id_;
    std::vector<id_t> registered_ids_;
    std::vector<id_t> freed_ids_;

  }; // class DeviceIdRegistry

  // Implementation for uint8_t IDs
  template<> inline void DeviceIdRegistry<uint8_t>::set_bounds()
  { first_id_ = 0; last_id_ = UINT8_MAX; }
  template<> inline uint8_t DeviceIdRegistry<uint8_t>::next_value(const uint8_t id)
  { return id+1; }

} // namespace skynet
#endif /* SKYNET_DEVICEIDREGISTRY_HPP__ */

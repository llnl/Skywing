#ifndef SKYNET_DEVICEIDREGISTRY_HPP__
#define SKYNET_DEVICEIDREGISTRY_HPP__

#include <cstdint>

namespace skynet
{

  template<typename id_t>
  class DeviceIdRegistry
  {

  public:

    DeviceIdRegistry()
    {
      set_bounds();
      next_free_id_ = first_id_;
    }

    const id_t next_id()
    {
      registered_ids_.push_back(next_free_id_);
      const id_t registered_id = next_free_id_;
      set_next_free_id();
      return registered_id;
    }

    void free_id(id_t id)
    {
      // HELLO!
    }

  private:

    id_t next_value(id_t id);

    void set_bounds();

    void set_next_free_id()
    {
      // first check if any ids have been freed
      if (!freed_ids_.empty())
      {
        next_free_id_ = freed_ids_.back();
        freed_ids_.pop_back();
      }
      else
      {
        bool is_free;
        bool found = false;
        int reset_count = 0;
        while (!found && reset_count < 2)
        {
          if (next_free_id_ == last_id_)
          {
            next_free_id_ = first_id_;
            reset_count++;
          }
          else
            next_free_id_ = next_value(next_free_id_);
          is_free = true;
          for (id_t id : registered_ids_)
          {
            if (id == next_free_id_)
            {
              is_free = false;
              break;
            }
          }
          found = is_free;
        }
        if (reset_count == 2)
        {
          printf("No free ids in DeviceRegistry\n");
          exit(-1);
        }
      }
    }

    id_t first_id_;
    id_t last_id_;
    id_t next_free_id_;
    std::vector<id_t> registered_ids_;
    std::vector<id_t> freed_ids_;

  }; // class DeviceIdRegistry

  template<> void DeviceIdRegistry<uint8_t>::set_bounds()
  { first_id_ = 0; last_id_ = UINT8_MAX; }
  template<> uint8_t DeviceIdRegistry<uint8_t>::next_value(uint8_t id)
  { return id+1; }

} // namespace skynet
#endif /* SKYNET_DEVICEIDREGISTRY_HPP__ */

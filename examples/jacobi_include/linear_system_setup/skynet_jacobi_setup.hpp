std::vector<std::string> obtain_machine_names(std::uint16_t size_of_system)
{
  std::vector<std::string > machine_names;
  machine_names.resize(size_of_system);
  for(int i = 0 ; i < size_of_system; i++)
  {
    machine_names[i] = "node" + std::to_string(i+1);
  }
return machine_names;
}

std::vector<std::uint16_t>  set_port(std::uint16_t starting_port_number, std::uint16_t size_of_system)
{
  std::vector<std::uint16_t> ports;

  for(std::uint16_t i = 0; i < size_of_system; i++)
  {
    ports.push_back(starting_port_number + (i * 100));
  }
  return ports;
}

template <class TagType>
std::vector<TagType> obtain_tags(std::uint16_t size_of_system)
{
  std::vector<TagType> tags;
  for(int i = 0; i < size_of_system; i++)
  {
    std::string hold = "tag" +  std::to_string(i);
    tags.push_back(TagType{hold});
  }
  return tags;
}

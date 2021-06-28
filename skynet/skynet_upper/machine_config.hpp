#ifndef SKYNET_MACHINE_CONFIG_HPP
#define SKYNET_MACHINE_CONFIG_HPP

#include <vector>
#include <map>
#include <string>
#include <assert>
#include <fstream>
#include <functional>

#include "skynet_core/skynet.hpp"

static std::pair<std::string, std::string> read_tag(std::istream& in) {
  std::string line;
  std::getline(in, line);
  std::string name, type;
  auto space = line.find(" ");
  if (space == line.end) {
    return std::make_pair(line, "");
  }
  name = line.substr(0, space);
  type = line.substr(space+1, line.size()-space);
  return std::make_pair(name, type);
}

struct TagGroup {
  skynet::internal::ReduceGroupTagBase reduce_group_tag;
  skynet::internal::ReduceValueTagBase reduce_value_tag;
  std::vector<skynet::internal::ReduceValueTagBase> reduce_value_tags;

  friend std::istream& operator>>(std::istream& in, TagGroup& reduce_group) {
    auto groupTag = read_tag(in);
    // more error checking
    reduce_group.reduce_group_tag = reduce_group_map[groupTag.second](groupTag.first);
    std::string line;
    std::getline(in, line);
    if (line != "-") {
      std::cerr << "error in reduce group\n";
      return in;
    }

    auto pubTag = read_tag(in);
    reduce_group.reduce_value_tag = reduce_value_map[pubTag.second](pubTag.first);
    std::getline(in, line);
    if (line != "-") {
      std::cerr << "error in reduce publish value\n";
      return in;
    }

    while (line != "--") {
      auto valTag = read_tag(in);
      reduce_group.reduce_value_tags.push_back(reduce_value_map[valTag.second](valTag.first));
      std::getline(in, line);
    }

    return in;
  }
};

struct MachineID {
  std::string name;
  std::string ip;
  std::uint16_t port;

  friend std::istream& operator>>(std::istream& in, MachineID& machine) {
      in >> machine.name >> machine.ip >> machine.port;
      return in;
  }
};

const std::unordered_map<std::string, std::function<skynet::internal::ReduceGroupTagBase(std::string)>> reduce_group_map {
  { "float", [](std::string name){ return skynet::ReduceGroupTag<float>{name}; } },
  { "double", [](std::string name){ return skynet::ReduceGroupTag<double>{name}; } },
  { "bool", [](std::string name){ return skynet::ReduceGroupTag<bool>{name}; } },
  { "vector<float>", [](std::string name){ return skynet::ReduceGroupTag<std::vector<float>>{name}; } },
  { "vector<double>", [](std::string name){ return skynet::ReduceGroupTag<std::vector<double>>{name}; } },
  { "vector<bool>", [](std::string name){ return skynet::ReduceGroupTag<std::vector<bool>>{name}; } },
  { "int8", [](std::string name){ return skynet::ReduceGroupTag<std::int8_t>{name}; } },
  { "int16", [](std::string name){ return skynet::ReduceGroupTag<std::int16_t>{name}; } },
  { "int32", [](std::string name){ return skynet::ReduceGroupTag<std::int32_t>{name}; } },
  { "int64", [](std::string name){ return skynet::ReduceGroupTag<std::int64_t>{name}; } },
  { "uint8", [](std::string name){ return skynet::ReduceGroupTag<std::uint8_t>{name}; } },
  { "uint16", [](std::string name){ return skynet::ReduceGroupTag<std::uint16_t>{name}; } },
  { "uint32", [](std::string name){ return skynet::ReduceGroupTag<std::uint32_t>{name}; } },
  { "uint64", [](std::string name){ return skynet::ReduceGroupTag<std::uint64_t>{name}; } },
  { "string", [](std::string name){ return skynet::ReduceGroupTag<std::string>{name}; } },
  { "vector<int8>", [](std::string name){ return skynet::ReduceGroupTag<std::vector<std::int8_t>>{name}; } },
  { "vector<int16>", [](std::string name){ return skynet::ReduceGroupTag<std::vector<std::int16_t>>{name}; } },
  { "vector<int32>", [](std::string name){ return skynet::ReduceGroupTag<std::vector<std::int32_t>>{name}; } },
  { "vector<int64>", [](std::string name){ return skynet::ReduceGroupTag<std::vector<std::int64_t>>{name}; } },
  { "vector<uint8>", [](std::string name){ return skynet::ReduceGroupTag<std::vector<std::uint8_t>>{name}; } },
  { "vector<uint16>", [](std::string name){ return skynet::ReduceGroupTag<std::vector<std::uint16_t>>{name}; } },
  { "vector<uint32>", [](std::string name){ return skynet::ReduceGroupTag<std::vector<std::uint32_t>>{name}; } },
  { "vector<uint64>", [](std::string name){ return skynet::ReduceGroupTag<std::vector<std::uint64_t>>{name}; } },
  { "vector<string>", [](std::string name){ return skynet::ReduceGroupTag<std::vector<std::string>>{name}; } },
  { "vector<byte>", [](std::string name){ return skynet::ReduceGroupTag<std::vector<std::byte>>{name}; } }
};

const std::unordered_map<std::string, std::function<skynet::internal::ReduceValueTagBase(std::string)>> reduce_value_map {
  { "float", [](std::string name){ return skynet::ReduceValueTag<float>{name}; } },
  { "double", [](std::string name){ return skynet::ReduceValueTag<double>{name}; } },
  { "bool", [](std::string name){ return skynet::ReduceValueTag<bool>{name}; } },
  { "vector<float>", [](std::string name){ return skynet::ReduceValueTag<std::vector<float>>{name}; } },
  { "vector<double>", [](std::string name){ return skynet::ReduceValueTag<std::vector<double>>{name}; } },
  { "vector<bool>", [](std::string name){ return skynet::ReduceValueTag<std::vector<bool>>{name}; } },
  { "int8", [](std::string name){ return skynet::ReduceValueTag<std::int8_t>{name}; } },
  { "int16", [](std::string name){ return skynet::ReduceValueTag<std::int16_t>{name}; } },
  { "int32", [](std::string name){ return skynet::ReduceValueTag<std::int32_t>{name}; } },
  { "int64", [](std::string name){ return skynet::ReduceValueTag<std::int64_t>{name}; } },
  { "uint8", [](std::string name){ return skynet::ReduceValueTag<std::uint8_t>{name}; } },
  { "uint16", [](std::string name){ return skynet::ReduceValueTag<std::uint16_t>{name}; } },
  { "uint32", [](std::string name){ return skynet::ReduceValueTag<std::uint32_t>{name}; } },
  { "uint64", [](std::string name){ return skynet::ReduceValueTag<std::uint64_t>{name}; } },
  { "string", [](std::string name){ return skynet::ReduceValueTag<std::string>{name}; } },
  { "vector<int8>", [](std::string name){ return skynet::ReduceValueTag<std::vector<std::int8_t>>{name}; } },
  { "vector<int16>", [](std::string name){ return skynet::ReduceValueTag<std::vector<std::int16_t>>{name}; } },
  { "vector<int32>", [](std::string name){ return skynet::ReduceValueTag<std::vector<std::int32_t>>{name}; } },
  { "vector<int64>", [](std::string name){ return skynet::ReduceValueTag<std::vector<std::int64_t>>{name}; } },
  { "vector<uint8>", [](std::string name){ return skynet::ReduceValueTag<std::vector<std::uint8_t>>{name}; } },
  { "vector<uint16>", [](std::string name){ return skynet::ReduceValueTag<std::vector<std::uint16_t>>{name}; } },
  { "vector<uint32>", [](std::string name){ return skynet::ReduceValueTag<std::vector<std::uint32_t>>{name}; } },
  { "vector<uint64>", [](std::string name){ return skynet::ReduceValueTag<std::vector<std::uint64_t>>{name}; } },
  { "vector<string>", [](std::string name){ return skynet::ReduceValueTag<std::vector<std::string>>{name}; } },
  { "vector<byte>", [](std::string name){ return skynet::ReduceValueTag<std::vector<std::byte>>{name}; } }
};

const std::unordered_map<std::string, std::function<skynet::internal::PublishTagBase(std::string)>> publish_tag_map {
  { "float", [](std::string name){ return skynet::PublishTag<float>{name}; } },
  { "double", [](std::string name){ return skynet::PublishTag<double>{name}; } },
  { "bool", [](std::string name){ return skynet::PublishTag<bool>{name}; } },
  { "vector<float>", [](std::string name){ return skynet::PublishTag<std::vector<float>>{name}; } },
  { "vector<double>", [](std::string name){ return skynet::PublishTag<std::vector<double>>{name}; } },
  { "vector<bool>", [](std::string name){ return skynet::PublishTag<std::vector<bool>>{name}; } },
  { "int8", [](std::string name){ return skynet::PublishTag<std::int8_t>{name}; } },
  { "int16", [](std::string name){ return skynet::PublishTag<std::int16_t>{name}; } },
  { "int32", [](std::string name){ return skynet::PublishTag<std::int32_t>{name}; } },
  { "int64", [](std::string name){ return skynet::PublishTag<std::int64_t>{name}; } },
  { "uint8", [](std::string name){ return skynet::PublishTag<std::uint8_t>{name}; } },
  { "uint16", [](std::string name){ return skynet::PublishTag<std::uint16_t>{name}; } },
  { "uint32", [](std::string name){ return skynet::PublishTag<std::uint32_t>{name}; } },
  { "uint64", [](std::string name){ return skynet::PublishTag<std::uint64_t>{name}; } },
  { "string", [](std::string name){ return skynet::PublishTag<std::string>{name}; } },
  { "vector<int8>", [](std::string name){ return skynet::PublishTag<std::vector<std::int8_t>>{name}; } },
  { "vector<int16>", [](std::string name){ return skynet::PublishTag<std::vector<std::int16_t>>{name}; } },
  { "vector<int32>", [](std::string name){ return skynet::PublishTag<std::vector<std::int32_t>>{name}; } },
  { "vector<int64>", [](std::string name){ return skynet::PublishTag<std::vector<std::int64_t>>{name}; } },
  { "vector<uint8>", [](std::string name){ return skynet::PublishTag<std::vector<std::uint8_t>>{name}; } },
  { "vector<uint16>", [](std::string name){ return skynet::PublishTag<std::vector<std::uint16_t>>{name}; } },
  { "vector<uint32>", [](std::string name){ return skynet::PublishTag<std::vector<std::uint32_t>>{name}; } },
  { "vector<uint64>", [](std::string name){ return skynet::PublishTag<std::vector<std::uint64_t>>{name}; } },
  { "vector<string>", [](std::string name){ return skynet::PublishTag<std::vector<std::string>>{name}; } },
  { "vector<byte>", [](std::string name){ return skynet::PublishTag<std::vector<std::byte>>{name}; } }
};


class MachineConfig {
public:
  explicit MachineConfig(std::string& filename) noexcept {
    std::ifstream in(filename);
    std::string line;

    in >> id;
    in >> line;
    std::assert(line == "---");
    line = "";

    while (line != "---") {
      MachineID temp;
      in >> temp;
      machines_to_connect.push_back(std::move(temp));
      in >> line;
    }
    line = "";

    while (line != "---") {
      auto tag = read_tag(in);
      if (tag.second == "") {
        // add more error checking here
        line = tag.first;
        continue;
      }
      add_publish_tag(tag.first, tag.second);
    }

    line = "";
    while (line != "---") {
      auto tag = read_tag(in);
      if (tag.second == "") {
        // add more error checking here
        line = tag.first;
        continue;
      }
      add_subscribe_tag(tag.first, tag.second);
    }

    while (in.eof()) {
      TagGroup temp;
      in >> temp;
      tag_groups.push_back(std::move(temp));
    }

  }

  MachineID& get_machine_id(std::string machine_name);

  
  void add_subscribe_tag(std::string subscribe_tag_name, std::string type) {
    subscribe_tags.push_back(publish_tag_map[type](name));
  }

  void add_publish_tag(std::string publish_tag_name, std::string type) {
    publish_tags.push_back(publish_tag_map[type](name));
  }

  void add_reduce_group_tag(std::string reduce_group_tag_name);

  void add_reduce_value_tag(std::string reduce_group_tag_name, std::string reduce_value_tag_name);

private:
  MachineID id;

  std::vector<MachineID> machines_to_connect;

  std::vector<skynet::internal::PublishTagBase> publish_tags;
  std::vector<skynet::internal::PublishTagBase> subscribe_tags;
  std::vector<TagGroup> tag_groups;

  template<typename T>
  static int read_until_n_dashes(std::istream& in, std::vector<T>& read_into)
  {
    std::string temp;
    int num_dashes = 0;
    while (std::getline(in, temp)) {
      if (temp.empty()) { continue; }
      if (!in || temp.front() == '-') { 
        num_dashes = temp.size();
        break; 
      }
      read_into.emplace_back(temp);
    }
    return num_dashes;
  }

  friend std::istream& operator>>(std::istream& in, MachineConfig& config)
  {
    std::getline(in, config.machine_name);
    std::getline(in, config.remote_address);
    in >> config.port >> std::ws;
    read_until_dash(in, config.tags_produced);
    read_until_dash(in, config.tags_to_subscribe_to);
    read_until_dash(in, config.machines_to_connect_to);
    return in;
  }

};

#endif // SKYNET_MACHINE_CONFIG_HPP

// We're generating the header, so don't try to include it
#define SKYNET_GENERATE_MESSAGE_SIZE_HEADER

#include "skynet/message.hpp"
#include "skynet/utility/serialize.hpp"

#include <fstream>
#include <iostream>

int main()
{
  const auto size = serialize(skynet::Message{}).size();
  std::ofstream fout("message_network_size.hpp");
  if (!fout)
  {
    std::cerr << "Error opening file for output.\n";
    return 1;
  }
  fout << "#define SKYNET_GENERATED_MESSAGE_NETWORK_SIZE " << size << '\n';
}
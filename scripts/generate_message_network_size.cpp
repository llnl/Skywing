// We're generating the header, so don't try to include it
#define SKYNET_GENERATE_MESSAGE_SIZE_HEADER

#include "skynet/detail/message.hpp"
#include "skynet/detail/utility/serialize.hpp"

#include <fstream>
#include <iostream>

int main()
{
  const auto size = skynet::detail::to_bytes(skynet::detail::Message{}).size();
  std::ofstream fout("message_network_size.hpp");
  if (!fout)
  {
    std::cerr << "Error opening file for output.\n";
    return 1;
  }
  fout << "#define SKYNET_GENERATED_MESSAGE_NETWORK_SIZE " << size << '\n';
}
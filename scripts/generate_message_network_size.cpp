#include "skynet/internal/message_headers.hpp"
#include "skynet/internal/utility/serialize.hpp"

#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>

using namespace skynet::internal;

// Determine the number of bytes that that a serialized object will take up and
// output it to an ostream followed by a comma
template<typename T>
void output_header_size(T, std::ostream& out)
{
  static_assert(std::is_trivially_copyable<T>::value,
    "A header type is non-trivally copyable!\n"
    "Not fit for serialization as a header."
  );
  const auto size = T{}.to_bytes().size();
  out << size << ',';
  // The kind of check below would be nice to have, but doesn't seem possible
  // as the serialization removes padding
  // // For right now, the serialized header should ALWAYS be the same size as the
  // // class being serialized (or 0 if the class is empty).  This may change in
  // // the future, but make sure this is the case for now
  // if ((size != 0 && std::is_empty_v<T>) || size != sizeof(T))
  // {
  //   std::cerr
  //     << "One of the headers has a mis-matched size!\n"
  //     << "Expected a size of " << (std::is_empty_v<T> ? 0 : sizeof(T)) << '\n'
  //     << "But got a size of " << size << '\n';
  //   std::exit(1);
  // }
}

// Call something with each type in the list default constructed as the first argument
template<typename Callable, typename... T>
void for_each(TypeList<T...>, Callable c)
{
  (c(T{}), ...);
}

// Output all of the members of a container, seperated by commas
template<typename T>
void output_container(std::ostream& out, const T& container)
{
  std::copy(container.begin(), container.end() - 1, std::ostream_iterator<int>(out, ","));
  out << container.back();
}

int main()
{
  std::ofstream fout("message_header_information.hpp");
  if (!fout)
  {
    std::cerr << "Error opening file for output.\n";
    return 1;
  }
  // First calculate the size of
  const auto base_size = UniversalHeader{}.to_bytes().size();
  // The total number of headers
  constexpr auto num_headers = size<JobHeaders> + size<StatusHeaders>;
  fout
    << "#include <array>\n"
    << "#include \"skynet/internal/message_headers.hpp\"\n"
    << "namespace skynet::internal::header_info {\n"
    << "constexpr int base_size = " << base_size << ";\n"
    << "constexpr std::array<int, " << num_headers << "> continue_sizes{";
  // Output the size of each header when serialized
  for_each(JobHeaders{}, [&](auto val) { output_header_size(val, fout); });
  for_each(StatusHeaders{}, [&](auto val) { output_header_size(val, fout); });
  fout << "};\n";
  // Close the namespace
  fout << "}\n";
}

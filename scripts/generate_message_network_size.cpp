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

template<typename T>
void output_header_size(T, std::ostream& out)
{
  static_assert(std::is_trivially_copyable<T>::value,
    "A header type is non-trivally copyable!\n"
    "Not fit for serialization as a header."
  );
  out << to_bytes(T{}).size() << ',';
}

// Call something with each type in the list default constructed as the first argument
template<typename Callable, typename... T>
void for_each(TypeList<T...>, Callable c)
{
  int dummy[sizeof...(T)]{(c(T{}), 0)...};
  (void)dummy;
}

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
  const auto base_size = to_bytes(UniversalHeader{}).size();
  constexpr auto num_headers = size<JobHeaders> + size<StatusHeaders>;
  fout
    << "#include <array>\n"
    << "#include \"skynet/internal/message_headers.hpp\"\n"
    << "namespace skynet { namespace internal { namespace header_info {\n"
    << "constexpr int base_size = " << base_size << ";\n"
    << "constexpr std::array<int, " << num_headers << "> continue_sizes{";
  for_each(JobHeaders{}, [&](auto val) { output_header_size(val, fout); });
  for_each(StatusHeaders{}, [&](auto val) { output_header_size(val, fout); });
  fout << "};\n";
  // Close the namespace
  fout << "} } }\n";
}

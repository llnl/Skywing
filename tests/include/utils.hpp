#ifndef SKYNET_TEST_UTILS_HPP
#define SKYNET_TEST_UTILS_HPP

namespace skynet
{
  std::mt19937_64 make_prng() noexcept
  {
    // The number of bytes required for initilizing a Mersenne Twister
    constexpr auto bytes_needed =
        std::mt19937_64::word_size * std::mt19937_64::state_size;
    // Create the initial state
    using result_type = std::random_device::result_type;
    constexpr auto array_size = bytes_needed / sizeof(result_type);
    std::array<result_type, array_size> values;
    std::generate(values.begin(), values.end(), []() {
        return std::random_device{}();
    });
    // Seed the PRNG with the values
    std::seed_seq seq(values.begin(), values.end());
    return std::mt19937_64{seq};
  }
} // namespace skynet

#endif // SKYNET_TEST_UTILS_HPP

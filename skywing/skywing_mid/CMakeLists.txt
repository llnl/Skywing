# Headers enumerated in a FILE_SET.
target_sources(skywing_mid
  PUBLIC
  FILE_SET mid_api
  TYPE HEADERS
  FILES
  associative_vector.hpp
  asynchronous_iterative.hpp
  big_float.hpp
  data_input.hpp
  helpers/skynet_config.hpp
  helpers/skynet_helper.hpp
  idempotent_processor.hpp
  internal/iterative_helpers.hpp
  internal/pubsub_converter_helpers.hpp
  iterative_method.hpp
  iterative_resilience_policies.hpp
  jacobi_processor.hpp
  neighbor_data_handler.hpp
  power_method_processor.hpp
  publish_policies.hpp
  pubsub_converter.hpp
  push_flow_processor.hpp
  push_sum_processor.hpp
  quacc_processor.hpp
  stop_policies.hpp
  sum_processor.hpp
  synchronous_iterative.hpp
)

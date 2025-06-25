# Headers enumerated in a FILE_SET.
target_sources(skywing_mid
  PUBLIC
  FILE_SET mid_api
  TYPE HEADERS
  FILES
  associative_vector.hpp
  asynchronous_iterative.hpp
  big_float.hpp
  cola_processor.hpp
  data_input.hpp
  helpers/skywing_config.hpp
  helpers/skywing_helper.hpp
  idempotent_processor.hpp
  internal/iterative_helpers.hpp
  internal/pubsub_converter_helpers.hpp
  iterative_method.hpp
  iterative_resilience_policies.hpp
  data_handler.hpp
  power_method_processor.hpp
  publish_policies.hpp
  pubsub_converter.hpp
  push_flow_processor.hpp
  push_sum_processor.hpp
  push_sum_basic_processor.hpp
  quacc_processor.hpp
  iteration_policies.hpp
  sum_processor.hpp
  synchronous_iterative.hpp
  python_processor.hpp
  linear_system_processors/jacobi_processors/jacobi_processor.hpp
)

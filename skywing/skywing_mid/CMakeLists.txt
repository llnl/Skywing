# Headers enumerated in a FILE_SET.
target_sources(skywing_mid
  PUBLIC
  FILE_SET mid_api
  TYPE HEADERS
  FILES
  associative_vector.hpp
  associative_matrix.hpp
  asynchronous_iterative.hpp
  big_float.hpp
  data_handler.hpp
  data_input.hpp
  helpers/skywing_config.hpp
  helpers/skywing_helper.hpp
  idempotent_processor.hpp
  internal/iterative_helpers.hpp
  internal/pubsub_converter_helpers.hpp
  iteration_policies.hpp
  iterative_method.hpp
  iterative_resilience_policies.hpp
  linear_system_processors/admm_processor.hpp
  linear_system_processors/admm_shared_processor.hpp
  linear_system_processors/cola_processor.hpp
  linear_system_processors/jacobi_processor.hpp
  power_method_processor.hpp
  publish_policies.hpp
  pubsub_converter.hpp
  push_flow_processor.hpp
  push_sum_processor.hpp
  push_sum_basic_processor.hpp
  python_processor.hpp
  quacc_processor.hpp
  sum_processor.hpp
  synchronous_iterative.hpp
)

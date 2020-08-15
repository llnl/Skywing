async_jacobi_header_general_example_exe = executable(
  'async_jacobi_header_general',
  ['async_jacobi_header_general.cpp'],
  dependencies : [skynet_core_dep]
)

conf_data = configuration_data()
conf_data.set('async_jacobi_header_general_exe', async_jacobi_header_general_example_exe.full_path())

configure_file(input: 'run.sh.in', output: 'run.sh', configuration: conf_data)

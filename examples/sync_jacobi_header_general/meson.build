sync_jacobi_header_general_exe = executable(
  'sync_jacobi_header_general',
  ['sync_jacobi_header_general.cpp'],
  dependencies : [skynet_core_dep]
)

conf_data = configuration_data()
conf_data.set('sync_jacobi_header_general_exe', sync_jacobi_header_general_exe.full_path())

configure_file(input: 'run.sh.in', output: 'run.sh', configuration: conf_data)

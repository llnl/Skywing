push_sum_ps_exe = executable(
  'push_sum_ps',
  ['push_sum_ps.cpp'],
  dependencies : [skynet_core_dep]
)

conf_data = configuration_data()
conf_data.set('push_sum_ps_exe', push_sum_ps_exe.full_path())

configure_file(input: 'run.sh.in', output: 'run.sh', configuration: conf_data)

push_sum_exe = executable(
  'push_sum',
  ['push_sum.cpp'],
  dependencies : [skynet_core_dep]
)

conf_data = configuration_data()
conf_data.set('push_sum_exe', push_sum_exe.full_path())

configure_file(input: 'run.sh.in', output: 'run.sh', configuration: conf_data)

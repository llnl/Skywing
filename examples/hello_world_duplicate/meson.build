hello_world_duplicate_exe = executable(
  'hello_world_duplicate',
  ['hello_world_duplicate.cpp'],
  dependencies : [skynet_core_dep]
)

conf_data = configuration_data()
conf_data.set('hello_duplicate_exe', hello_world_duplicate_exe.full_path())

configure_file(input: 'run.sh.in', output: 'run.sh', configuration: conf_data)

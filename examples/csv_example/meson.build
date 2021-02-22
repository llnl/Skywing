csv_example_exe = executable(
  'csv_example',
  ['csv_example.cpp'],
  dependencies : [skynet_core_dep]
)

conf_data = configuration_data()
conf_data.set('csv_example_exe', csv_example_exe.full_path())

configure_file(input: 'run.sh.in', output: 'run.sh', configuration: conf_data)

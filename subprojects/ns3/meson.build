project('ns3', 'cpp')

ns3_deps = []

ns3_required_deps = [
   'core',
   'network',
   'point-to-point',
   'internet',
   'stats',
   'mpi',
   'bridge',
   'traffic-control',
   'applications',
   'config-store'
]

foreach dep : ns3_required_deps
  dep_suffix = get_option('buildtype').startswith('debug').to_string('-debug', '')
  ns3_deps += dependency('libns3-dev-' + dep + dep_suffix, required : true)
endforeach

ns3_dep = declare_dependency(dependencies : ns3_deps)

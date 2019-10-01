# Due to the way Meson works, have to have this seperate folder to output to
# a generated folder; so anything that produces files should go in here
# NOTE:
# Meson appears to run scripts in the BASE of build directory, so even
# things defined here will output to that by default.
# (i.e., will generally need to output to generated_includes/generated/file_name
#  instead of just file_name)

# The message network size script relies on just the serialization of Skynet
skynet_serialize= library('skynet_serialize',
  [
    '../../src/skynet/internal/utility/serialize.cpp'
  ],
  include_directories : include_directories('../../include', '../../src/include', '../../generated_includes')
)

make_network_message_size = executable(
  'make_network_message_size',
  '../../scripts/generate_message_network_size.cpp',
  dependencies : [
    declare_dependency(
      include_directories : include_directories('../../include', '../../src/include')
    )
  ],
  link_with : skynet_serialize
)

gen_message_header_information = custom_target(
  'gen_message_header_information',
  input : [],
  output : ['message_header_information.hpp'],
  command : [make_network_message_size, '@OUTPUT0@']
)

# Set up the platform specific include for endian stuff
endian_data = configuration_data()

endian_data.set('is_little_endian', (host_machine.endian() == 'little').to_string().to_lower())
if meson.get_compiler('cpp').get_id() == 'msvc'
  endian_data.set_quoted('endian_include', 'skynet/internal/endian_win.hpp')
else
  endian_data.set_quoted('endian_include', 'skynet/internal/endian_posix.hpp')
endif

configure_file(
  input : 'endian.hpp.in',
  output : 'endian.hpp',
  configuration : endian_data
)
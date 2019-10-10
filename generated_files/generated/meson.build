# Due to the way Meson works, have to have this seperate folder to output to
# a generated folder; so anything that produces files should go in here
# NOTE:
# Meson appears to run scripts in the BASE of build directory, so even
# things defined here will output to that by default.
# (i.e., will generally need to output to generated_includes/generated/file_name
#  instead of just file_name)

cpn_builder = find_program('capnp')
cpn_builder_cpp = find_program('capnpc-c++')

gen_messages = custom_target('gen_messages',
  input : ['message_format.capnp'],
  output : ['message_format.capnp.c++', 'message_format.capnp.h'],
  command : [
    cpn_builder,
    '-I' + meson.current_source_dir(),
    'compile',
    '--src-prefix=' + meson.current_source_dir(),
    '-o' + cpn_builder_cpp.path() + ':' + meson.current_build_dir(),
    '@INPUT0@'
  ]
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

generated_sources = [gen_messages]

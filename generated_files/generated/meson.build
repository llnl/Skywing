# Due to the way Meson works, have to have this seperate folder to output to
# a generated folder; so anything that produces files should go in here
# NOTE:
# Meson appears to run scripts in the BASE of build directory, so even
# things defined here will output to that by default.
# (i.e., will generally need to output to generated_includes/generated/file_name
#  instead of just file_name)

cpn_builder_prog = find_program('capnp')
cpn_builder_cpp_prog = find_program('capnpc-c++')

capn_proto_gen = generator(cpn_builder_prog,
  output : ['@PLAINNAME@.c++', '@PLAINNAME@.h'],
  arguments : [
    '-I' + meson.current_source_dir(),
    'compile',
    # This is ugly and I don't know why it was working before but isn't now
    # Just hardcode the path
    '--src-prefix=../generated_files/generated',
    '-o' + cpn_builder_cpp_prog.path() + ':@BUILD_DIR@',
    '@INPUT@'
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

generated_sources = capn_proto_gen.process('message_format.capnp')

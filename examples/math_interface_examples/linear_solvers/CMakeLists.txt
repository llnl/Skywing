configure_file(data/matrix.txt data/matrix.txt @ONLY)
configure_file(data/rhs.txt data/rhs.txt @ONLY)
configure_file(data/partition.txt data/partition.txt @ONLY)

add_subdirectory(jacobi)
add_subdirectory(lc_jacobi)
add_subdirectory(synchronizing_jacobi)
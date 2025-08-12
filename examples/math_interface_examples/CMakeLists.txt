add_subdirectory(barbell_jacobi)
add_subdirectory(cola)
add_subdirectory(jacobi)
add_subdirectory(lc_jacobi)
add_subdirectory(synchronizing_jacobi)

file(COPY
  jacobi_data/
  DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/jacobi_data)

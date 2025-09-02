add_subdirectory(barbell_jacobi)
add_subdirectory(jacobi)
add_subdirectory(lc_jacobi)
add_subdirectory(synchronizing_jacobi)

if (SKYWING_USE_EIGEN)
  add_subdirectory(cola)
endif ()

file(COPY
  jacobi_data/
  DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/jacobi_data)

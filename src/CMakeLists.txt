# To turn-on static linking feature, uncomment the following 3 lines.
# SET(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
# SET(BUILD_SHARED_LIBRARIES OFF)
# SET(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++ -static")

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} PROJECT_SRCS)
add_executable(psrs ${PROJECT_SRCS})

target_link_libraries(psrs ${MPI_C_LIBRARIES} m)
if(MPI_C_COMPILE_FLAGS)
      set_target_properties(psrs
          PROPERTIES COMPILE_FLAGS "${MPI_C_COMPILE_FLAGS}")
endif()

if(MPI_C_LINK_FLAGS)
      set_target_properties(psrs
          PROPERTIES LINK_FLAGS "${MPI_C_LINK_FLAGS}")
endif()

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    target_compile_definitions(psrs PRIVATE  PRINT_DEBUG_INFO=1)
endif()

# install(TARGETS main
# RUNTIME DESTINATION /usr/bin      COMPONENT Runtime)
# LIBRARY DESTINATION lib      COMPONENT Runtime
# ARCHIVE DESTINATION lib/timestamp COMPONENT Development)

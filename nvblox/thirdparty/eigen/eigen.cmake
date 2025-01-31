include(ExternalProject)

ExternalProject_Add(
    ext_eigen
    PREFIX eigen
    GIT_REPOSITORY git@gitlab.com:libeigen/eigen.git
    GIT_TAG 3.4.0
    UPDATE_COMMAND ""
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
)

add_library(nvblox_eigen INTERFACE)

target_include_directories(nvblox_eigen SYSTEM INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/eigen/include/eigen3/>
    $<INSTALL_INTERFACE:include/eigen3>
)
add_dependencies(nvblox_eigen ext_eigen)

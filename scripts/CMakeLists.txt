# scripts/CMakeLists.txt
# Python Topology Validation Integration for CMake
#
# This adds a 'validate_topology' target that runs the Python topology validator
# on all JSON config files in the configs/ directory.

find_package(Python3 QUIET)

if(Python3_EXECUTABLE)
    message(STATUS "Python3 found: ${Python3_EXECUTABLE}")

    # Helper function to add a single validation
    function(add_topology_validation name config)
        add_custom_target(${name}
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/topology_validator.py
                ${config}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Validating ${config}..."
        )
    endfunction()

    # Create individual validation targets
    add_topology_validation(validate_mesh_2x2 ${CMAKE_SOURCE_DIR}/configs/mesh_2x2.json)
    add_topology_validation(validate_mesh_4x4 ${CMAKE_SOURCE_DIR}/configs/mesh_4x4.json)
    add_topology_validation(validate_ring_8 ${CMAKE_SOURCE_DIR}/configs/ring_8.json)
    add_topology_validation(validate_hierarchical ${CMAKE_SOURCE_DIR}/configs/hierarchical_2x2.json)

    # Create a combined validation target
    add_custom_target(validate_topology
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/topology_validator.py
            ${CMAKE_SOURCE_DIR}/configs/mesh_2x2.json
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/topology_validator.py
            ${CMAKE_SOURCE_DIR}/configs/mesh_4x4.json
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/topology_validator.py
            ${CMAKE_SOURCE_DIR}/configs/ring_8.json
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/topology_validator.py
            ${CMAKE_SOURCE_DIR}/configs/hierarchical_2x2.json
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Validating all topology configurations..."
        DEPENDS cpptlm_core
    )

else()
    message(WARNING "Python3 not found - topology validation will not be available")
    message(STATUS "To enable validation, install Python 3 and ensure it is in PATH")
endif()

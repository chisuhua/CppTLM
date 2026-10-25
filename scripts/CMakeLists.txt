# scripts/CMakeLists.txt — Python 拓扑验证集成 (重构后)
#
# 5 子目录布局 (2026-06-08):
# - scripts/build/      build.sh, format.sh
# - scripts/test/       test.sh, run_all_tests.sh, ci_e2e_test.sh
# - scripts/pipeline/   run_full_pipeline.sh
# - scripts/topology/   topology_*.py, analyzer.py, path_tracer.py, credit_flow.py
# - scripts/stats/      stats_*.py, layout_manager.py, derive_expr.py
#
# 本文件调度 topology/ 下的 topology_validator.py 作为 cmake target。

find_package(Python3 QUIET)

if(Python3_EXECUTABLE)
    message(STATUS "Python3 found: ${Python3_EXECUTABLE}")

    set(TOPOLOGY_VALIDATOR "${CMAKE_SOURCE_DIR}/scripts/topology/topology_validator.py")

    # Helper function to add a single validation
    function(add_topology_validation name config)
        add_custom_target(${name}
            COMMAND ${Python3_EXECUTABLE} ${TOPOLOGY_VALIDATOR}
                ${config}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Validating ${config}..."
        )
    endfunction()

    # Create individual validation targets
    add_topology_validation(validate_mesh_2x2 ${CMAKE_SOURCE_DIR}/configs/mesh_2x2_tlm.json)
    add_topology_validation(validate_mesh_4x4 ${CMAKE_SOURCE_DIR}/configs/mesh_4x4_tlm.json)
    add_topology_validation(validate_ring_8 ${CMAKE_SOURCE_DIR}/configs/ring_8_tlm.json)
    add_topology_validation(validate_hierarchical ${CMAKE_SOURCE_DIR}/configs/hierarchical_2x2_tlm.json)

    # CEP 装配期判据的离线挂点(队列 #4,per ADR-CEP-09 抓取点 1)。
    # 目标定义在 examples/CMakeLists.txt(先于本文件 add_subdirectory)——
    # 若不存在则跳过,避免配置期硬失败;存在时与 module_factory 闸**共享同一组判据函数**。
    set(CEP_GATE_COMMAND "")
    set(CEP_GATE_DEPENDS "")
    if(TARGET cep_channel_dependency_gate)
        set(CEP_GATE_COMMAND COMMAND $<TARGET_FILE:cep_channel_dependency_gate>)
        set(CEP_GATE_DEPENDS cep_channel_dependency_gate)
    endif()

    # Create a combined validation target
    add_custom_target(validate_topology
        COMMAND ${Python3_EXECUTABLE} ${TOPOLOGY_VALIDATOR}
            ${CMAKE_BINARY_DIR}/configs/dgpu_board_v1_mvp.json
        COMMAND ${Python3_EXECUTABLE} ${TOPOLOGY_VALIDATOR}
            ${CMAKE_SOURCE_DIR}/configs/mesh_2x2_tlm.json
        COMMAND ${Python3_EXECUTABLE} ${TOPOLOGY_VALIDATOR}
            ${CMAKE_SOURCE_DIR}/configs/mesh_4x4_tlm.json
        COMMAND ${Python3_EXECUTABLE} ${TOPOLOGY_VALIDATOR}
            ${CMAKE_SOURCE_DIR}/configs/ring_8_tlm.json
        COMMAND ${Python3_EXECUTABLE} ${TOPOLOGY_VALIDATOR}
            ${CMAKE_SOURCE_DIR}/configs/hierarchical_2x2_tlm.json
        ${CEP_GATE_COMMAND}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Validating all topology configurations..."
        DEPENDS cpptlm_core ${CEP_GATE_DEPENDS}
    )

else()
    message(WARNING "Python3 not found - topology validation will not be available")
    message(STATUS "To enable validation, install Python 3 and ensure it is in PATH")
endif()

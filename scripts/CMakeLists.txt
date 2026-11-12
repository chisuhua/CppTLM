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

    # ── 拓扑校验 config 清单(**单一真源**)────────────────────────────────
    # 格式 = `<target 名>|<config 路径>`;**个体 target 与组合 target 均由此派生**
    # ⇒ 二者不可能漂移。
    # ⚠️ 2026-11-12(per 台账 Entry #358):此前个体 target 与组合 target **各手抄**了一份
    #    config 清单,组合那份的首条引用 `${CMAKE_BINARY_DIR}/configs/dgpu_board_v1_mvp.json`
    #    —— 该文件已被 T-bs-5 物理删除(根 CMakeLists.txt:93-94 注释)⇒ target 首条命令即
    #    FileNotFoundError ⇒ 其后 4 条校验 + CEP 装配闸 CLI 挂点**全部从未执行**。
    #    ⛔ 两处均不再手抄清单 ⇒ 该类漂移在结构上不可能再发生。
    set(CPPTLM_TOPOLOGY_VALIDATION_PAIRS
        "validate_mesh_2x2|${CMAKE_SOURCE_DIR}/configs/mesh_2x2_tlm.json"
        "validate_mesh_4x4|${CMAKE_SOURCE_DIR}/configs/mesh_4x4_tlm.json"
        "validate_ring_8|${CMAKE_SOURCE_DIR}/configs/ring_8_tlm.json"
        "validate_hierarchical|${CMAKE_SOURCE_DIR}/configs/hierarchical_2x2_tlm.json"
    )

    # Create individual validation targets + 派生组合 target 用的 config 清单
    set(CPPTLM_TOPOLOGY_VALIDATION_CONFIGS "")
    foreach(_pair IN LISTS CPPTLM_TOPOLOGY_VALIDATION_PAIRS)
        string(REPLACE "|" ";" _pair_parts "${_pair}")
        list(GET _pair_parts 0 _tgt_name)
        list(GET _pair_parts 1 _tgt_config)
        add_topology_validation(${_tgt_name} ${_tgt_config})
        list(APPEND CPPTLM_TOPOLOGY_VALIDATION_CONFIGS ${_tgt_config})
    endforeach()

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
    # ⛔ config 清单**从 CPPTLM_TOPOLOGY_VALIDATION_CONFIGS 派生**,不再手抄 ——
    #    手抄副本曾引用已删除文件 ⇒ 整条 target 跑不完 ⇒ 闸挂点从未执行。
    set(CPPTLM_TOPOLOGY_VALIDATION_COMMANDS "")
    foreach(_cfg IN LISTS CPPTLM_TOPOLOGY_VALIDATION_CONFIGS)
        list(APPEND CPPTLM_TOPOLOGY_VALIDATION_COMMANDS
            COMMAND ${Python3_EXECUTABLE} ${TOPOLOGY_VALIDATOR} ${_cfg})
    endforeach()

    add_custom_target(validate_topology
        ${CPPTLM_TOPOLOGY_VALIDATION_COMMANDS}
        ${CEP_GATE_COMMAND}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Validating all topology configurations..."
        DEPENDS cpptlm_core ${CEP_GATE_DEPENDS}
    )

else()
    message(WARNING "Python3 not found - topology validation will not be available")
    message(STATUS "To enable validation, install Python 3 and ensure it is in PATH")
endif()

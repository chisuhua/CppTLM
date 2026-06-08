# samples/CMakeLists.txt
# 演示程序构建配置（依赖 cpptlm_core）

# 所有 demo 共享的 include 目录（cpptlm_core 头文件）
set(SAMPLES_INCLUDE_DIRS
 ${CMAKE_SOURCE_DIR}/include
 ${CMAKE_SOURCE_DIR}/include/core
 ${CMAKE_SOURCE_DIR}/external/json
)

# stats_demo — 性能统计框架演示
add_executable(stats_demo stats_demo.cc)
target_link_libraries(stats_demo PRIVATE cpptlm_core)
target_include_directories(stats_demo PRIVATE ${SAMPLES_INCLUDE_DIRS})

# traffic_gen_demo — TrafficGenTLM 压力测试演示
add_executable(traffic_gen_demo traffic_gen_demo.cc)
target_link_libraries(traffic_gen_demo PRIVATE cpptlm_core)
target_include_directories(traffic_gen_demo PRIVATE ${SAMPLES_INCLUDE_DIRS})

# streaming_demo — StreamingReporter 实时统计输出演示
add_executable(streaming_demo streaming_demo.cc)
target_link_libraries(streaming_demo PRIVATE cpptlm_core)
target_include_directories(streaming_demo PRIVATE ${SAMPLES_INCLUDE_DIRS})

# ==============================================================================
# Python 演示 target（手动触发，避免 Python 错误打断默认构建）
# ==============================================================================

# topology_generator_demo — 演示 topology_generator.py 生成 5 种拓扑
add_custom_target(demo_topology_generator
 COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/samples/topology_generator_demo.py
 WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
 COMMENT "Running samples/topology_generator_demo.py"
)

# stats_visualization_demo — 演示 stats_annotator.py 生成 HTML 报告
add_custom_target(demo_stats_visualization
 COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/samples/stats_visualization_demo.py
 WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
 COMMENT "Running samples/stats_visualization_demo.py"
)

# stats_watcher_demo — 演示 stats_watcher.py 控制台监控
add_custom_target(demo_stats_watcher
 COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/samples/stats_watcher_demo.py
 WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
 COMMENT "Running samples/stats_watcher_demo.py"
)

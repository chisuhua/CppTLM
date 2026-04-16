# samples/CMakeLists.txt
# 演示程序构建配置（依赖 cpptlm_core）

# stats_demo — 性能统计框架演示
add_executable(stats_demo stats_demo.cc)
target_link_libraries(stats_demo PRIVATE cpptlm_core)

# traffic_gen_demo — TrafficGenTLM 压力测试演示
add_executable(traffic_gen_demo traffic_gen_demo.cc)
target_link_libraries(traffic_gen_demo PRIVATE cpptlm_core)

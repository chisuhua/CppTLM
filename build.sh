# 1. 创建构建目录
mkdir -p build && cd build

# 2. 生成 Makefile
cmake .. -DBUILD_TESTS=ON

# 3. 编译
make 

ctest --output-on-failure

make validate_topology

# 4. 运行
#./sim

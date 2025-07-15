#!/bin/bash

# 定义项目根目录
PROJECT_DIR=$(pwd)

# 删除并重新创建 build 目录
echo "Cleaning build directory..."
rm -rf "$PROJECT_DIR/build"
mkdir "$PROJECT_DIR/build"

# 进入 build 目录
cd "$PROJECT_DIR/build" || { echo "Error: Failed to enter build directory."; exit 1; }

# 运行 cmake 配置
echo "Configuring project with CMake..."
cmake .. || { echo "Error: CMake configuration failed."; exit 1; }

# 编译项目
echo "Building the project..."
make -j8
if [ $? -ne 0 ]; then
    echo "Error: Compilation failed."
    exit 1
fi

# 运行可执行文件
echo "Running the executable..."
./ProtocolStack -c 0xf

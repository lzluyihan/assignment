# 自瞄

## 工具链配置

由于后续采用cpp20+来编写代码
故需要以下工具链：
clang + cmake + ninja + vcpkg + git

1. Install Clang 19

``` zsh
wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
sudo ./llvm.sh 19 all
```

Configure as default compiler:

``` zsh
sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-19 100
sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-19 100
```

2. Install CMake

``` zsh
# Download and install latest CMake
wget https://github.com/Kitware/CMake/releases/download/v3.31.6/cmake-3.31.6-linux-x86_64.sh
chmod +x cmake-3.31.6-linux-x86_64.sh
sudo ./cmake-3.31.6-linux-x86_64.sh --prefix=/opt/cmake-3.31.6 --skip-license
sudo update-alternatives --install /usr/bin/cmake cmake /opt/cmake-3.31.6/bin/cmake 100
```

3. Install Ninja

``` zsh
sudo apt-get update
sudo apt-get install wget unzip
wget https://github.com/ninja-build/ninja/releases/download/v1.12.1/ninja-linux.zip
unzip ninja-linux.zip
sudo mv ninja /usr/bin/ninja
sudo chmod +x /usr/bin/ninja
```

4. Install vcpkg

``` zsh
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
cd ~/vcpkg
./bootstrap-vcpkg.sh
```

Add to ~/.bashrc(或者是zshrc):

``` zsh
export VCPKG_ROOT=~/vcpkg
export PATH=$VCPKG_ROOT:$PATH
```

## 编译

1. clone 源码

``` zsh
git clone https://github.com/Aurora-UJS/auto_aim_refactoring.git
```

2. 安装依赖项(按理来说用vcpkg管理是不需要的，但是还没测试过所以不确定，最好还是执行下)：
   - [MindVision SDK（这个不需要，我们是Daheng-galaxy）](https://mindvision.com.cn/category/software/sdk-installation-package/)或[HikRobot SDK(同上，不需要)](https://www.hikrobotics.com/cn2/source/support/software/MVS_STD_GML_V2.1.2_231116.zip)
   - [OpenVINO](https://docs.openvino.ai/2024/get-started/install-openvino/install-openvino-archive-linux.html)
   - [Ceres](http://ceres-solver.org/installation.html)
   - 其余：

    ```bash
    sudo apt install -y \
        git \
        g++ \
        can-utils \
        libopencv-dev \
        libfmt-dev \
        libeigen3-dev \
        libspdlog-dev \
        libyaml-cpp-dev \
        libusb-1.0-0-dev \
        nlohmann-json3-dev \
        openssh-server \
        screen
    ```

编译

``` zsh
cmake -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++ \
      -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

build

``` zsh
ninja -C build
```

run demo

``` zsh
./build/auto_aim_test
```

连上相机基本上就可以直接run相关的单元测试模块先调了～

# RingMaster

RingMaster 是一个由 io_uring 驱动的高性能 I/O 服务器，旨在展示高效、可扩展的网络应用，具有最小的开销。

## 特性

- 利用 Linux 的 io_uring 进行异步 I/O 操作
- 以最小的资源使用支持高并发
- 易于理解的代码库，非常适合学习和扩展
- 模块化设计，易于集成到更大的项目中

## 先决条件

在开始之前，请确保您的系统已安装以下依赖项：

### 基本开发工具

1. 更新软件包列表：
   ```
   sudo apt-get update
   ```

2. 安装基本构建工具：
   ```
   sudo apt-get install build-essential
   ```

3. 安装 GCC（如果未包含在 build-essential 中）：
   ```
   sudo apt-get install gcc
   ```

4. 安装 CMake：
   ```
   sudo apt-get install cmake
   ```

5. 安装 Git：
   ```
   sudo apt-get install git
   ```

### io_uring 依赖

6. 安装 liburing：
   ```
   sudo apt-get install liburing-dev
   ```

### 调试工具

7. 安装 GDB（用于调试）：
   ```
   sudo apt-get install gdb
   ```

8. 安装 Valgrind（用于内存泄漏检测）：
   ```
   sudo apt-get install valgrind
   ```

### 验证安装

安装后，您可以验证安装：

- GCC 版本：
  ```
  gcc --version
  ```
- CMake 版本：
  ```
  cmake --version
  ```
- Git 版本：
  ```
  git --version
  ```

### 系统要求

- Linux 内核版本 5.1 或更高（支持 io_uring）
  要检查您的内核版本，运行：
  ```
  uname -r
  ```
  如果您的内核版本低于 5.1，您需要升级内核。

### 其他注意事项

- 如果您使用的不是 Ubuntu 或 Debian，软件包名称和安装命令可能会有所不同。请参考您的发行版文档以获取等效的软件包。

- 对于某些系统，您可能需要将用户添加到 `sudo` 组以运行这些命令。如果遇到权限问题，请联系您的系统管理员。

- 如果您计划使用 CLion 进行开发（如本 README 后面所述），请确保已安装。您可以从 [JetBrains 网站](https://www.jetbrains.com/clion/) 下载。

安装完所有这些先决条件后，您就可以继续下载和编译 RingMaster 项目了。

## 详细安装和使用指南

### 步骤 1：下载源代码

1. 打开终端。

2. 克隆 RingMaster 仓库：
   ```
   git clone https://github.com/jacoblai/RingMaster.git
   ```

3. 进入项目目录：
   ```
   cd RingMaster
   ```

### 步骤 2：编译项目

1. 在 RingMaster 目录中，使用 GCC 编译项目：
   ```
   gcc -o ringmaster main.c iouring_server.c -luring
   ```

2. 如果编译成功，您应该看不到任何输出，并且在当前目录中会创建一个名为 `ringmaster` 的新可执行文件。

3. 如果遇到任何错误：
    - 确保正确安装了 liburing
    - 检查 GCC 版本是否兼容
    - 验证所有源文件都在正确的位置

### 步骤 3：运行服务器

1. 要启动服务器，使用以下命令：
   ```
   ./ringmaster <端口>
   ```
   将 `<端口>` 替换为您想要的端口号（例如 8080）。

2. 例如，要在端口 8080 上运行服务器：
   ```
   ./ringmaster 8080
   ```

3. 您应该看到类似以下的输出：
   ```
   Starting server with MAX_CONNECTIONS = 1024
   Starting server on port 8080
   Server started. Press Ctrl+C to stop.
   ```

### 步骤 4：测试服务器

1. 打开一个新的终端窗口或标签。

2. 使用 `telnet` 或 `nc`（netcat）等工具连接到服务器：

   使用 telnet：
   ```
   telnet localhost 8080
   ```

   或使用 netcat：
   ```
   nc localhost 8080
   ```

3. 连接后，输入一条消息并按 Enter。服务器应该会回显您的消息。

4. 要断开连接，关闭 telnet 或 nc 会话（通常通过按 Ctrl+C 或 Ctrl+D）。

### 步骤 5：停止服务器

要停止服务器，在运行服务器的终端中按 Ctrl+C。您应该会看到一条表示服务器正在关闭的消息。

## 配置选项

RingMaster 在 `iouring_server.h` 中有几个可配置的参数：

- `MAX_CONNECTIONS`：最大同时连接数（默认：1024）
- `BUFFER_SIZE`：每个连接的读/写缓冲区大小（默认：8192 字节）

要修改这些参数：

1. 在文本编辑器中打开 `iouring_server.h`。
2. 找到 `MAX_CONNECTIONS` 和 `BUFFER_SIZE` 的 `#define` 语句。
3. 根据需要更改值。
4. 保存文件并按照步骤 2 中的描述重新编译项目。

## 故障排除

1. **与 io_uring 相关的编译错误：**
    - 错误消息：`iouring.h: No such file or directory`
    - 解决方案：确保安装了 liburing。运行 `sudo apt-get install liburing-dev`

2. **启动服务器时出现 "Address already in use" 错误：**
    - 错误消息：`bind: Address already in use`
    - 解决方案：所选端口正在使用中。等待几分钟或选择不同的端口。

3. **服务器不接受连接：**
    - 检查防火墙是否阻止了端口。尝试暂时禁用防火墙。
    - 确保您连接到正确的 IP 和端口。

4. **CPU 使用率高：**
    - 此服务器设计用于高性能，可能会使用大量 CPU。这是正常行为。

5. **内存使用随时间增长：**
    - 服务器设计用于处理多个并发连接。如果内存使用似乎过高，使用 Valgrind 等工具检查内存泄漏。

## 使用 CLion 进行开发

CLion 是一个强大的 C 和 C++ 开发 IDE。以下是如何在 CLion 中设置和使用 RingMaster：

### 导入项目

1. 打开 CLion。
2. 在欢迎屏幕中点击 "Open" 或 "Open or Import"。
3. 导航到克隆 RingMaster 仓库的目录并选择它。
4. CLion 应该会自动检测项目结构。如果提示，选择 "Open Project"。

### 设置项目

1. 打开项目后，转到 `File > Settings`（在 Windows/Linux 上）或 `CLion > Preferences`（在 macOS 上）。
2. 导航到 `Build, Execution, Deployment > CMake`。
3. 在 "CMake options" 字段中，添加：`-DCMAKE_C_COMPILER=gcc`
4. 点击 "Apply" 然后 "OK"。

### 创建 CMakeLists.txt

由于项目没有 CMakeLists.txt 文件，您需要创建一个：

1. 在项目浏览器中右键点击项目根目录。
2. 选择 `New > CMakeLists.txt`。
3. 添加以下内容到文件：

```cmake
cmake_minimum_required(VERSION 3.10)
project(RingMaster C)

set(CMAKE_C_STANDARD 11)

# 查找 liburing
find_library(URING_LIB uring)

# 添加可执行文件
add_executable(ringmaster main.c iouring_server.c)

# 链接 liburing
target_link_libraries(ringmaster ${URING_LIB})
```

4. 保存文件。

### 配置运行/调试

1. 点击 CLion 右上角的 "Add Configuration"。
2. 点击 "+" 按钮并选择 "CMake Application"。
3. 设置以下内容：
    - 名称：RingMaster
    - 目标：ringmaster
    - 程序参数：8080（或您喜欢的端口）
4. 点击 "Apply" 然后 "OK"。

### 构建项目

1. 转到 `Build > Build Project` 或使用键盘快捷键（通常是 Ctrl+F9 或 Cmd+F9）。
2. CLion 将编译项目并在 "Build" 工具窗口中报告任何错误。

### 运行项目

1. 从右上角的运行配置下拉菜单中选择 "RingMaster"。
2. 点击绿色的 "Run" 按钮或按 Shift+F10（在 macOS 上是 Ctrl+R）。
3. 服务器将启动，您将在 "Run" 工具窗口中看到输出。

### 调试

1. 通过点击编辑器左边距所需行来设置断点。
2. 从运行配置下拉菜单中选择 "RingMaster"。
3. 点击绿色的 "Debug" 按钮或按 Shift+F9（在 macOS 上是 Ctrl+D）。
4. 调试器将启动，执行将在您的断点处暂停。

### CLion 设置故障排除

1. **CMake 项目未加载：**
    - 确保您的系统上安装了 CMake。
    - 尝试重新加载 CMake 项目：`Tools > CMake > Reload CMake Project`。

2. **找不到 liburing：**
    - 确保您的系统上安装了 liburing。
    - 您可能需要在 CMakeLists.txt 中指定 liburing 的路径：
      ```cmake
      set(URING_INCLUDE_DIR "/path/to/liburing/include")
      set(URING_LIB_DIR "/path/to/liburing/lib")
      include_directories(${URING_INCLUDE_DIR})
      link_directories(${URING_LIB_DIR})
      ```

3. **编译错误：**
    - 检查您的 GCC 版本是否兼容（建议 7.5.0 或更高版本）。
    - 确保所有必要的头文件都可用。您可能需要安装额外的开发包。

4. **调试器不工作：**
    - 确保您的系统上安装了 GDB。
    - 检查 CLion 的调试器设置：`File > Settings > Build, Execution, Deployment > Debugger`。

记得在对 CMakeLists.txt 或项目设置进行更改后重新构建项目。

### 设置 CLion 远程调试

远程调试允许您从本地 CLion IDE 调试在远程机器（例如生产或预发布服务器）上运行的应用程序。以下是如何设置：

#### 在远程机器上：

1. 确保安装了 GDB 服务器。在 Ubuntu/Debian 上，您可以用以下命令安装：
   ```
   sudo apt-get install gdbserver
   ```

2. 使用调试符号编译项目：
   ```
   gcc -g -o ringmaster main.c iouring_server.c -luring
   ```

3. 使用 gdbserver 启动应用程序：
   ```
   gdbserver *:1234 ./ringmaster 8080
   ```
   这会启动 gdbserver，在所有接口的 1234 端口上监听，并在 8080 端口上运行 RingMaster 服务器。

#### 在 CLion 中（在您的本地机器上）：

1. 转到 `Run > Edit Configurations`。

2. 点击 `+` 按钮并选择 `GDB Remote Debug`。

3. 设置配置：
    - 名称：`RingMaster Remote Debug`
    - 'target remote' 参数：`tcp://remote_ip:1234`
      （将 `remote_ip` 替换为您远程机器的 IP 地址）
    - 路径映射：
      本地路径：`/path/to/local/RingMaster`
      远程路径：`/path/to/remote/RingMaster`
    - 启动前：添加一个新任务 `Build` 以确保编译最新代码。

4. 点击 `Apply` 和 `OK`。

#### 开始远程调试会话：

1. 根据需要在本地代码中设置断点。

2. 从右上角的下拉菜单中选择 `RingMaster Remote Debug` 配置。

3. 点击 `Debug` 按钮（虫子图标）或按 Shift+F9（在 macOS 上是 Ctrl+D）。

4. CLion 将连接到远程 gdbserver 并在 `main` 函数处停止。

5. 现在您可以像调试本地运行的应用程序一样进行调试。

#### 远程调试故障排除：

1. **连接被拒绝错误：**
    - 确保 gdbserver 在远程机器上运行。
    - 检查是否有防火墙阻止了 1234 端口的连接。

2. **找不到调试符号：**
    - 确保在远程机器上使用 `-g` 标志编译程序。

3. **断点不触发：**
    - 验证本地和远程代码是否完全相同。
    - 检查调试配置中的路径映射。

4. **GDB 服务器版本不匹配：**
    - 尝试使本地机器上的 GDB 版本与远程服务器上的版本匹配。

5. **无法步进标准库代码：**
    - 您可能需要在远程机器上安装标准库的调试符号。
      在 Ubuntu/Debian 上：`sudo apt-get install libc6-dbg`

记得在进行更改后在远程机器上重新构建项目，并使用新的二进制文件重新启动 gdbserver。

#### 安全考虑：

- 远程调试会在您的服务器上打开一个端口。确保这个端口得到适当的保护，最好只能通过 VPN 或 SSH 隧道访问。
- 永远不要以 root 身份运行 gdbserver。始终使用具有有限权限的用户。
- 在生产环境中，谨慎使用远程调试，只在必要时使用。

## 贡献

欢迎对 RingMaster 做出贡献！请随时提交 Pull Request。

## 许可证

本项目采用 MIT 许可证 - 有关详细信息，请参阅 LICENSE 文件。

## 联系方式

如果您想联系开发者，可以通过以下方式：
- 电子邮件：229292620@qq.com
- GitHub：[https://github.com/jacoblai](https://github.com/jacoblai)

## 致谢

- [io_uring](https://kernel.dk/io_uring.pdf) - Linux 内核的 io_uring 接口
- [liburing](https://github.com/axboe/liburing) - io_uring 接口的库
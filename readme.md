# RingMaster

RingMaster is a high-performance I/O server powered by io_uring, designed to demonstrate efficient, scalable network applications with minimal overhead.

## Features

- Utilizes Linux's io_uring for asynchronous I/O operations
- Supports high concurrency with minimal resource usage
- Easy-to-understand codebase, great for learning and extending
- Modular design allowing easy integration into larger projects

## Prerequisites

Before you begin, ensure your system has the following dependencies installed:

### Basic Development Tools

1. Update your package list:
   ```
   sudo apt-get update
   ```

2. Install essential build tools:
   ```
   sudo apt-get install build-essential
   ```

3. Install GCC (if not already included in build-essential):
   ```
   sudo apt-get install gcc
   ```

4. Install CMake:
   ```
   sudo apt-get install cmake
   ```

5. Install Git:
   ```
   sudo apt-get install git
   ```

### io_uring Dependencies

6. Install liburing:
   ```
   sudo apt-get install liburing-dev
   ```

### Debugging Tools

7. Install GDB (for debugging):
   ```
   sudo apt-get install gdb
   ```

8. Install Valgrind (for memory leak detection):
   ```
   sudo apt-get install valgrind
   ```

### Verify Installations

After installing, you can verify the installations:

- GCC version:
  ```
  gcc --version
  ```
- CMake version:
  ```
  cmake --version
  ```
- Git version:
  ```
  git --version
  ```

### System Requirements

- Linux kernel version 5.1 or later (for io_uring support)
  To check your kernel version, run:
  ```
  uname -r
  ```
  If your kernel version is lower than 5.1, you'll need to upgrade your kernel.

### Additional Notes

- If you're using a distribution other than Ubuntu or Debian, the package names and installation commands may vary. Please refer to your distribution's documentation for the equivalent packages.

- For some systems, you might need to add your user to the `sudo` group to run these commands. If you encounter permission issues, contact your system administrator.

- If you plan to use CLion for development (as described later in this README), make sure you have it installed. You can download it from the [JetBrains website](https://www.jetbrains.com/clion/).

Once you have all these prerequisites installed, you're ready to proceed with downloading and compiling the RingMaster project.

## Detailed Installation and Usage Guide

### Step 1: Download the Source Code

1. Open a terminal.

2. Clone the RingMaster repository:
   ```
   git clone https://github.com/jacoblai/RingMaster.git
   ```

3. Navigate to the project directory:
   ```
   cd RingMaster
   ```

### Step 2: Compile the Project

1. In the RingMaster directory, compile the project using GCC:
   ```
   gcc -o ringmaster main.c iouring_server.c -luring
   ```

2. If the compilation is successful, you should see no output, and a new executable file named `ringmaster` will be created in the current directory.

3. If you encounter any errors:
    - Ensure liburing is correctly installed
    - Check that your GCC version is compatible
    - Verify that all source files are in the correct location

### Step 3: Run the Server

1. To start the server, use the following command:
   ```
   ./ringmaster <port>
   ```
   Replace `<port>` with your desired port number (e.g., 8080).

2. For example, to run the server on port 8080:
   ```
   ./ringmaster 8080
   ```

3. You should see output similar to:
   ```
   Starting server with MAX_CONNECTIONS = 1024
   Starting server on port 8080
   Server started. Press Ctrl+C to stop.
   ```

### Step 4: Test the Server

1. Open a new terminal window or tab.

2. Use a tool like `telnet` or `nc` (netcat) to connect to the server:

   Using telnet:
   ```
   telnet localhost 8080
   ```

   Or using netcat:
   ```
   nc localhost 8080
   ```

3. Once connected, type a message and press Enter. The server should echo your message back.

4. To disconnect, close the telnet or nc session (usually by pressing Ctrl+C or Ctrl+D).

### Step 5: Stop the Server

To stop the server, press Ctrl+C in the terminal where it's running. You should see a message indicating that the server is shutting down.

## Configuration Options

RingMaster has several configurable parameters in `iouring_server.h`:

- `MAX_CONNECTIONS`: Maximum number of simultaneous connections (default: 1024)
- `BUFFER_SIZE`: Size of the read/write buffers for each connection (default: 8192 bytes)

To modify these:

1. Open `iouring_server.h` in a text editor.
2. Locate the `#define` statements for `MAX_CONNECTIONS` and `BUFFER_SIZE`.
3. Change the values as needed.
4. Save the file and recompile the project as described in Step 2.

## Troubleshooting

1. **Compilation errors related to io_uring:**
    - Error message: `iouring.h: No such file or directory`
    - Solution: Ensure liburing is installed. Run `sudo apt-get install liburing-dev`

2. **"Address already in use" error when starting the server:**
    - Error message: `bind: Address already in use`
    - Solution: The chosen port is in use. Either wait a few minutes or choose a different port.

3. **Server doesn't accept connections:**
    - Check if your firewall is blocking the port. Try temporarily disabling the firewall.
    - Ensure you're connecting to the correct IP and port.

4. **High CPU usage:**
    - This server is designed for high performance and may use significant CPU. This is normal behavior.

5. **Memory usage grows over time:**
    - The server is designed to handle many concurrent connections. If memory usage seems excessive, use tools like Valgrind to check for memory leaks.

## Developing with CLion

CLion is a powerful IDE for C and C++ development. Here's how to set up and use RingMaster in CLion:

### Importing the Project

1. Open CLion.
2. Click on "Open" or "Open or Import" in the welcome screen.
3. Navigate to the directory where you cloned the RingMaster repository and select it.
4. CLion should automatically detect the project structure. If prompted, select "Open Project".

### Setting up the Project

1. Once the project is open, go to `File > Settings` (on Windows/Linux) or `CLion > Preferences` (on macOS).
2. Navigate to `Build, Execution, Deployment > CMake`.
3. In the "CMake options" field, add: `-DCMAKE_C_COMPILER=gcc`
4. Click "Apply" and then "OK".

### Creating CMakeLists.txt

Since the project doesn't come with a CMakeLists.txt file, you'll need to create one:

1. Right-click on the project root in the project explorer.
2. Select `New > CMakeLists.txt`.
3. Add the following content to the file:

```cmake
cmake_minimum_required(VERSION 3.10)
project(RingMaster C)

set(CMAKE_C_STANDARD 11)

# Find liburing
find_library(URING_LIB uring)

# Add executable
add_executable(ringmaster main.c iouring_server.c)

# Link against liburing
target_link_libraries(ringmaster ${URING_LIB})
```

4. Save the file.

### Configuring Run/Debug

1. Click on "Add Configuration" in the top-right corner of CLion.
2. Click the "+" button and select "CMake Application".
3. Set the following:
    - Name: RingMaster
    - Target: ringmaster
    - Program arguments: 8080 (or your preferred port)
4. Click "Apply" and then "OK".

### Building the Project

1. Go to `Build > Build Project` or use the keyboard shortcut (usually Ctrl+F9 or Cmd+F9).
2. CLion will compile the project and report any errors in the "Build" tool window.

### Running the Project

1. Select "RingMaster" from the run configurations dropdown in the top-right corner.
2. Click the green "Run" button or press Shift+F10 (Ctrl+R on macOS).
3. The server will start, and you'll see the output in the "Run" tool window.

### Debugging

1. Set breakpoints by clicking in the left margin of the editor at the desired line.
2. Select "RingMaster" from the run configurations dropdown.
3. Click the green "Debug" button or press Shift+F9 (Ctrl+D on macOS).
4. The debugger will start, and execution will pause at your breakpoints.

### Troubleshooting CLion Setup

1. **CMake project is not loaded:**
    - Ensure you have CMake installed on your system.
    - Try reloading the CMake project: `Tools > CMake > Reload CMake Project`.

2. **Cannot find liburing:**
    - Make sure liburing is installed on your system.
    - You might need to specify the path to liburing in CMakeLists.txt:
      ```cmake
      set(URING_INCLUDE_DIR "/path/to/liburing/include")
      set(URING_LIB_DIR "/path/to/liburing/lib")
      include_directories(${URING_INCLUDE_DIR})
      link_directories(${URING_LIB_DIR})
      ```

3. **Compilation errors:**
    - Check that your GCC version is compatible (7.5.0 or later recommended).
    - Ensure all necessary headers are available. You might need to install additional development packages.

4. **Debugger doesn't work:**
    - Ensure you have GDB installed on your system.
    - Check CLion's debugger settings in `File > Settings > Build, Execution, Deployment > Debugger`.

Remember to rebuild the project after making changes to CMakeLists.txt or project settings.

### Setting up Remote Debugging with CLion

Remote debugging allows you to debug your application running on a remote machine (e.g., a production or staging server) from your local CLion IDE. Here's how to set it up:

#### On the Remote Machine:

1. Ensure GDB server is installed. On Ubuntu/Debian, you can install it with:
   ```
   sudo apt-get install gdbserver
   ```

2. Compile the project with debug symbols:
   ```
   gcc -g -o ringmaster main.c iouring_server.c -luring
   ```

3. Start the application with gdbserver:
   ```
   gdbserver *:1234 ./ringmaster 8080
   ```
   This starts gdbserver, listening on all interfaces on port 1234, and runs the RingMaster server on port 8080.

#### In CLion (on your local machine):

1. Go to `Run > Edit Configurations`.

2. Click the `+` button and select `GDB Remote Debug`.

3. Set up the configuration:
    - Name: `RingMaster Remote Debug`
    - 'target remote' args: `tcp://remote_ip:1234`
      (Replace `remote_ip` with the IP address of your remote machine)
    - Path mappings:
      Local path: `/path/to/local/RingMaster`
      Remote path: `/path/to/remote/RingMaster`
    - Before launch: Add a new task `Build` to ensure the latest code is compiled.

4. Click `Apply` and `OK`.

#### Starting a Remote Debug Session:

1. Set breakpoints in your local code as needed.

2. Select the `RingMaster Remote Debug` configuration from the dropdown in the top-right corner.

3. Click the `Debug` button (bug icon) or press Shift+F9 (Ctrl+D on macOS).

4. CLion will connect to the remote gdbserver and stop at the `main` function.

5. You can now debug as if the application was running locally.

#### Troubleshooting Remote Debugging:

1. **Connection refused error:**
    - Ensure gdbserver is running on the remote machine.
    - Check if any firewall is blocking the connection on port 1234.

2. **No debug symbols found:**
    - Make sure you compiled the program with the `-g` flag on the remote machine.

3. **Breakpoints not hitting:**
    - Verify that the local and remote code are identical.
    - Check the path mappings in the debug configuration.

4. **GDB server version mismatch:**
    - Try to match the GDB version on your local machine with the one on the remote server.

5. **Cannot step through standard library code:**
    - You may need to install debug symbols for standard libraries on the remote machine.
      On Ubuntu/Debian: `sudo apt-get install libc6-dbg`

Remember to rebuild your project on the remote machine after making changes, and restart gdbserver with the new binary.

#### Security Considerations:

- Remote debugging opens a port on your server. Ensure this port is properly secured, preferably only accessible through a VPN or SSH tunnel.
- Never run gdbserver as root. Always use a user with limited privileges.
- In production environments, use remote debugging cautiously and only when necessary.

## Contributing

Contributions to RingMaster are welcome! Please feel free to submit a Pull Request.

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Contact

If you want to contact the developer, you can reach out at:
- Email: 229292620@qq.com
- GitHub: [https://github.com/jacoblai](https://github.com/jacoblai)

## Acknowledgements

- [io_uring](https://kernel.dk/io_uring.pdf) - The Linux kernel's io_uring interface
- [liburing](https://github.com/axboe/liburing) - Library for io_uring interface
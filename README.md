# Task Manager

A C++ Task Manager application that displays system processes and resource usage information.

## Features

- Real-time CPU usage monitoring
- Memory usage tracking
- Disk usage monitoring
- Process list with detailed information
- Modern Qt-based GUI

## Requirements

- CMake 3.16 or higher
- Qt 6
- C++17 compatible compiler
- Windows OS (for system monitoring features)

## Building the Application

1. Create a build directory:
```bash
mkdir build
cd build
```

2. Configure the project with CMake:
```bash
cmake ..
```

3. Build the project:
```bash
cmake --build .
```

## Running the Application

After building, you can run the application from the build directory:
```bash
./TaskManager
```

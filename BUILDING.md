# Build Projects

It is recommended to compile the project with CMake preset and workflow.

## Environment Requirement

* Visual Studio 2022 (17) OR Visual Studio 2026 (18)
* Windows SDK 10.0.26100.0
* CMake 4.0 or higher

## x86 64-bit

CMake configuration:  

```shell
cmake --preset vs2022-amd64
```
OR
```shell
cmake --preset vs2026-amd64
```

Build (Debug or Release profile): 

```shell
cmake --build --preset windows-amd64-debug
cmake --build --preset windows-amd64-release
```

Release (configuration and build Release profile):  

```shell
cmake --workflow --preset windows-amd64-release
```
# Software requirements
- Windows
- CMake (version >= 3.17.0)
- Visual Studio 2019 (VS 16)
	- Requires Workload: "Desktop development with C++"
- If you get a .dll error, you also need to install Visual C++ Runtime 2019

# Create a project (Visual Studio 2019, Windows 10)
- create `build` directory
- `cd build`
- 32-bit:
  - `cmake -A Win32 -G "Visual Studio 16" ..`
- 64-bit:
  - `cmake -A x64 -G "Visual Studio 16" ..`

# To open the project in Visual Studio
- double-click build/mc2.sln

# To build mc2 from the command line
- `cd build`
- cmake --build . --config Release

# To switch from 32-bit to 64-bit or vice-versa
- delete `build` folder
- follow above instructions

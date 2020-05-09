# Software requirements:
- Windows
- CMake (version >= 3.17.0)
- Visual Studio 2019 (VS 16)
	- In Visual Studio Installer, make sure you select the workload: "Desktop development with C++"
- If you get a .dll error, install Visual C++ Runtime 2019

# Creating a project (Visual Studio 2019, Windows 10):
- create `build` directory
- `cd build`
- Create the project in either 32-bit or 64-bit mode:
  - 32-bit: `cmake -A Win32 -G "Visual Studio 16" ..`
  - 64-bit: `cmake -A x64 -G "Visual Studio 16" ..`

# To build and run mc2 from the command line:
- `cd build`
- `cmake --build . --config Release`
- double-click `bin/mc2.exe`

# To open the project in Visual Studio:
- double-click build/mc2.sln

# To switch from 32-bit to 64-bit or vice-versa:
- delete `build` folder
- follow above instructions

## A Minecraft-like game using C++ and OpenGL.

This is a Minecraft-like game created from scratch using C++ and OpenGL.

[![Youtube](https://img.youtube.com/vi/DtxxP2QCIko/0.jpg)](https://www.youtube.com/watch?v=DtxxP2QCIko)

## Running

You can download and run the [latest release](https://github.com/serg06/mc2/releases), or you can [compile it yourself](INSTRUCTIONS.md). (Requirements: Windows 10, OpenGL 4.6+, and if you run an older version (before v0.2.3-alpha), [VC++ 2019 Runtime](https://support.microsoft.com/en-ca/help/2977003/the-latest-supported-visual-c-downloads)).

## But why?

At its core, Minecraft is a very simple game. A world made of blocks, a couple low-poly entities, and textures which would've been considered low-res 20 years ago. So why does it run *so* poorly?

This started out as a project to learn C++ and OpenGL, but after achieving that, my goal has changed: I want to create a version of Minecraft that's so efficient and smooth, it can run better in the browser than it does on the desktop.

## Next features to implement:

- Cave generation
- Inventory
- Setting up models/textures by reading Minecraft's json/png files directly
- Rendering water height properly (each corner gets a water height depending on the 4 surrounding blocks, then each square's top texture is generated from the 4 corner heights)
- Entities using an entity-component system library

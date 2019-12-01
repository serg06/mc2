# (In-progress): A Minecraft-like game using C++ and OpenGL.

[![IMAGE ALT TEXT HERE](https://img.youtube.com/vi/32xDZ_fhXz4/0.jpg)](https://www.youtube.com/watch?v=32xDZ_fhXz4)

This is a from-scratch creation of a Minecraft-like game, using C++ and OpenGL.

OpenGL: No previous experience, learning it from *OpenGL Superbible*.

C++: No previous experience, learning it on-the-fly, from *OpenGL Superbible*, and from my existing expertise with Java and C.

## Wanna try it out?

Download the latest [release](https://github.com/serg06/mc2/releases) (Windows 10, OpenGL 4.5+.)

## Current features:

- Basic chunk generation, with chunk data efficiently stored in a 65536-byte array.

- Smooth player movement (flying), smooth-ish collisions (WIP), smooth looking around.

- OpenGL draws each chunk (65536 blocks) with a single draw call, indexing into data using only a blockId.

- Handles window resizing. 

## Some planned features:

- Optimal data organization and OpenGL calls for super high FPS.

- Newest OpenGL features/commands.

- Offloading as much work as possible to the GPU, while also minimizing draw calls.

- Excellent object-oriented C++ code and project structure.

- Automatic world generation.
	- Lakes/mountains/hills/trees
	- Different biomes.
	- Custom structures.

- Algorithm for deterministic world generation; that is, given a seed and a chunk coordinate, I can determine exactly what structure/biome/etc will be in that chunk.

- Automatic chunk offloading to keep RAM and VRAM from filling up.

- Perfectly smooth mob/block collision, including sliding along walls when walking diagonally. (In the future I might add support for non-cube world blocks (like slabs or window panes in Minecraft.)

- Orgasmic look and feel: prioritizing stuff like mouse latency, intuitive movement, low frame-time.

- Data chunking for easy map storage and drawing.
	- Super efficient map storage, including smart algorithms to fit the job; no more 2GB save files like in Minecraft.

- Skybox and butts.

- Settings on settings on settings. Enough settings to allow even the slowest PC to run at a reasonable framerate.

- Making sure to exclude all the issues that Minecraft has.

## Reasoning behind this project:

At its core, Minecraft is a simple game. The world data is structured, the animations are simple, and collisions are un-complex. The result? Minecraft is a game that can be created *from scratch*. I can make this game and call it mine.

I have a lot of simple yet excellent game concepts in my head; after this project, I'll finally know enough to be able to bring them to life. 

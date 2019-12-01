# (In-progress): A Minecraft-like game using C++ and OpenGL.

This is a from-scratch creation of a Minecraft-like game, using C++ and OpenGL. (Learning both from *OpenGL Superbible*.)

Progress video (December 1, 2019):

[![IMAGE ALT TEXT HERE](https://img.youtube.com/vi/32xDZ_fhXz4/0.jpg)](https://www.youtube.com/watch?v=32xDZ_fhXz4)

## Wanna try it out?

Download the latest [release](https://github.com/serg06/mc2/releases) (Windows 10, OpenGL 4.5+.)

## Current features:

- Infinite world generation, world is split into 16x256x16 chunks, chunks are split into 16x16x16 mini-chunks. One mini-chunk is rendered per draw call.

- Culling: Mini-chunks are culled out when they're surrounded by blocks on all sides.

- Movement: Smooth player movement (flying).

- Collisions: Super smooth and quite perfect (at reasonable refresh rates).

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

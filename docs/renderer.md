# Renderers

The engine has two renderers, chosen when it is built. Which one is in use is what
decides most of what the port's video code looks like.

## Software: the fallback (`HeXen2NX_sw`)

uHexen2's software renderer in plain C, without its x86 assembly. It draws 8-bit
pixels into a buffer of 640x360, and `source/platform/vid_nx.c` expands them through
the palette into a libnx framebuffer. The system scales that window up to the
screen. It needs nothing but the CPU, which makes it the way to tell a problem of
the platform from a problem of the GPU code.

## deko3D: the goal

The plan for stage 4, not written yet. uHexen2's OpenGL renderer (`gl_*.c`) is kept,
for it already knows the game: fullbrights, translucent water, the class skins, the
particles. Its calls go to a small fixed-function layer on deko3D instead of a GL
driver:

- **State**: blending, alpha test, depth and culling are tracked, and become
  deko3D state objects when something is drawn.
- **Geometry**: `glBegin`/`glEnd` pieces become triangles in a streaming vertex
  buffer. Draws in the same state are batched into one, which is what makes up for
  a GL-1-era renderer's habit of drawing surface by surface.
- **Shaders**: the few fixed-function combinations the renderer uses become GLSL
  shaders, compiled to DKSH by `uam` when the port is built.
- **Textures**: the engine already expands its 8-bit textures to RGBA. Uploads go
  through a staging buffer, and lightmap updates keep their order against the draws
  around them with a barrier.
- **Palette effects**: damage flashes and underwater tint become a full screen pass.

# Tilt Ball (N64, libdragon)

Build (requires libdragon toolchain: https://github.com/DragonMinded/libdragon):

    libdragon init      # first time only, via libdragon CLI
    libdragon make      # produces tiltball.z64

or, with a local install:  `export N64_INST=/opt/libdragon && make`

Controls: Analog stick = tilt board, A = jump, C-Left/C-Right = orbit camera, Start = reset.

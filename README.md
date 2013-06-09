Etnaviv Mesa fork
=================

Initial steps toward an open source GLES1/2 driver for Vivante GPU hardware.

This is being maintained as a set of patches on top of the Mesa main repository. Expect frequent rebases.

Build instructions
-------------------

To be written.

My configure script for cubox:
```bash
#!/bin/bash
DIR=... # path to target headers and libraries
ETNAVIV_BASE="${HOME}/projects/etna_viv"
ETNAVIV_LIB="${ETNAVIV_BASE}/native/etnaviv"
ETNAVIV_INC="${ETNAVIV_BASE}/native"
GCABI_INC="${ETNAVIV_BASE}/native/include_dove"

export TARGET="arm-linux-gnueabihf"
export CFLAGS="-I${DIR}/cubox/include -I${ETNAVIV_INC} -I${GCABI_INC}"
export CXXFLAGS="-I${DIR}/cubox/include -I${ETNAVIV_INC} -I${GCABI_INC}"
export LDFLAGS="-L${DIR}/cubox/lib -L${ETNAVIV_LIB}"
export LIBDRM_LIBS="-L${DIR}/cubox/lib -ldrm"
export ETNA_LIBS="-letnaviv"
./configure --target=${TARGET} --host=${TARGET} \
    --enable-gles2 --enable-gles1 --disable-glx --enable-egl --enable-dri \
    --with-gallium-drivers=swrast,etna --with-egl-platforms=fbdev \
    --enable-gallium-egl --enable-debug --with-dri-drivers=
```

- Relies on `libetnaviv.a` and headers from the `etna_viv` project (https://github.com/laanwj/etna_viv).

Mesa cross compiling
---------------------
- libexpat and libdrm need to be available on the target (neither is used at the moment, but they are 
dependencies for Mesa)

- `src/glsl/builtin_compiler/builtin_compiler` must be built on the host instead of the target hardware.
  Currently, this has to be done manually by conifguring and building Mesa for the host first, backing up the executable,
  and copying it back after `make clean` and configuring for cross compile. 


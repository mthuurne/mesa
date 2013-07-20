Etnaviv Mesa fork
=================

Open source GLES1/2 driver for Vivante GPU hardware.

This is being maintained as a set of patches on top of the Mesa main repository. Expect frequent rebases
while in this phase of development.

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

export TARGET="arm-linux-gnueabihf"
export CFLAGS="-I${DIR}/cubox/include -I${ETNAVIV_INC}"
export CXXFLAGS="-I${DIR}/cubox/include -I${ETNAVIV_INC}"
export LDFLAGS="-L${DIR}/cubox/lib -L${ETNAVIV_LIB}"
export LIBDRM_LIBS="-L${DIR}/cubox/lib -ldrm"
export ETNA_LIBS="-letnaviv"
./configure --target=${TARGET} --host=${TARGET} \
    --enable-gles2 --enable-gles1 --disable-glx --enable-egl --enable-dri \
    --with-gallium-drivers=swrast,etna --with-egl-platforms=fbdev \
    --enable-gallium-egl --enable-debug --with-dri-drivers=
```

- The etna gallium driver uses `libetnaviv.a` and its headers from the 
  `etna_viv` project (https://github.com/laanwj/etna_viv) for low-level access and register descriptions.

Mesa cross compiling
---------------------
- libexpat and libdrm need to be available on the target (neither is used at the moment, but they are 
dependencies for Mesa).
In many cases these can be copied from the device, after installing the appropriate development package.

- `src/glsl/builtin_compiler/builtin_compiler` must be built on the host instead of the target hardware,
  otherwise you'll get an error:

    ./.libs/libglslcore.a: could not read symbols: Archive has no index; run ranlib to add one

  This has to be done manually by configuring and building Mesa for the host first, backing up the executable
  `src/glsl/builtin_compiler/builtin_compiler`, and copying it back after `make clean` and configuring for cross compile. 

    ./configure
    make
    cp src/glsl/builtin_compiler/builtin_compiler src/glsl/builtin_compiler/builtin_compiler.x86
    make clean
    ./configure --target=... (see above)
    cp src/glsl/builtin_compiler/builtin_compiler.x86 src/glsl/builtin_compiler/builtin_compiler

  You don't need to re-generate `builtin_compiler` unless there are changes in src/glsl.

Testing
====================

This section lists some tests and demos that can be used for exercising the driver.

Mesatest
-------------
A few testcases that I made especially for this driver, based on the samples from the OpenGL ES 2.0 programming 
guide (http://www.opengles-book.com/) can be found here:

https://github.com/laanwj/mesatest_gles

Glmark2
--------------
Some of the Glmark2 demos already run with this driver, but this is a work in progress.

Need a special glmark2 with fbdev support, which can be got here:

https://code.launchpad.net/~laanwj/glmark2/fbdev

Fetch:

    bzr branch lp:~laanwj/glmark2/fbdev

Build:

    ./waf configure --with-flavors=fbdev-glesv2 --data-path=${PWD}/data
    ./waf

Run:

    cd build/src
    ./glmark2-es2 -b shading -s 1280x1024 --visual-config alpha=0  

Support matrix (cubox, v2, gc600):

    [Scene] build    -> renders
    [Scene] shading  -> renders w/ gouraud, with phong it misses POW
    [Scene] texture  -> renders
    [Scene] effect2d -> renders

    Corrupted:
    [Scene] shadow    -> rendering corrupted about every 1/2 frames

    Shader assertion:
    [Scene] refract  -> missing instruction POW
    [Scene] bump     -> missing instruction POW
    [Scene] jellyfish -> missing instruction LRP
    [Scene] ideas    -> missing instruction POW
    [Scene] loop      -> missing loops support
    [Scene] buffer   -> missing instruction LRP
    [Scene] terrain   -> missing instruction LRP

    Shows nothing:
    [Scene] clear    -> shows nothing (is it supposed to?)
    [Scene] conditionals -> shows nothing
    [Scene] function -> shows nothing
    [Scene] pulsar    -> shows nothing

    Crashes GPU:
    [Scene] desktop  -> crashes GPU


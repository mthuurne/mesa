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

export ETNA_LIBS="-letnaviv" # important!
export LIBTOOL_FOR_BUILD="/usr/bin/libtool" # important!

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

Setup
===================

I use this script to set up the framebuffer console for (double or single buffered) rendering,
as well as prevent blanking and avoid screen corruption by hiding the cursor.

    #!/bin/bash
    # Set to usable resolution (double buffered)
    fbset 1280x1024-60 -vyres 2048 -depth 32
    # Set to usable resolution (single buffered)
    #fbset 1280x1024-60 -vyres 1024 -depth 32

    # Disable automatic blanking
    echo -e '\033[9;0]' > /dev/tty1
    echo 0 > /sys/class/graphics/fb0/blank

    # Disable blinking cursor
    echo -e '\033[?17;0;0c' > /dev/tty1

Switching between Etna en Swrast
--------------------------------
Frequently it is useful to compare the rendering from etna to the software rasterizer;
this can be done with the environment variable `EGL_FBDEV_DRIVER`, i.e.

    # Run with etna driver
    export EGL_FBDEV_DRIVER=etna
    (run EGL demo...)

    # Run with software rasterizer
    export EGL_FBDEV_DRIVER=swrast
    (run EGL demo...)

Testing
====================

This section lists some tests and demos that can be used to exercise the driver.

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
    [Scene] shading  -> renders
    [Scene] texture  -> renders
    [Scene] effect2d -> renders
    [Scene] bump     -> renders

    Corrupted:
    [Scene] shadow    -> rendering corrupted about every 1/2 frames

    Shader assertion:
    [Scene] refract  -> missing instruction CMP
    [Scene] jellyfish -> missing instruction SIN
    [Scene] ideas    -> missing instruction KILP
    [Scene] loop      -> missing loops support
    [Scene] buffer   -> missing instruction CMP
    [Scene] terrain   -> missing instruction CMP

    Shows nothing:
    [Scene] clear    -> shows nothing (is it supposed to?)
    [Scene] conditionals -> shows nothing
    [Scene] function -> shows nothing
    [Scene] pulsar    -> shows nothing

    Crashes GPU:
    [Scene] desktop  -> crashes GPU


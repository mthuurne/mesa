#ifndef __ETNA_FBDEV_PRIVATE_H__
#define __ETNA_FBDEV_PRIVATE_H__

#include <etnaviv/etna_rs.h>

struct etna_fbdev_drawable {
    /* Front buffer RS target.
     *
     * XXX this is very similar to pipe_blit_info src/dst, in which 
     * case resource_copy_region/etna_pipe_blit could be used with
     * a linear-tiled pipe_resource for the framebuffer.
     */
    struct etna_rs_target frontbuffer;
};

#endif


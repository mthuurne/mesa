#ifndef __ETNA_FBDEV_PUBLIC_H__
#define __ETNA_FBDEV_PUBLIC_H__

struct etna_fbdev_drawable;
struct fb_var_screeninfo;
struct fb_fix_screeninfo;

/* Create etna fbdev drawable */
struct etna_fbdev_drawable *etna_fbdev_drawable_create(struct pipe_screen *screen, bool want_fence,
          const struct fb_var_screeninfo *vinfo,
          const struct fb_fix_screeninfo *finfo,
          unsigned xoffset, unsigned yoffset,
          unsigned width, unsigned height);

/* Destroy drawable */
void etna_fbdev_drawable_destroy(struct etna_fbdev_drawable *drawable);

/* Get handle for passing to flush_frontbuffer */
void *etna_fbdev_get_buffer(struct etna_fbdev_drawable *drawable);

/* Get fence handle for frame, to know when to swap buffers */
struct pipe_fence_handle *etna_fbdev_get_fence(struct etna_fbdev_drawable *drawable);

#endif

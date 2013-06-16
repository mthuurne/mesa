#ifndef __ETNA_FBDEV_PUBLIC_H__
#define __ETNA_FBDEV_PUBLIC_H__

struct etna_fbdev_drawable;
struct fb_var_screeninfo;
struct fb_fix_screeninfo;

/* Create etna fbdev drawable */
struct etna_fbdev_drawable *etna_fbdev_drawable_create(void);

/* Destroy drawable */
void etna_fbdev_drawable_destroy(struct etna_fbdev_drawable *drawable);

/* Update dimensions and render target if needed */
bool etna_fbdev_drawable_update(struct etna_fbdev_drawable *drawable,
                              const struct fb_var_screeninfo *vinfo,
                              const struct fb_fix_screeninfo *finfo);

/* Get handle for passing to flush_frontbuffer */
void *etna_fbdev_get_front_buffer(struct etna_fbdev_drawable *drawable);

#endif

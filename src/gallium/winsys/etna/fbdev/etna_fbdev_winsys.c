#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include "util/u_format.h"
#include "util/u_memory.h"
#include "util/u_inlines.h"

#include "etna_fbdev_public.h"
#include "etna_fbdev_winsys.h"

#include "etnaviv/viv.h"
#include "etna/etna_screen.h"
#include "etna/etna_fb.h"

#include <stdio.h>
#include <linux/fb.h>

struct etna_fbdev_drawable *etna_fbdev_drawable_create(void)
{
    struct etna_fbdev_drawable *drawable = CALLOC_STRUCT(etna_fbdev_drawable);
    return drawable;
}

/* Destroy drawable */
void etna_fbdev_drawable_destroy(struct etna_fbdev_drawable *drawable)
{
    FREE(drawable);
}

/* Update dimensions and render target if needed */
bool etna_fbdev_drawable_update(struct etna_fbdev_drawable *drawable,
                              const struct fb_var_screeninfo *vinfo,
                              const struct fb_fix_screeninfo *finfo)
{
   unsigned x, y, width, height;

   x = vinfo->xoffset;
   y = vinfo->yoffset;
   width = vinfo->xres;
   height = vinfo->yres;

   /* sanitize the values */
   if (x + width > vinfo->xres_virtual) {
      if (x > vinfo->xres_virtual)
         width = 0;
      else
         width = vinfo->xres_virtual - x;
   }
   if (y + height > vinfo->yres_virtual) {
      if (y > vinfo->yres_virtual)
         height = 0;
      else
         height = vinfo->yres_virtual - y;
   }

   drawable->frontbuffer.width = vinfo->xres;
   drawable->frontbuffer.height = vinfo->yres;
   drawable->frontbuffer.addr = finfo->smem_start +
       finfo->line_length * vinfo->yoffset +
       vinfo->bits_per_pixel / 8 * vinfo->xoffset;
   drawable->frontbuffer.stride = finfo->line_length;
   if(!etna_fb_get_format(vinfo, &drawable->frontbuffer.rs_format, &drawable->frontbuffer.swap_rb))
   {
       return false;
   }
   return (drawable->frontbuffer.width &&
           drawable->frontbuffer.height);
}

/* Get handle for passing to flush_frontbuffer */
void *etna_fbdev_get_front_buffer(struct etna_fbdev_drawable *drawable)
{
    if(drawable == NULL || !drawable->frontbuffer.width || !drawable->frontbuffer.height)
        return NULL;
    return &drawable->frontbuffer;
}


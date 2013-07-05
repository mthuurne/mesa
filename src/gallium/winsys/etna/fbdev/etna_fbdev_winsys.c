#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include "util/u_format.h"
#include "util/u_memory.h"
#include "util/u_inlines.h"

#include "etna_fbdev_public.h"
#include "etna_fbdev_winsys.h"

#include "etna/etna_screen.h"

#include <etnaviv/viv.h>
#include <etnaviv/etna_fb.h>

#include <stdio.h>
#include <linux/fb.h>

struct etna_fbdev_drawable *etna_fbdev_drawable_create(struct pipe_screen *screen, bool want_fence)
{
    struct etna_fbdev_drawable *drawable = CALLOC_STRUCT(etna_fbdev_drawable);
    drawable->screen = screen;
    drawable->buffer.want_fence = want_fence;
    return drawable;
}

/* Destroy drawable */
void etna_fbdev_drawable_destroy(struct etna_fbdev_drawable *drawable)
{
   /* release fence */
   drawable->screen->fence_reference(drawable->screen, &drawable->buffer.fence, NULL);
   FREE(drawable);
}

/* Update dimensions and render target if needed */
bool etna_fbdev_drawable_update(struct etna_fbdev_drawable *drawable,
                              const struct fb_var_screeninfo *vinfo,
                              const struct fb_fix_screeninfo *finfo,
                              unsigned xoffset, unsigned yoffset,
                              unsigned width, unsigned height)
{
   /* sanitize the values */
   if (xoffset + width > vinfo->xres_virtual) {
      if (xoffset > vinfo->xres_virtual)
         width = 0;
      else
         width = vinfo->xres_virtual - xoffset;
   }
   if (yoffset + height > vinfo->yres_virtual) {
      if (yoffset > vinfo->yres_virtual)
         height = 0;
      else
         height = vinfo->yres_virtual - yoffset;
   }

   drawable->buffer.width = vinfo->xres;
   drawable->buffer.height = vinfo->yres;
   drawable->buffer.addr = finfo->smem_start +
      finfo->line_length * yoffset +
      vinfo->bits_per_pixel / 8 * xoffset;
   drawable->buffer.stride = finfo->line_length;

   if(!etna_fb_get_format(vinfo, &drawable->buffer.rs_format, &drawable->buffer.swap_rb))
   {
      return false;
   }
   return width && height;
}

/* Get handle for passing to flush_frontbuffer */
void *etna_fbdev_get_buffer(struct etna_fbdev_drawable *drawable)
{
    if(drawable == NULL || !drawable->buffer.width || !drawable->buffer.height)
        return NULL;
    return &drawable->buffer;
}

struct pipe_fence_handle *etna_fbdev_get_fence(struct etna_fbdev_drawable *drawable)
{
    if(drawable == NULL)
        return NULL;
    return drawable->buffer.fence;
}


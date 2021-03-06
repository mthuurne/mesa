/*
 * Mesa 3-D graphics library
 *
 * Copyright (C) 2013 LunarG, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Chia-I Wu <olv@lunarg.com>
 */

#include "brw_defines.h"
#include "intel_reg.h"

#include "ilo_cp.h"
#include "ilo_format.h"
#include "ilo_resource.h"
#include "ilo_shader.h"
#include "ilo_gpe_gen7.h"

static void
gen7_emit_GPGPU_WALKER(const struct ilo_gpe *gpe,
                       struct ilo_cp *cp)
{
   assert(!"GPGPU_WALKER unsupported");
}

static void
gen7_emit_3DSTATE_CLEAR_PARAMS(const struct ilo_gpe *gpe,
                               uint32_t clear_val,
                               struct ilo_cp *cp)
{
   const uint32_t cmd = ILO_GPE_CMD(0x3, 0x0, 0x04);
   const uint8_t cmd_len = 3;

   ILO_GPE_VALID_GEN(gpe, 7, 7);

   ilo_cp_begin(cp, cmd_len);
   ilo_cp_write(cp, cmd | (cmd_len - 2));
   ilo_cp_write(cp, clear_val);
   ilo_cp_write(cp, 1);
   ilo_cp_end(cp);
}

static void
gen7_emit_3DSTATE_DEPTH_BUFFER(const struct ilo_gpe *gpe,
                               const struct pipe_surface *surface,
                               const struct pipe_depth_stencil_alpha_state *dsa,
                               bool hiz,
                               struct ilo_cp *cp)
{
   ilo_gpe_gen6_emit_3DSTATE_DEPTH_BUFFER(gpe, surface, dsa, hiz, cp);
}

static void
gen7_emit_3dstate_pointer(const struct ilo_gpe *gpe,
                          int subop, uint32_t pointer,
                          struct ilo_cp *cp)
{
   const uint32_t cmd = ILO_GPE_CMD(0x3, 0x0, subop);
   const uint8_t cmd_len = 2;

   ILO_GPE_VALID_GEN(gpe, 7, 7);

   ilo_cp_begin(cp, cmd_len);
   ilo_cp_write(cp, cmd | (cmd_len - 2));
   ilo_cp_write(cp, pointer);
   ilo_cp_end(cp);
}

static void
gen7_emit_3DSTATE_CC_STATE_POINTERS(const struct ilo_gpe *gpe,
                                    uint32_t color_calc_state,
                                    struct ilo_cp *cp)
{
   gen7_emit_3dstate_pointer(gpe, 0x0e, color_calc_state, cp);
}

static void
gen7_emit_3DSTATE_GS(const struct ilo_gpe *gpe,
                     const struct ilo_shader *gs,
                     int max_threads, int num_samplers,
                     struct ilo_cp *cp)
{
   const uint32_t cmd = ILO_GPE_CMD(0x3, 0x0, 0x11);
   const uint8_t cmd_len = 7;
   uint32_t dw2, dw4, dw5;

   ILO_GPE_VALID_GEN(gpe, 7, 7);

   if (!gs) {
      ilo_cp_begin(cp, cmd_len);
      ilo_cp_write(cp, cmd | (cmd_len - 2));
      ilo_cp_write(cp, 0);
      ilo_cp_write(cp, 0);
      ilo_cp_write(cp, 0);
      ilo_cp_write(cp, 0);
      ilo_cp_write(cp, GEN6_GS_STATISTICS_ENABLE);
      ilo_cp_write(cp, 0);
      ilo_cp_end(cp);
      return;
   }

   dw2 = ((num_samplers + 3) / 4) << GEN6_GS_SAMPLER_COUNT_SHIFT;

   dw4 = ((gs->in.count + 1) / 2) << GEN6_GS_URB_READ_LENGTH_SHIFT |
         GEN7_GS_INCLUDE_VERTEX_HANDLES |
         0 << GEN6_GS_URB_ENTRY_READ_OFFSET_SHIFT |
         gs->in.start_grf << GEN6_GS_DISPATCH_START_GRF_SHIFT;

   dw5 = (max_threads - 1) << GEN6_GS_MAX_THREADS_SHIFT |
         GEN6_GS_STATISTICS_ENABLE |
         GEN6_GS_ENABLE;

   ilo_cp_begin(cp, cmd_len);
   ilo_cp_write(cp, cmd | (cmd_len - 2));
   ilo_cp_write(cp, gs->cache_offset);
   ilo_cp_write(cp, dw2);
   ilo_cp_write(cp, 0); /* scratch */
   ilo_cp_write(cp, dw4);
   ilo_cp_write(cp, dw5);
   ilo_cp_write(cp, 0);
   ilo_cp_end(cp);
}

static void
gen7_emit_3DSTATE_SF(const struct ilo_gpe *gpe,
                     const struct pipe_rasterizer_state *rasterizer,
                     const struct pipe_surface *zs_surf,
                     struct ilo_cp *cp)
{
   const uint32_t cmd = ILO_GPE_CMD(0x3, 0x0, 0x13);
   const uint8_t cmd_len = 7;
   uint32_t dw[6];

   ILO_GPE_VALID_GEN(gpe, 7, 7);

   ilo_gpe_gen6_fill_3dstate_sf_raster(gpe, rasterizer,
         1, (zs_surf) ? zs_surf->format : PIPE_FORMAT_NONE, true,
         dw, Elements(dw));

   ilo_cp_begin(cp, cmd_len);
   ilo_cp_write(cp, cmd | (cmd_len - 2));
   ilo_cp_write_multi(cp, dw, 6);
   ilo_cp_end(cp);
}

static void
gen7_emit_3DSTATE_WM(const struct ilo_gpe *gpe,
                     const struct ilo_shader *fs,
                     const struct pipe_rasterizer_state *rasterizer,
                     bool cc_may_kill,
                     struct ilo_cp *cp)
{
   const uint32_t cmd = ILO_GPE_CMD(0x3, 0x0, 0x14);
   const uint8_t cmd_len = 3;
   const int num_samples = 1;
   uint32_t dw1, dw2;

   ILO_GPE_VALID_GEN(gpe, 7, 7);

   dw1 = GEN7_WM_STATISTICS_ENABLE |
         GEN7_WM_LINE_AA_WIDTH_2_0;

   if (false) {
      dw1 |= GEN7_WM_DEPTH_CLEAR;
      dw1 |= GEN7_WM_DEPTH_RESOLVE;
      dw1 |= GEN7_WM_HIERARCHICAL_DEPTH_RESOLVE;
   }

   if (fs) {
      /*
       * Set this bit if
       *
       *  a) fs writes colors and color is not masked, or
       *  b) fs writes depth, or
       *  c) fs or cc kills
       */
      dw1 |= GEN7_WM_DISPATCH_ENABLE;

      /*
       * From the Ivy Bridge PRM, volume 2 part 1, page 278:
       *
       *     "This bit (Pixel Shader Kill Pixel), if ENABLED, indicates that
       *      the PS kernel or color calculator has the ability to kill
       *      (discard) pixels or samples, other than due to depth or stencil
       *      testing. This bit is required to be ENABLED in the following
       *      situations:
       *
       *      - The API pixel shader program contains "killpix" or "discard"
       *        instructions, or other code in the pixel shader kernel that
       *        can cause the final pixel mask to differ from the pixel mask
       *        received on dispatch.
       *
       *      - A sampler with chroma key enabled with kill pixel mode is used
       *        by the pixel shader.
       *
       *      - Any render target has Alpha Test Enable or AlphaToCoverage
       *        Enable enabled.
       *
       *      - The pixel shader kernel generates and outputs oMask.
       *
       *      Note: As ClipDistance clipping is fully supported in hardware
       *      and therefore not via PS instructions, there should be no need
       *      to ENABLE this bit due to ClipDistance clipping."
       */
      if (fs->has_kill || cc_may_kill)
         dw1 |= GEN7_WM_KILL_ENABLE;

      if (fs->out.has_pos)
         dw1 |= GEN7_WM_PSCDEPTH_ON;
      if (fs->in.has_pos)
         dw1 |= GEN7_WM_USES_SOURCE_DEPTH | GEN7_WM_USES_SOURCE_W;

      dw1 |= fs->in.barycentric_interpolation_mode <<
         GEN7_WM_BARYCENTRIC_INTERPOLATION_MODE_SHIFT;
   }
   else if (cc_may_kill) {
         dw1 |= GEN7_WM_DISPATCH_ENABLE |
                GEN7_WM_KILL_ENABLE;
   }

   dw1 |= GEN7_WM_POSITION_ZW_PIXEL;

   /* same value as in 3DSTATE_SF */
   if (rasterizer->line_smooth)
      dw1 |= GEN7_WM_LINE_END_CAP_AA_WIDTH_1_0;

   if (rasterizer->poly_stipple_enable)
      dw1 |= GEN7_WM_POLYGON_STIPPLE_ENABLE;
   if (rasterizer->line_stipple_enable)
      dw1 |= GEN7_WM_LINE_STIPPLE_ENABLE;

   if (rasterizer->bottom_edge_rule)
      dw1 |= GEN7_WM_POINT_RASTRULE_UPPER_RIGHT;

   if (num_samples > 1) {
      if (rasterizer->multisample)
         dw1 |= GEN7_WM_MSRAST_ON_PATTERN;
      else
         dw1 |= GEN7_WM_MSRAST_OFF_PIXEL;

      dw2 = GEN7_WM_MSDISPMODE_PERPIXEL;
   }
   else {
      dw1 |= GEN7_WM_MSRAST_OFF_PIXEL;

      dw2 = GEN7_WM_MSDISPMODE_PERSAMPLE;
   }

   ilo_cp_begin(cp, cmd_len);
   ilo_cp_write(cp, cmd | (cmd_len - 2));
   ilo_cp_write(cp, dw1);
   ilo_cp_write(cp, dw2);
   ilo_cp_end(cp);
}

static void
gen7_emit_3dstate_constant(const struct ilo_gpe *gpe,
                           int subop,
                           const uint32_t *bufs, const int *sizes,
                           int num_bufs,
                           struct ilo_cp *cp)
{
   const uint32_t cmd = ILO_GPE_CMD(0x3, 0x0, subop);
   const uint8_t cmd_len = 7;
   uint32_t dw[6];
   int total_read_length, i;

   ILO_GPE_VALID_GEN(gpe, 7, 7);

   /* VS, HS, DS, GS, and PS variants */
   assert(subop >= 0x15 && subop <= 0x1a && subop != 0x18);

   assert(num_bufs <= 4);

   dw[0] = 0;
   dw[1] = 0;

   total_read_length = 0;
   for (i = 0; i < 4; i++) {
      int read_len;

      /*
       * From the Ivy Bridge PRM, volume 2 part 1, page 112:
       *
       *     "Constant buffers must be enabled in order from Constant Buffer 0
       *      to Constant Buffer 3 within this command.  For example, it is
       *      not allowed to enable Constant Buffer 1 by programming a
       *      non-zero value in the VS Constant Buffer 1 Read Length without a
       *      non-zero value in VS Constant Buffer 0 Read Length."
       */
      if (i >= num_bufs || !sizes[i]) {
         for (; i < 4; i++) {
            assert(i >= num_bufs || !sizes[i]);
            dw[2 + i] = 0;
         }
         break;
      }

      /* read lengths are in 256-bit units */
      read_len = (sizes[i] + 31) / 32;
      /* the lower 5 bits are used for memory object control state */
      assert(bufs[i] % 32 == 0);

      dw[i / 2] |= read_len << ((i % 2) ? 16 : 0);
      dw[2 + i] = bufs[i];

      total_read_length += read_len;
   }

   /*
    * From the Ivy Bridge PRM, volume 2 part 1, page 113:
    *
    *     "The sum of all four read length fields must be less than or equal
    *      to the size of 64"
    */
   assert(total_read_length <= 64);

   ilo_cp_begin(cp, cmd_len);
   ilo_cp_write(cp, cmd | (cmd_len - 2));
   ilo_cp_write_multi(cp, dw, 6);
   ilo_cp_end(cp);
}

static void
gen7_emit_3DSTATE_CONSTANT_VS(const struct ilo_gpe *gpe,
                              const uint32_t *bufs, const int *sizes,
                              int num_bufs,
                              struct ilo_cp *cp)
{
   gen7_emit_3dstate_constant(gpe, 0x15, bufs, sizes, num_bufs, cp);
}

static void
gen7_emit_3DSTATE_CONSTANT_GS(const struct ilo_gpe *gpe,
                              const uint32_t *bufs, const int *sizes,
                              int num_bufs,
                              struct ilo_cp *cp)
{
   gen7_emit_3dstate_constant(gpe, 0x16, bufs, sizes, num_bufs, cp);
}

static void
gen7_emit_3DSTATE_CONSTANT_PS(const struct ilo_gpe *gpe,
                              const uint32_t *bufs, const int *sizes,
                              int num_bufs,
                              struct ilo_cp *cp)
{
   gen7_emit_3dstate_constant(gpe, 0x17, bufs, sizes, num_bufs, cp);
}

static void
gen7_emit_3DSTATE_SAMPLE_MASK(const struct ilo_gpe *gpe,
                              unsigned sample_mask,
                              int num_samples,
                              struct ilo_cp *cp)
{
   const uint32_t cmd = ILO_GPE_CMD(0x3, 0x0, 0x18);
   const uint8_t cmd_len = 2;
   const unsigned valid_mask = ((1 << num_samples) - 1) | 0x1;

   ILO_GPE_VALID_GEN(gpe, 7, 7);

   /*
    * From the Ivy Bridge PRM, volume 2 part 1, page 294:
    *
    *     "If Number of Multisamples is NUMSAMPLES_1, bits 7:1 of this field
    *      (Sample Mask) must be zero.
    *
    *      If Number of Multisamples is NUMSAMPLES_4, bits 7:4 of this field
    *      must be zero."
    */
   sample_mask &= valid_mask;

   ilo_cp_begin(cp, cmd_len);
   ilo_cp_write(cp, cmd | (cmd_len - 2));
   ilo_cp_write(cp, sample_mask);
   ilo_cp_end(cp);
}

static void
gen7_emit_3DSTATE_CONSTANT_HS(const struct ilo_gpe *gpe,
                              const uint32_t *bufs, const int *sizes,
                              int num_bufs,
                              struct ilo_cp *cp)
{
   gen7_emit_3dstate_constant(gpe, 0x19, bufs, sizes, num_bufs, cp);
}

static void
gen7_emit_3DSTATE_CONSTANT_DS(const struct ilo_gpe *gpe,
                              const uint32_t *bufs, const int *sizes,
                              int num_bufs,
                              struct ilo_cp *cp)
{
   gen7_emit_3dstate_constant(gpe, 0x1a, bufs, sizes, num_bufs, cp);
}

static void
gen7_emit_3DSTATE_HS(const struct ilo_gpe *gpe,
                     const struct ilo_shader *hs,
                     int max_threads, int num_samplers,
                     struct ilo_cp *cp)
{
   const uint32_t cmd = ILO_GPE_CMD(0x3, 0x0, 0x1b);
   const uint8_t cmd_len = 7;
   uint32_t dw1, dw2, dw5;

   ILO_GPE_VALID_GEN(gpe, 7, 7);

   if (!hs) {
      ilo_cp_begin(cp, cmd_len);
      ilo_cp_write(cp, cmd | (cmd_len - 2));
      ilo_cp_write(cp, 0);
      ilo_cp_write(cp, 0);
      ilo_cp_write(cp, 0);
      ilo_cp_write(cp, 0);
      ilo_cp_write(cp, 0);
      ilo_cp_write(cp, 0);
      ilo_cp_end(cp);

      return;
   }

   dw1 = (num_samplers + 3) / 4 << 27 |
         0 << 18 |
         (max_threads - 1);
   if (false)
      dw1 |= 1 << 16;

   dw2 = 1 << 31 | /* HS Enable */
         1 << 29 | /* HS Statistics Enable */
         0; /* Instance Count */

   dw5 = hs->in.start_grf << 19 |
         0 << 11 |
         0 << 4;

   ilo_cp_begin(cp, cmd_len);
   ilo_cp_write(cp, cmd | (cmd_len - 2));
   ilo_cp_write(cp, dw1);
   ilo_cp_write(cp, dw2);
   ilo_cp_write(cp, hs->cache_offset);
   ilo_cp_write(cp, 0);
   ilo_cp_write(cp, dw5);
   ilo_cp_write(cp, 0);
   ilo_cp_end(cp);
}

static void
gen7_emit_3DSTATE_TE(const struct ilo_gpe *gpe,
                     struct ilo_cp *cp)
{
   const uint32_t cmd = ILO_GPE_CMD(0x3, 0x0, 0x1c);
   const uint8_t cmd_len = 4;

   ILO_GPE_VALID_GEN(gpe, 7, 7);

   ilo_cp_begin(cp, cmd_len);
   ilo_cp_write(cp, cmd | (cmd_len - 2));
   ilo_cp_write(cp, 0);
   ilo_cp_write(cp, 0);
   ilo_cp_write(cp, 0);
   ilo_cp_end(cp);
}

static void
gen7_emit_3DSTATE_DS(const struct ilo_gpe *gpe,
                     const struct ilo_shader *ds,
                     int max_threads, int num_samplers,
                     struct ilo_cp *cp)
{
   const uint32_t cmd = ILO_GPE_CMD(0x3, 0x0, 0x1d);
   const uint8_t cmd_len = 6;
   uint32_t dw2, dw4, dw5;

   ILO_GPE_VALID_GEN(gpe, 7, 7);

   if (!ds) {
      ilo_cp_begin(cp, cmd_len);
      ilo_cp_write(cp, cmd | (cmd_len - 2));
      ilo_cp_write(cp, 0);
      ilo_cp_write(cp, 0);
      ilo_cp_write(cp, 0);
      ilo_cp_write(cp, 0);
      ilo_cp_write(cp, 0);
      ilo_cp_end(cp);

      return;
   }

   dw2 = (num_samplers + 3) / 4 << 27 |
         0 << 18 |
         (max_threads - 1);
   if (false)
      dw2 |= 1 << 16;

   dw4 = ds->in.start_grf << 20 |
         0 << 11 |
         0 << 4;

   dw5 = (max_threads - 1) << 25 |
         1 << 10 |
         1;

   ilo_cp_begin(cp, cmd_len);
   ilo_cp_write(cp, cmd | (cmd_len - 2));
   ilo_cp_write(cp, ds->cache_offset);
   ilo_cp_write(cp, dw2);
   ilo_cp_write(cp, 0);
   ilo_cp_write(cp, dw4);
   ilo_cp_write(cp, dw5);
   ilo_cp_end(cp);
}

static void
gen7_emit_3DSTATE_STREAMOUT(const struct ilo_gpe *gpe,
                            bool enable,
                            bool rasterizer_discard,
                            bool flatshade_first,
                            struct ilo_cp *cp)
{
   const uint32_t cmd = ILO_GPE_CMD(0x3, 0x0, 0x1e);
   const uint8_t cmd_len = 3;
   uint32_t dw1, dw2;
   int i;

   ILO_GPE_VALID_GEN(gpe, 7, 7);

   if (!enable) {
      ilo_cp_begin(cp, cmd_len);
      ilo_cp_write(cp, cmd | (cmd_len - 2));
      ilo_cp_write(cp, (rasterizer_discard) ? SO_RENDERING_DISABLE : 0);
      ilo_cp_write(cp, 0);
      ilo_cp_end(cp);
      return;
   }

   dw1 = SO_FUNCTION_ENABLE |
         SO_STATISTICS_ENABLE;
   if (rasterizer_discard)
      dw1 |= SO_RENDERING_DISABLE;
   if (!flatshade_first)
      dw1 |= SO_REORDER_TRAILING;
   for (i = 0; i < 4; i++)
      dw1 |= SO_BUFFER_ENABLE(i);

   dw2 = 0 << SO_STREAM_0_VERTEX_READ_OFFSET_SHIFT |
         0 << SO_STREAM_0_VERTEX_READ_LENGTH_SHIFT;

   ilo_cp_begin(cp, cmd_len);
   ilo_cp_write(cp, cmd | (cmd_len - 2));
   ilo_cp_write(cp, dw1);
   ilo_cp_write(cp, dw2);
   ilo_cp_end(cp);
}

static void
gen7_emit_3DSTATE_SBE(const struct ilo_gpe *gpe,
                      const struct pipe_rasterizer_state *rasterizer,
                      const struct ilo_shader *fs,
                      const struct ilo_shader *last_sh,
                      struct ilo_cp *cp)
{
   const uint32_t cmd = ILO_GPE_CMD(0x3, 0x0, 0x1f);
   const uint8_t cmd_len = 14;
   uint32_t dw[13];

   ILO_GPE_VALID_GEN(gpe, 7, 7);

   ilo_gpe_gen6_fill_3dstate_sf_sbe(gpe, rasterizer,
         fs, last_sh, dw, Elements(dw));

   ilo_cp_begin(cp, cmd_len);
   ilo_cp_write(cp, cmd | (cmd_len - 2));
   ilo_cp_write_multi(cp, dw, 13);
   ilo_cp_end(cp);
}

static void
gen7_emit_3DSTATE_PS(const struct ilo_gpe *gpe,
                     const struct ilo_shader *fs,
                     int max_threads, int num_samplers,
                     bool dual_blend,
                     struct ilo_cp *cp)
{
   const uint32_t cmd = ILO_GPE_CMD(0x3, 0x0, 0x20);
   const uint8_t cmd_len = 8;
   uint32_t dw2, dw4, dw5;

   ILO_GPE_VALID_GEN(gpe, 7, 7);

   /*
    * From the Ivy Bridge PRM, volume 2 part 1, page 286:
    *
    *     "This field (Maximum Number of Threads) must have an odd value so
    *      that the max number of PS threads is even."
    */
   max_threads &= ~1;

   /* the valid range is [4, 48] */
   if (max_threads < 4)
      max_threads = 4;

   if (!fs) {
      ilo_cp_begin(cp, cmd_len);
      ilo_cp_write(cp, cmd | (cmd_len - 2));
      ilo_cp_write(cp, 0);
      ilo_cp_write(cp, 0);
      ilo_cp_write(cp, 0);
      /* GPU hangs if none of the dispatch enable bits is set */
      ilo_cp_write(cp, (max_threads - 1) << IVB_PS_MAX_THREADS_SHIFT |
                       GEN7_PS_8_DISPATCH_ENABLE);
      ilo_cp_write(cp, 0);
      ilo_cp_write(cp, 0);
      ilo_cp_write(cp, 0);
      ilo_cp_end(cp);

      return;
   }

   dw2 = (num_samplers + 3) / 4 << GEN7_PS_SAMPLER_COUNT_SHIFT |
         0 << GEN7_PS_BINDING_TABLE_ENTRY_COUNT_SHIFT;
   if (false)
      dw2 |= GEN7_PS_FLOATING_POINT_MODE_ALT;

   dw4 = (max_threads - 1) << IVB_PS_MAX_THREADS_SHIFT |
         GEN7_PS_POSOFFSET_NONE;

   if (false)
      dw4 |= GEN7_PS_PUSH_CONSTANT_ENABLE;
   if (fs->in.count)
      dw4 |= GEN7_PS_ATTRIBUTE_ENABLE;
   if (dual_blend)
      dw4 |= GEN7_PS_DUAL_SOURCE_BLEND_ENABLE;

   if (fs->dispatch_16)
      dw4 |= GEN7_PS_16_DISPATCH_ENABLE;
   else
      dw4 |= GEN7_PS_8_DISPATCH_ENABLE;

   dw5 = fs->in.start_grf << GEN7_PS_DISPATCH_START_GRF_SHIFT_0 |
         0 << GEN7_PS_DISPATCH_START_GRF_SHIFT_1 |
         0 << GEN7_PS_DISPATCH_START_GRF_SHIFT_2;

   ilo_cp_begin(cp, cmd_len);
   ilo_cp_write(cp, cmd | (cmd_len - 2));
   ilo_cp_write(cp, fs->cache_offset);
   ilo_cp_write(cp, dw2);
   ilo_cp_write(cp, 0); /* scratch */
   ilo_cp_write(cp, dw4);
   ilo_cp_write(cp, dw5);
   ilo_cp_write(cp, 0); /* kernel 1 */
   ilo_cp_write(cp, 0); /* kernel 2 */
   ilo_cp_end(cp);
}

static void
gen7_emit_3DSTATE_VIEWPORT_STATE_POINTERS_SF_CLIP(const struct ilo_gpe *gpe,
                                                  uint32_t sf_clip_viewport,
                                                  struct ilo_cp *cp)
{
   gen7_emit_3dstate_pointer(gpe, 0x21, sf_clip_viewport, cp);
}

static void
gen7_emit_3DSTATE_VIEWPORT_STATE_POINTERS_CC(const struct ilo_gpe *gpe,
                                             uint32_t cc_viewport,
                                             struct ilo_cp *cp)
{
   gen7_emit_3dstate_pointer(gpe, 0x23, cc_viewport, cp);
}

static void
gen7_emit_3DSTATE_BLEND_STATE_POINTERS(const struct ilo_gpe *gpe,
                                       uint32_t blend_state,
                                       struct ilo_cp *cp)
{
   gen7_emit_3dstate_pointer(gpe, 0x24, blend_state, cp);
}

static void
gen7_emit_3DSTATE_DEPTH_STENCIL_STATE_POINTERS(const struct ilo_gpe *gpe,
                                               uint32_t depth_stencil_state,
                                               struct ilo_cp *cp)
{
   gen7_emit_3dstate_pointer(gpe, 0x25, depth_stencil_state, cp);
}

static void
gen7_emit_3DSTATE_BINDING_TABLE_POINTERS_VS(const struct ilo_gpe *gpe,
                                            uint32_t binding_table,
                                            struct ilo_cp *cp)
{
   gen7_emit_3dstate_pointer(gpe, 0x26, binding_table, cp);
}

static void
gen7_emit_3DSTATE_BINDING_TABLE_POINTERS_HS(const struct ilo_gpe *gpe,
                                            uint32_t binding_table,
                                            struct ilo_cp *cp)
{
   gen7_emit_3dstate_pointer(gpe, 0x27, binding_table, cp);
}

static void
gen7_emit_3DSTATE_BINDING_TABLE_POINTERS_DS(const struct ilo_gpe *gpe,
                                            uint32_t binding_table,
                                            struct ilo_cp *cp)
{
   gen7_emit_3dstate_pointer(gpe, 0x28, binding_table, cp);
}

static void
gen7_emit_3DSTATE_BINDING_TABLE_POINTERS_GS(const struct ilo_gpe *gpe,
                                            uint32_t binding_table,
                                            struct ilo_cp *cp)
{
   gen7_emit_3dstate_pointer(gpe, 0x29, binding_table, cp);
}

static void
gen7_emit_3DSTATE_BINDING_TABLE_POINTERS_PS(const struct ilo_gpe *gpe,
                                            uint32_t binding_table,
                                            struct ilo_cp *cp)
{
   gen7_emit_3dstate_pointer(gpe, 0x2a, binding_table, cp);
}

static void
gen7_emit_3DSTATE_SAMPLER_STATE_POINTERS_VS(const struct ilo_gpe *gpe,
                                            uint32_t sampler_state,
                                            struct ilo_cp *cp)
{
   gen7_emit_3dstate_pointer(gpe, 0x2b, sampler_state, cp);
}

static void
gen7_emit_3DSTATE_SAMPLER_STATE_POINTERS_HS(const struct ilo_gpe *gpe,
                                            uint32_t sampler_state,
                                            struct ilo_cp *cp)
{
   gen7_emit_3dstate_pointer(gpe, 0x2c, sampler_state, cp);
}

static void
gen7_emit_3DSTATE_SAMPLER_STATE_POINTERS_DS(const struct ilo_gpe *gpe,
                                            uint32_t sampler_state,
                                            struct ilo_cp *cp)
{
   gen7_emit_3dstate_pointer(gpe, 0x2d, sampler_state, cp);
}

static void
gen7_emit_3DSTATE_SAMPLER_STATE_POINTERS_GS(const struct ilo_gpe *gpe,
                                            uint32_t sampler_state,
                                            struct ilo_cp *cp)
{
   gen7_emit_3dstate_pointer(gpe, 0x2e, sampler_state, cp);
}

static void
gen7_emit_3DSTATE_SAMPLER_STATE_POINTERS_PS(const struct ilo_gpe *gpe,
                                            uint32_t sampler_state,
                                            struct ilo_cp *cp)
{
   gen7_emit_3dstate_pointer(gpe, 0x2f, sampler_state, cp);
}

static void
gen7_emit_3dstate_urb(const struct ilo_gpe *gpe,
                      int subop, int offset, int size,
                      int entry_size,
                      struct ilo_cp *cp)
{
   const uint32_t cmd = ILO_GPE_CMD(0x3, 0x0, subop);
   const uint8_t cmd_len = 2;
   const int row_size = 64; /* 512 bits */
   int alloc_size, num_entries;

   ILO_GPE_VALID_GEN(gpe, 7, 7);

   /* VS, HS, DS, and GS variants */
   assert(subop >= 0x30 && subop <= 0x33);

   /* in multiples of 8KB */
   assert(offset % 8192 == 0);
   offset /= 8192;

   /* in multiple of 512-bit rows */
   alloc_size = (entry_size + row_size - 1) / row_size;
   if (!alloc_size)
      alloc_size = 1;

   /*
    * From the Ivy Bridge PRM, volume 2 part 1, page 34:
    *
    *     "VS URB Entry Allocation Size equal to 4(5 512-bit URB rows) may
    *      cause performance to decrease due to banking in the URB. Element
    *      sizes of 16 to 20 should be programmed with six 512-bit URB rows."
    */
   if (subop == 0x30 && alloc_size == 5)
      alloc_size = 6;

   /* in multiples of 8 */
   num_entries = (size / row_size / alloc_size) & ~7;

   switch (subop) {
   case 0x30: /* 3DSTATE_URB_VS */
      assert(num_entries >= 32);
      if (gpe->gt == 2 && num_entries > 704)
         num_entries = 704;
      else if (gpe->gt == 1 && num_entries > 512)
         num_entries = 512;
      break;
   case 0x32: /* 3DSTATE_URB_DS */
      if (num_entries)
         assert(num_entries >= 138);
      break;
   default:
      break;
   }

   ilo_cp_begin(cp, cmd_len);
   ilo_cp_write(cp, cmd | (cmd_len - 2));
   ilo_cp_write(cp, offset << GEN7_URB_STARTING_ADDRESS_SHIFT |
                    (alloc_size - 1) << GEN7_URB_ENTRY_SIZE_SHIFT |
                    num_entries);
   ilo_cp_end(cp);
}

static void
gen7_emit_3DSTATE_URB_VS(const struct ilo_gpe *gpe,
                         int offset, int size, int entry_size,
                         struct ilo_cp *cp)
{
   gen7_emit_3dstate_urb(gpe, 0x30, offset, size, entry_size, cp);
}

static void
gen7_emit_3DSTATE_URB_HS(const struct ilo_gpe *gpe,
                         int offset, int size, int entry_size,
                         struct ilo_cp *cp)
{
   gen7_emit_3dstate_urb(gpe, 0x31, offset, size, entry_size, cp);
}

static void
gen7_emit_3DSTATE_URB_DS(const struct ilo_gpe *gpe,
                         int offset, int size, int entry_size,
                         struct ilo_cp *cp)
{
   gen7_emit_3dstate_urb(gpe, 0x32, offset, size, entry_size, cp);
}

static void
gen7_emit_3DSTATE_URB_GS(const struct ilo_gpe *gpe,
                         int offset, int size, int entry_size,
                         struct ilo_cp *cp)
{
   gen7_emit_3dstate_urb(gpe, 0x33, offset, size, entry_size, cp);
}

static void
gen7_emit_3dstate_push_constant_alloc(const struct ilo_gpe *gpe,
                                      int subop, int offset, int size,
                                      struct ilo_cp *cp)
{
   const uint32_t cmd = ILO_GPE_CMD(0x3, 0x1, subop);
   const uint8_t cmd_len = 2;
   int end;

   ILO_GPE_VALID_GEN(gpe, 7, 7);

   /* VS, HS, DS, GS, and PS variants */
   assert(subop >= 0x12 && subop <= 0x16);

   /*
    * From the Ivy Bridge PRM, volume 2 part 1, page 68:
    *
    *     "(A table that says the maximum size of each constant buffer is
    *      16KB")
    *
    * From the Ivy Bridge PRM, volume 2 part 1, page 115:
    *
    *     "The sum of the Constant Buffer Offset and the Constant Buffer Size
    *      may not exceed the maximum value of the Constant Buffer Size."
    *
    * Thus, the valid range of buffer end is [0KB, 16KB].
    */
   end = (offset + size) / 1024;
   if (end > 16) {
      assert(!"invalid constant buffer end");
      end = 16;
   }

   /* the valid range of buffer offset is [0KB, 15KB] */
   offset = (offset + 1023) / 1024;
   if (offset > 15) {
      assert(!"invalid constant buffer offset");
      offset = 15;
   }

   if (offset > end) {
      assert(!size);
      offset = end;
   }

   /* the valid range of buffer size is [0KB, 15KB] */
   size = end - offset;
   if (size > 15) {
      assert(!"invalid constant buffer size");
      size = 15;
   }

   ilo_cp_begin(cp, cmd_len);
   ilo_cp_write(cp, cmd | (cmd_len - 2));
   ilo_cp_write(cp, offset << GEN7_PUSH_CONSTANT_BUFFER_OFFSET_SHIFT |
                    size);
   ilo_cp_end(cp);
}

static void
gen7_emit_3DSTATE_PUSH_CONSTANT_ALLOC_VS(const struct ilo_gpe *gpe,
                                         int offset, int size,
                                         struct ilo_cp *cp)
{
   gen7_emit_3dstate_push_constant_alloc(gpe, 0x12, offset, size, cp);
}

static void
gen7_emit_3DSTATE_PUSH_CONSTANT_ALLOC_HS(const struct ilo_gpe *gpe,
                                         int offset, int size,
                                         struct ilo_cp *cp)
{
   gen7_emit_3dstate_push_constant_alloc(gpe, 0x13, offset, size, cp);
}

static void
gen7_emit_3DSTATE_PUSH_CONSTANT_ALLOC_DS(const struct ilo_gpe *gpe,
                                         int offset, int size,
                                         struct ilo_cp *cp)
{
   gen7_emit_3dstate_push_constant_alloc(gpe, 0x14, offset, size, cp);
}

static void
gen7_emit_3DSTATE_PUSH_CONSTANT_ALLOC_GS(const struct ilo_gpe *gpe,
                                         int offset, int size,
                                         struct ilo_cp *cp)
{
   gen7_emit_3dstate_push_constant_alloc(gpe, 0x15, offset, size, cp);
}

static void
gen7_emit_3DSTATE_PUSH_CONSTANT_ALLOC_PS(const struct ilo_gpe *gpe,
                                         int offset, int size,
                                         struct ilo_cp *cp)
{
   gen7_emit_3dstate_push_constant_alloc(gpe, 0x16, offset, size, cp);
}

static void
gen7_emit_3DSTATE_SO_DECL_LIST(const struct ilo_gpe *gpe,
                               struct ilo_cp *cp)
{
   const uint32_t cmd = ILO_GPE_CMD(0x3, 0x1, 0x17);
   uint8_t cmd_len;
   uint16_t decls[128];
   int num_decls, i;

   ILO_GPE_VALID_GEN(gpe, 7, 7);

   memset(decls, 0, sizeof(decls));
   num_decls = 0;

   cmd_len = 2 * num_decls + 3;

   ilo_cp_begin(cp, cmd_len);
   ilo_cp_write(cp, cmd | (cmd_len - 2));
   ilo_cp_write(cp, 0 << SO_STREAM_TO_BUFFER_SELECTS_0_SHIFT |
                    0 << SO_STREAM_TO_BUFFER_SELECTS_1_SHIFT |
                    0 << SO_STREAM_TO_BUFFER_SELECTS_2_SHIFT |
                    0 << SO_STREAM_TO_BUFFER_SELECTS_3_SHIFT);
   ilo_cp_write(cp, num_decls << SO_NUM_ENTRIES_0_SHIFT |
                    0 << SO_NUM_ENTRIES_1_SHIFT |
                    0 << SO_NUM_ENTRIES_2_SHIFT |
                    0 << SO_NUM_ENTRIES_3_SHIFT);

   for (i = 0; i < num_decls; i++) {
      ilo_cp_write(cp, decls[i]);
      ilo_cp_write(cp, 0);
   }

   ilo_cp_end(cp);
}

static void
gen7_emit_3DSTATE_SO_BUFFER(const struct ilo_gpe *gpe,
                            int index,
                            bool enable,
                            struct ilo_cp *cp)
{
   const uint32_t cmd = ILO_GPE_CMD(0x3, 0x1, 0x18);
   const uint8_t cmd_len = 4;
   int start, end;

   ILO_GPE_VALID_GEN(gpe, 7, 7);

   if (!enable) {
      ilo_cp_begin(cp, cmd_len);
      ilo_cp_write(cp, cmd | (cmd_len - 2));
      ilo_cp_write(cp, index << SO_BUFFER_INDEX_SHIFT);
      ilo_cp_write(cp, 0);
      ilo_cp_write(cp, 0);
      ilo_cp_end(cp);
      return;
   }

   start = end = 0;

   ilo_cp_begin(cp, cmd_len);
   ilo_cp_write(cp, cmd | (cmd_len - 2));
   ilo_cp_write(cp, index << SO_BUFFER_INDEX_SHIFT);
   ilo_cp_write(cp, start);
   ilo_cp_write(cp, end);
   ilo_cp_end(cp);
}

static void
gen7_emit_3DPRIMITIVE(const struct ilo_gpe *gpe,
                      const struct pipe_draw_info *info,
                      bool rectlist,
                      struct ilo_cp *cp)
{
   const uint32_t cmd = ILO_GPE_CMD(0x3, 0x3, 0x00);
   const uint8_t cmd_len = 7;
   const int prim = (rectlist) ?
      _3DPRIM_RECTLIST : ilo_gpe_gen6_translate_pipe_prim(info->mode);
   const int vb_access = (info->indexed) ?
      GEN7_3DPRIM_VERTEXBUFFER_ACCESS_RANDOM :
      GEN7_3DPRIM_VERTEXBUFFER_ACCESS_SEQUENTIAL;

   ILO_GPE_VALID_GEN(gpe, 7, 7);

   ilo_cp_begin(cp, cmd_len);
   ilo_cp_write(cp, cmd | (cmd_len - 2));
   ilo_cp_write(cp, vb_access | prim);
   ilo_cp_write(cp, info->count);
   ilo_cp_write(cp, info->start);
   ilo_cp_write(cp, info->instance_count);
   ilo_cp_write(cp, info->start_instance);
   ilo_cp_write(cp, info->index_bias);
   ilo_cp_end(cp);
}

static uint32_t
gen7_emit_SF_CLIP_VIEWPORT(const struct ilo_gpe *gpe,
                           const struct pipe_viewport_state *viewports,
                           int num_viewports,
                           struct ilo_cp *cp)
{
   const int state_align = 64 / 4;
   const int state_len = 16 * num_viewports;
   uint32_t state_offset, *dw;
   int i;

   ILO_GPE_VALID_GEN(gpe, 7, 7);

   /*
    * From the Ivy Bridge PRM, volume 2 part 1, page 270:
    *
    *     "The viewport-specific state used by both the SF and CL units
    *      (SF_CLIP_VIEWPORT) is stored as an array of up to 16 elements, each
    *      of which contains the DWords described below. The start of each
    *      element is spaced 16 DWords apart. The location of first element of
    *      the array, as specified by both Pointer to SF_VIEWPORT and Pointer
    *      to CLIP_VIEWPORT, is aligned to a 64-byte boundary."
    */
   assert(num_viewports && num_viewports <= 16);

   dw = ilo_cp_steal_ptr(cp, "SF_CLIP_VIEWPORT",
         state_len, state_align, &state_offset);

   for (i = 0; i < num_viewports; i++) {
      const struct pipe_viewport_state *vp = &viewports[i];

      ilo_gpe_gen6_fill_SF_VIEWPORT(gpe, vp, 1, dw, 8);

      ilo_gpe_gen6_fill_CLIP_VIEWPORT(gpe, vp, 1, dw + 8, 4);

      dw[12] = 0;
      dw[13] = 0;
      dw[14] = 0;
      dw[15] = 0;

      dw += 16;
   }

   return state_offset;
}

static void
gen7_fill_null_SURFACE_STATE(const struct ilo_gpe *gpe,
                             unsigned width, unsigned height,
                             unsigned depth, unsigned lod,
                             uint32_t *dw, int num_dwords)
{
   ILO_GPE_VALID_GEN(gpe, 7, 7);
   assert(num_dwords == 8);

   /*
    * From the Ivy Bridge PRM, volume 4 part 1, page 62:
    *
    *     "A null surface is used in instances where an actual surface is not
    *      bound. When a write message is generated to a null surface, no
    *      actual surface is written to. When a read message (including any
    *      sampling engine message) is generated to a null surface, the result
    *      is all zeros.  Note that a null surface type is allowed to be used
    *      with all messages, even if it is not specificially indicated as
    *      supported. All of the remaining fields in surface state are ignored
    *      for null surfaces, with the following exceptions:
    *
    *      * Width, Height, Depth, LOD, and Render Target View Extent fields
    *        must match the depth buffer's corresponding state for all render
    *        target surfaces, including null.
    *      * All sampling engine and data port messages support null surfaces
    *        with the above behavior, even if not mentioned as specifically
    *        supported, except for the following:
    *        * Data Port Media Block Read/Write messages.
    *      * The Surface Type of a surface used as a render target (accessed
    *        via the Data Port's Render Target Write message) must be the same
    *        as the Surface Type of all other render targets and of the depth
    *        buffer (defined in 3DSTATE_DEPTH_BUFFER), unless either the depth
    *        buffer or render targets are SURFTYPE_NULL."
    *
    * From the Ivy Bridge PRM, volume 4 part 1, page 65:
    *
    *     "If Surface Type is SURFTYPE_NULL, this field (Tiled Surface) must be
    *      true"
    */

   dw[0] = BRW_SURFACE_NULL << BRW_SURFACE_TYPE_SHIFT |
           BRW_SURFACEFORMAT_B8G8R8A8_UNORM << BRW_SURFACE_FORMAT_SHIFT |
           BRW_SURFACE_TILED << 13;

   dw[1] = 0;

   dw[2] = SET_FIELD(height - 1, GEN7_SURFACE_HEIGHT) |
           SET_FIELD(width  - 1, GEN7_SURFACE_WIDTH);

   dw[3] = SET_FIELD(depth - 1, BRW_SURFACE_DEPTH);

   dw[4] = 0;
   dw[5] = lod;
   dw[6] = 0;
   dw[7] = 0;
}

static void
gen7_fill_buffer_SURFACE_STATE(const struct ilo_gpe *gpe,
                               const struct ilo_resource *res,
                               unsigned offset, unsigned size,
                               unsigned struct_size,
                               enum pipe_format elem_format,
                               bool is_rt, bool render_cache_rw,
                               uint32_t *dw, int num_dwords)
{
   const bool typed = (elem_format != PIPE_FORMAT_NONE);
   const bool structured = (!typed && struct_size > 1);
   const int elem_size = (typed) ?
      util_format_get_blocksize(elem_format) : 1;
   int width, height, depth, pitch;
   int surface_type, surface_format, num_entries;

   ILO_GPE_VALID_GEN(gpe, 7, 7);
   assert(num_dwords == 8);

   surface_type = (structured) ? 5 : BRW_SURFACE_BUFFER;

   surface_format = (typed) ?
      ilo_translate_color_format(elem_format) : BRW_SURFACEFORMAT_RAW;

   num_entries = size / struct_size;
   /* see if there is enough space to fit another element */
   if (size % struct_size >= elem_size && !structured)
      num_entries++;

   /*
    * From the Ivy Bridge PRM, volume 4 part 1, page 67:
    *
    *     "For SURFTYPE_BUFFER render targets, this field (Surface Base
    *      Address) specifies the base address of first element of the
    *      surface. The surface is interpreted as a simple array of that
    *      single element type. The address must be naturally-aligned to the
    *      element size (e.g., a buffer containing R32G32B32A32_FLOAT elements
    *      must be 16-byte aligned)
    *
    *      For SURFTYPE_BUFFER non-rendertarget surfaces, this field specifies
    *      the base address of the first element of the surface, computed in
    *      software by adding the surface base address to the byte offset of
    *      the element in the buffer."
    */
   if (is_rt)
      assert(offset % elem_size == 0);

   /*
    * From the Ivy Bridge PRM, volume 4 part 1, page 68:
    *
    *     "For typed buffer and structured buffer surfaces, the number of
    *      entries in the buffer ranges from 1 to 2^27.  For raw buffer
    *      surfaces, the number of entries in the buffer is the number of
    *      bytes which can range from 1 to 2^30."
    */
   assert(num_entries >= 1 &&
          num_entries <= 1 << ((typed || structured) ? 27 : 30));

   /*
    * From the Ivy Bridge PRM, volume 4 part 1, page 69:
    *
    *     "For SURFTYPE_BUFFER: The low two bits of this field (Width) must be
    *      11 if the Surface Format is RAW (the size of the buffer must be a
    *      multiple of 4 bytes)."
    *
    * From the Ivy Bridge PRM, volume 4 part 1, page 70:
    *
    *     "For surfaces of type SURFTYPE_BUFFER and SURFTYPE_STRBUF, this
    *      field (Surface Pitch) indicates the size of the structure."
    *
    *     "For linear surfaces with Surface Type of SURFTYPE_STRBUF, the pitch
    *      must be a multiple of 4 bytes."
    */
   if (structured)
      assert(struct_size % 4 == 0);
   else if (!typed)
      assert(num_entries % 4 == 0);

   pitch = struct_size;

   /*
    * From the Ivy Bridge PRM, volume 4 part 1, page 65:
    *
    *     "If Surface Type is SURFTYPE_BUFFER, this field (Tiled Surface) must
    *      be false (because buffers are supported only in linear memory)."
    */
   assert(res->tiling == INTEL_TILING_NONE);

   pitch--;
   num_entries--;
   /* bits [6:0] */
   width  = (num_entries & 0x0000007f);
   /* bits [20:7] */
   height = (num_entries & 0x001fff80) >> 7;
   /* bits [30:21] */
   depth  = (num_entries & 0x7fe00000) >> 21;
   /* limit to [26:21] */
   if (typed || structured)
      depth &= 0x3f;

   dw[0] = surface_type << BRW_SURFACE_TYPE_SHIFT |
           surface_format << BRW_SURFACE_FORMAT_SHIFT;
   if (render_cache_rw)
      dw[0] |= BRW_SURFACE_RC_READ_WRITE;

   dw[1] = offset;

   dw[2] = SET_FIELD(height, GEN7_SURFACE_HEIGHT) |
           SET_FIELD(width, GEN7_SURFACE_WIDTH);

   dw[3] = SET_FIELD(depth, BRW_SURFACE_DEPTH) |
           pitch;

   dw[4] = 0;
   dw[5] = 0;
   dw[6] = 0;
   dw[7] = 0;
}

static void
gen7_fill_normal_SURFACE_STATE(const struct ilo_gpe *gpe,
                               struct ilo_resource *res,
                               enum pipe_format format,
                               unsigned first_level, unsigned num_levels,
                               unsigned first_layer, unsigned num_layers,
                               bool is_rt, bool render_cache_rw,
                               uint32_t *dw, int num_dwords)
{
   int surface_type, surface_format;
   int width, height, depth, pitch, lod;
   unsigned layer_offset, x_offset, y_offset;

   ILO_GPE_VALID_GEN(gpe, 7, 7);
   assert(num_dwords == 8);

   surface_type = ilo_gpe_gen6_translate_texture(res->base.target);
   assert(surface_type != BRW_SURFACE_BUFFER);

   if (is_rt)
      surface_format = ilo_translate_render_format(format);
   else
      surface_format = ilo_translate_texture_format(format);
   assert(surface_format >= 0);

   width = res->base.width0;
   height = res->base.height0;
   pitch = res->bo_stride;

   switch (res->base.target) {
   case PIPE_TEXTURE_3D:
      depth = res->base.depth0;
      break;
   case PIPE_TEXTURE_CUBE:
   case PIPE_TEXTURE_CUBE_ARRAY:
      /*
       * From the Ivy Bridge PRM, volume 4 part 1, page 70:
       *
       *     "For SURFTYPE_CUBE: For Sampling Engine Surfaces, the range of
       *      this field is [0,340], indicating the number of cube array
       *      elements (equal to the number of underlying 2D array elements
       *      divided by 6). For other surfaces, this field must be zero."
       *
       *     "Errata: For SURFTYPE_CUBE sampling engine surfaces, the range of
       *      this field is limited to [0,85]."
       */
      if (!is_rt) {
         assert(num_layers % 6 == 0);
         depth = num_layers / 6;
         break;
      }
      assert(num_layers == 1);
      /* fall through */
   default:
      depth = num_layers;
      break;
   }

   /* sanity check the size */
   assert(width >= 1 && height >= 1 && depth >= 1 && pitch >= 1);
   switch (surface_type) {
   case BRW_SURFACE_1D:
      assert(width <= 16384 && height == 1 && depth <= 2048);
      break;
   case BRW_SURFACE_2D:
      assert(width <= 16384 && height <= 16384 && depth <= 2048);
      break;
   case BRW_SURFACE_3D:
      assert(width <= 2048 && height <= 2048 && depth <= 2048);
      break;
   case BRW_SURFACE_CUBE:
      assert(width <= 16384 && height <= 16384 && depth <= 86);
      assert(width == height);
      break;
   default:
      assert(!"unexpected surface type");
      break;
   }

   /*
    * Compute the offset to the layer manually.
    *
    * For rendering, the hardware requires LOD to be the same for all render
    * targets and the depth buffer.  We need to compute the offset to the
    * layer manually and always set LOD to 0.
    */
   if (is_rt) {
      /* we lose the capability for layered rendering */
      assert(num_levels == 1 && num_layers == 1);

      layer_offset = ilo_resource_get_slice_offset(res,
            first_level, first_layer, true, &x_offset, &y_offset);

      assert(x_offset % 4 == 0);
      assert(y_offset % 2 == 0);
      x_offset /= 4;
      y_offset /= 2;

      /* derive the size for the LOD */
      width = u_minify(res->base.width0, first_level);
      height = u_minify(res->base.height0, first_level);
      if (surface_type == BRW_SURFACE_3D)
         depth = u_minify(res->base.depth0, first_level);

      first_level = 0;
      first_layer = 0;
      lod = 0;
   }
   else {
      layer_offset = 0;
      x_offset = 0;
      y_offset = 0;
      lod = num_levels - 1;
   }

   /*
    * From the Ivy Bridge PRM, volume 4 part 1, page 68:
    *
    *     "The Base Address for linear render target surfaces and surfaces
    *      accessed with the typed surface read/write data port messages must
    *      be element-size aligned, for non-YUV surface formats, or a multiple
    *      of 2 element-sizes for YUV surface formats.  Other linear surfaces
    *      have no alignment requirements (byte alignment is sufficient)."
    *
    * From the Ivy Bridge PRM, volume 4 part 1, page 70:
    *
    *     "For linear render target surfaces and surfaces accessed with the
    *      typed data port messages, the pitch must be a multiple of the
    *      element size for non-YUV surface formats. Pitch must be a multiple
    *      of 2 * element size for YUV surface formats. For linear surfaces
    *      with Surface Type of SURFTYPE_STRBUF, the pitch must be a multiple
    *      of 4 bytes.For other linear surfaces, the pitch can be any multiple
    *      of bytes."
    *
    * From the Ivy Bridge PRM, volume 4 part 1, page 74:
    *
    *     "For linear surfaces, this field (X Offset) must be zero."
    */
   if (res->tiling == INTEL_TILING_NONE) {
      if (is_rt) {
         const int elem_size = util_format_get_blocksize(format);
         assert(layer_offset % elem_size == 0);
         assert(pitch % elem_size == 0);
      }

      assert(!x_offset);
   }

   dw[0] = surface_type << BRW_SURFACE_TYPE_SHIFT |
           surface_format << BRW_SURFACE_FORMAT_SHIFT |
           ilo_gpe_gen6_translate_winsys_tiling(res->tiling) << 13 |
           GEN7_SURFACE_ARYSPC_FULL;

   if (surface_type != BRW_SURFACE_3D && depth > 1)
      dw[0] |= GEN7_SURFACE_IS_ARRAY;

   if (res->valign_4)
      dw[0] |= GEN7_SURFACE_VALIGN_4;

   if (res->halign_8)
      dw[0] |= GEN7_SURFACE_HALIGN_8;

   if (render_cache_rw)
      dw[0] |= BRW_SURFACE_RC_READ_WRITE;

   if (surface_type == BRW_SURFACE_CUBE && !is_rt)
      dw[0] |= BRW_SURFACE_CUBEFACE_ENABLES;

   dw[1] = layer_offset;

   dw[2] = SET_FIELD(height - 1, GEN7_SURFACE_HEIGHT) |
           SET_FIELD(width - 1, GEN7_SURFACE_WIDTH);

   dw[3] = SET_FIELD(depth - 1, BRW_SURFACE_DEPTH) |
           (pitch - 1);

   dw[4] = first_layer << 18 |
           (depth - 1) << 7;

   if (res->base.nr_samples > 4)
      dw[4] |= GEN7_SURFACE_MULTISAMPLECOUNT_8;
   else if (res->base.nr_samples > 2)
      dw[4] |= GEN7_SURFACE_MULTISAMPLECOUNT_4;
   else
      dw[4] |= GEN7_SURFACE_MULTISAMPLECOUNT_1;

   dw[5] = x_offset << BRW_SURFACE_X_OFFSET_SHIFT |
           y_offset << BRW_SURFACE_Y_OFFSET_SHIFT |
           SET_FIELD(first_level, GEN7_SURFACE_MIN_LOD) |
           lod;

   dw[6] = 0;
   dw[7] = 0;
}

static uint32_t
gen7_emit_SURFACE_STATE(const struct ilo_gpe *gpe,
                        struct intel_bo *bo, bool for_render,
                        const uint32_t *dw, int num_dwords,
                        struct ilo_cp *cp)
{
   const int state_align = 32 / 4;
   const int state_len = 8;
   uint32_t state_offset;
   uint32_t read_domains, write_domain;

   ILO_GPE_VALID_GEN(gpe, 7, 7);
   assert(num_dwords == state_len);

   if (for_render) {
      read_domains = INTEL_DOMAIN_RENDER;
      write_domain = INTEL_DOMAIN_RENDER;
   }
   else {
      read_domains = INTEL_DOMAIN_SAMPLER;
      write_domain = 0;
   }

   ilo_cp_steal(cp, "SURFACE_STATE", state_len, state_align, &state_offset);
   ilo_cp_write(cp, dw[0]);
   ilo_cp_write_bo(cp, dw[1], bo, read_domains, write_domain);
   ilo_cp_write(cp, dw[2]);
   ilo_cp_write(cp, dw[3]);
   ilo_cp_write(cp, dw[4]);
   ilo_cp_write(cp, dw[5]);
   ilo_cp_write(cp, dw[6]);
   ilo_cp_write(cp, dw[7]);
   ilo_cp_end(cp);

   return state_offset;
}

static uint32_t
gen7_emit_surf_SURFACE_STATE(const struct ilo_gpe *gpe,
                             const struct pipe_surface *surface,
                             struct ilo_cp *cp)
{
   struct intel_bo *bo;
   uint32_t dw[8];

   ILO_GPE_VALID_GEN(gpe, 7, 7);

   if (surface && surface->texture) {
      struct ilo_resource *res = ilo_resource(surface->texture);

      bo = res->bo;

      /*
       * classic i965 sets render_cache_rw for constant buffers and sol
       * surfaces but not render buffers.  Why?
       */
      gen7_fill_normal_SURFACE_STATE(gpe, res, surface->format,
            surface->u.tex.level, 1,
            surface->u.tex.first_layer,
            surface->u.tex.last_layer - surface->u.tex.first_layer + 1,
            true, true, dw, Elements(dw));
   }
   else {
      bo = NULL;
      gen7_fill_null_SURFACE_STATE(gpe,
            surface->width, surface->height, 1, 0, dw, Elements(dw));
   }

   return gen7_emit_SURFACE_STATE(gpe, bo, true, dw, Elements(dw), cp);
}

static uint32_t
gen7_emit_view_SURFACE_STATE(const struct ilo_gpe *gpe,
                             const struct pipe_sampler_view *view,
                             struct ilo_cp *cp)
{
   struct ilo_resource *res = ilo_resource(view->texture);
   uint32_t dw[8];

   ILO_GPE_VALID_GEN(gpe, 7, 7);

   gen7_fill_normal_SURFACE_STATE(gpe, res, view->format,
         view->u.tex.first_level,
         view->u.tex.last_level - view->u.tex.first_level + 1,
         view->u.tex.first_layer,
         view->u.tex.last_layer - view->u.tex.first_layer + 1,
         false, false, dw, Elements(dw));

   return gen7_emit_SURFACE_STATE(gpe, res->bo, false, dw, Elements(dw), cp);
}

static uint32_t
gen7_emit_cbuf_SURFACE_STATE(const struct ilo_gpe *gpe,
                             const struct pipe_constant_buffer *cbuf,
                             struct ilo_cp *cp)
{
   const enum pipe_format elem_format = PIPE_FORMAT_R32G32B32A32_FLOAT;
   struct ilo_resource *res = ilo_resource(cbuf->buffer);
   uint32_t dw[8];

   ILO_GPE_VALID_GEN(gpe, 7, 7);

   gen7_fill_buffer_SURFACE_STATE(gpe, res,
         cbuf->buffer_offset, cbuf->buffer_size,
         util_format_get_blocksize(elem_format), elem_format,
         false, false, dw, Elements(dw));

   return gen7_emit_SURFACE_STATE(gpe, res->bo, false, dw, Elements(dw), cp);
}

static uint32_t
gen7_emit_SAMPLER_BORDER_COLOR_STATE(const struct ilo_gpe *gpe,
                                     const union pipe_color_union *color,
                                     struct ilo_cp *cp)
{
   const int state_align = 32 / 4;
   const int state_len = 4;
   uint32_t state_offset, *dw;

   ILO_GPE_VALID_GEN(gpe, 7, 7);

   dw = ilo_cp_steal_ptr(cp, "SAMPLER_BORDER_COLOR_STATE",
         state_len, state_align, &state_offset);
   memcpy(dw, color->f, 4 * 4);

   return state_offset;
}

static int
gen7_estimate_command_size(const struct ilo_gpe *gpe,
                           enum ilo_gpe_gen7_command cmd,
                           int arg)
{
   static const struct {
      int header;
      int body;
   } gen7_command_size_table[ILO_GPE_GEN7_COMMAND_COUNT] = {
      [ILO_GPE_GEN7_STATE_BASE_ADDRESS]                       = { 0,  10 },
      [ILO_GPE_GEN7_STATE_SIP]                                = { 0,  2  },
      [ILO_GPE_GEN7_3DSTATE_VF_STATISTICS]                    = { 0,  1  },
      [ILO_GPE_GEN7_PIPELINE_SELECT]                          = { 0,  1  },
      [ILO_GPE_GEN7_MEDIA_VFE_STATE]                          = { 0,  8  },
      [ILO_GPE_GEN7_MEDIA_CURBE_LOAD]                         = { 0,  4  },
      [ILO_GPE_GEN7_MEDIA_INTERFACE_DESCRIPTOR_LOAD]          = { 0,  4  },
      [ILO_GPE_GEN7_MEDIA_STATE_FLUSH]                        = { 0,  2  },
      [ILO_GPE_GEN7_GPGPU_WALKER]                             = { 0,  11 },
      [ILO_GPE_GEN7_3DSTATE_CLEAR_PARAMS]                     = { 0,  3  },
      [ILO_GPE_GEN7_3DSTATE_DEPTH_BUFFER]                     = { 0,  7  },
      [ILO_GPE_GEN7_3DSTATE_STENCIL_BUFFER]                   = { 0,  3  },
      [ILO_GPE_GEN7_3DSTATE_HIER_DEPTH_BUFFER]                = { 0,  3  },
      [ILO_GPE_GEN7_3DSTATE_VERTEX_BUFFERS]                   = { 1,  4  },
      [ILO_GPE_GEN7_3DSTATE_VERTEX_ELEMENTS]                  = { 1,  2  },
      [ILO_GPE_GEN7_3DSTATE_INDEX_BUFFER]                     = { 0,  3  },
      [ILO_GPE_GEN7_3DSTATE_CC_STATE_POINTERS]                = { 0,  2  },
      [ILO_GPE_GEN7_3DSTATE_SCISSOR_STATE_POINTERS]           = { 0,  2  },
      [ILO_GPE_GEN7_3DSTATE_VS]                               = { 0,  6  },
      [ILO_GPE_GEN7_3DSTATE_GS]                               = { 0,  7  },
      [ILO_GPE_GEN7_3DSTATE_CLIP]                             = { 0,  4  },
      [ILO_GPE_GEN7_3DSTATE_SF]                               = { 0,  7  },
      [ILO_GPE_GEN7_3DSTATE_WM]                               = { 0,  3  },
      [ILO_GPE_GEN7_3DSTATE_CONSTANT_VS]                      = { 0,  7  },
      [ILO_GPE_GEN7_3DSTATE_CONSTANT_GS]                      = { 0,  7  },
      [ILO_GPE_GEN7_3DSTATE_CONSTANT_PS]                      = { 0,  7  },
      [ILO_GPE_GEN7_3DSTATE_SAMPLE_MASK]                      = { 0,  2  },
      [ILO_GPE_GEN7_3DSTATE_CONSTANT_HS]                      = { 0,  7  },
      [ILO_GPE_GEN7_3DSTATE_CONSTANT_DS]                      = { 0,  7  },
      [ILO_GPE_GEN7_3DSTATE_HS]                               = { 0,  7  },
      [ILO_GPE_GEN7_3DSTATE_TE]                               = { 0,  4  },
      [ILO_GPE_GEN7_3DSTATE_DS]                               = { 0,  6  },
      [ILO_GPE_GEN7_3DSTATE_STREAMOUT]                        = { 0,  3  },
      [ILO_GPE_GEN7_3DSTATE_SBE]                              = { 0,  14 },
      [ILO_GPE_GEN7_3DSTATE_PS]                               = { 0,  8  },
      [ILO_GPE_GEN7_3DSTATE_VIEWPORT_STATE_POINTERS_SF_CLIP]  = { 0,  2  },
      [ILO_GPE_GEN7_3DSTATE_VIEWPORT_STATE_POINTERS_CC]       = { 0,  2  },
      [ILO_GPE_GEN7_3DSTATE_BLEND_STATE_POINTERS]             = { 0,  2  },
      [ILO_GPE_GEN7_3DSTATE_DEPTH_STENCIL_STATE_POINTERS]     = { 0,  2  },
      [ILO_GPE_GEN7_3DSTATE_BINDING_TABLE_POINTERS_VS]        = { 0,  2  },
      [ILO_GPE_GEN7_3DSTATE_BINDING_TABLE_POINTERS_HS]        = { 0,  2  },
      [ILO_GPE_GEN7_3DSTATE_BINDING_TABLE_POINTERS_DS]        = { 0,  2  },
      [ILO_GPE_GEN7_3DSTATE_BINDING_TABLE_POINTERS_GS]        = { 0,  2  },
      [ILO_GPE_GEN7_3DSTATE_BINDING_TABLE_POINTERS_PS]        = { 0,  2  },
      [ILO_GPE_GEN7_3DSTATE_SAMPLER_STATE_POINTERS_VS]        = { 0,  2  },
      [ILO_GPE_GEN7_3DSTATE_SAMPLER_STATE_POINTERS_HS]        = { 0,  2  },
      [ILO_GPE_GEN7_3DSTATE_SAMPLER_STATE_POINTERS_DS]        = { 0,  2  },
      [ILO_GPE_GEN7_3DSTATE_SAMPLER_STATE_POINTERS_GS]        = { 0,  2  },
      [ILO_GPE_GEN7_3DSTATE_SAMPLER_STATE_POINTERS_PS]        = { 0,  2  },
      [ILO_GPE_GEN7_3DSTATE_URB_VS]                           = { 0,  2  },
      [ILO_GPE_GEN7_3DSTATE_URB_HS]                           = { 0,  2  },
      [ILO_GPE_GEN7_3DSTATE_URB_DS]                           = { 0,  2  },
      [ILO_GPE_GEN7_3DSTATE_URB_GS]                           = { 0,  2  },
      [ILO_GPE_GEN7_3DSTATE_DRAWING_RECTANGLE]                = { 0,  4  },
      [ILO_GPE_GEN7_3DSTATE_POLY_STIPPLE_OFFSET]              = { 0,  2  },
      [ILO_GPE_GEN7_3DSTATE_POLY_STIPPLE_PATTERN]             = { 0,  33, },
      [ILO_GPE_GEN7_3DSTATE_LINE_STIPPLE]                     = { 0,  3  },
      [ILO_GPE_GEN7_3DSTATE_AA_LINE_PARAMETERS]               = { 0,  3  },
      [ILO_GPE_GEN7_3DSTATE_MULTISAMPLE]                      = { 0,  4  },
      [ILO_GPE_GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_VS]           = { 0,  2  },
      [ILO_GPE_GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_HS]           = { 0,  2  },
      [ILO_GPE_GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_DS]           = { 0,  2  },
      [ILO_GPE_GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_GS]           = { 0,  2  },
      [ILO_GPE_GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_PS]           = { 0,  2  },
      [ILO_GPE_GEN7_3DSTATE_SO_DECL_LIST]                     = { 3,  2  },
      [ILO_GPE_GEN7_3DSTATE_SO_BUFFER]                        = { 0,  4  },
      [ILO_GPE_GEN7_PIPE_CONTROL]                             = { 0,  5  },
      [ILO_GPE_GEN7_3DPRIMITIVE]                              = { 0,  7  },
   };
   const int header = gen7_command_size_table[cmd].header;
   const int body = gen7_command_size_table[cmd].body;
   const int count = arg;

   ILO_GPE_VALID_GEN(gpe, 7, 7);
   assert(cmd < ILO_GPE_GEN7_COMMAND_COUNT);

   return (likely(count)) ? header + body * count : 0;
}

static int
gen7_estimate_state_size(const struct ilo_gpe *gpe,
                        enum ilo_gpe_gen7_state state,
                        int arg)
{
   static const struct {
      int alignment;
      int body;
      bool is_array;
   } gen7_state_size_table[ILO_GPE_GEN7_STATE_COUNT] = {
      [ILO_GPE_GEN7_INTERFACE_DESCRIPTOR_DATA]          = { 8,  8,  true },
      [ILO_GPE_GEN7_SF_CLIP_VIEWPORT]                   = { 16, 16, true },
      [ILO_GPE_GEN7_CC_VIEWPORT]                        = { 8,  2,  true },
      [ILO_GPE_GEN7_COLOR_CALC_STATE]                   = { 16, 6,  false },
      [ILO_GPE_GEN7_BLEND_STATE]                        = { 16, 2,  true },
      [ILO_GPE_GEN7_DEPTH_STENCIL_STATE]                = { 16, 3,  false },
      [ILO_GPE_GEN7_SCISSOR_RECT]                       = { 8,  2,  true },
      [ILO_GPE_GEN7_BINDING_TABLE_STATE]                = { 8,  1,  true },
      [ILO_GPE_GEN7_SURFACE_STATE]                      = { 8,  8,  false },
      [ILO_GPE_GEN7_SAMPLER_STATE]                      = { 8,  4,  true },
      [ILO_GPE_GEN7_SAMPLER_BORDER_COLOR_STATE]         = { 8,  4,  false },
      [ILO_GPE_GEN7_PUSH_CONSTANT_BUFFER]               = { 8,  1,  true },
   };
   const int alignment = gen7_state_size_table[state].alignment;
   const int body = gen7_state_size_table[state].body;
   const bool is_array = gen7_state_size_table[state].is_array;
   const int count = arg;
   int estimate;

   ILO_GPE_VALID_GEN(gpe, 7, 7);
   assert(state < ILO_GPE_GEN7_STATE_COUNT);

   if (likely(count)) {
      if (is_array) {
         estimate = (alignment - 1) + body * count;
      }
      else {
         estimate = (alignment - 1) + body;
         /* all states are aligned */
         if (count > 1)
            estimate += util_align_npot(body, alignment) * (count - 1);
      }
   }
   else {
      estimate = 0;
   }

   return estimate;
}

static void
gen7_init(struct ilo_gpe_gen7 *gen7)
{
   const struct ilo_gpe_gen6 *gen6 = ilo_gpe_gen6_get();

   gen7->estimate_command_size = gen7_estimate_command_size;
   gen7->estimate_state_size = gen7_estimate_state_size;

#define GEN7_USE(gen7, name, from) gen7->emit_ ## name = from->emit_ ## name
#define GEN7_SET(gen7, name)       gen7->emit_ ## name = gen7_emit_ ## name
   GEN7_USE(gen7, STATE_BASE_ADDRESS, gen6);
   GEN7_USE(gen7, STATE_SIP, gen6);
   GEN7_USE(gen7, 3DSTATE_VF_STATISTICS, gen6);
   GEN7_USE(gen7, PIPELINE_SELECT, gen6);
   GEN7_USE(gen7, MEDIA_VFE_STATE, gen6);
   GEN7_USE(gen7, MEDIA_CURBE_LOAD, gen6);
   GEN7_USE(gen7, MEDIA_INTERFACE_DESCRIPTOR_LOAD, gen6);
   GEN7_USE(gen7, MEDIA_STATE_FLUSH, gen6);
   GEN7_SET(gen7, GPGPU_WALKER);
   GEN7_SET(gen7, 3DSTATE_CLEAR_PARAMS);
   GEN7_SET(gen7, 3DSTATE_DEPTH_BUFFER);
   GEN7_USE(gen7, 3DSTATE_STENCIL_BUFFER, gen6);
   GEN7_USE(gen7, 3DSTATE_HIER_DEPTH_BUFFER, gen6);
   GEN7_USE(gen7, 3DSTATE_VERTEX_BUFFERS, gen6);
   GEN7_USE(gen7, 3DSTATE_VERTEX_ELEMENTS, gen6);
   GEN7_USE(gen7, 3DSTATE_INDEX_BUFFER, gen6);
   GEN7_SET(gen7, 3DSTATE_CC_STATE_POINTERS);
   GEN7_USE(gen7, 3DSTATE_SCISSOR_STATE_POINTERS, gen6);
   GEN7_USE(gen7, 3DSTATE_VS, gen6);
   GEN7_SET(gen7, 3DSTATE_GS);
   GEN7_USE(gen7, 3DSTATE_CLIP, gen6);
   GEN7_SET(gen7, 3DSTATE_SF);
   GEN7_SET(gen7, 3DSTATE_WM);
   GEN7_SET(gen7, 3DSTATE_CONSTANT_VS);
   GEN7_SET(gen7, 3DSTATE_CONSTANT_GS);
   GEN7_SET(gen7, 3DSTATE_CONSTANT_PS);
   GEN7_SET(gen7, 3DSTATE_SAMPLE_MASK);
   GEN7_SET(gen7, 3DSTATE_CONSTANT_HS);
   GEN7_SET(gen7, 3DSTATE_CONSTANT_DS);
   GEN7_SET(gen7, 3DSTATE_HS);
   GEN7_SET(gen7, 3DSTATE_TE);
   GEN7_SET(gen7, 3DSTATE_DS);
   GEN7_SET(gen7, 3DSTATE_STREAMOUT);
   GEN7_SET(gen7, 3DSTATE_SBE);
   GEN7_SET(gen7, 3DSTATE_PS);
   GEN7_SET(gen7, 3DSTATE_VIEWPORT_STATE_POINTERS_SF_CLIP);
   GEN7_SET(gen7, 3DSTATE_VIEWPORT_STATE_POINTERS_CC);
   GEN7_SET(gen7, 3DSTATE_BLEND_STATE_POINTERS);
   GEN7_SET(gen7, 3DSTATE_DEPTH_STENCIL_STATE_POINTERS);
   GEN7_SET(gen7, 3DSTATE_BINDING_TABLE_POINTERS_VS);
   GEN7_SET(gen7, 3DSTATE_BINDING_TABLE_POINTERS_HS);
   GEN7_SET(gen7, 3DSTATE_BINDING_TABLE_POINTERS_DS);
   GEN7_SET(gen7, 3DSTATE_BINDING_TABLE_POINTERS_GS);
   GEN7_SET(gen7, 3DSTATE_BINDING_TABLE_POINTERS_PS);
   GEN7_SET(gen7, 3DSTATE_SAMPLER_STATE_POINTERS_VS);
   GEN7_SET(gen7, 3DSTATE_SAMPLER_STATE_POINTERS_HS);
   GEN7_SET(gen7, 3DSTATE_SAMPLER_STATE_POINTERS_DS);
   GEN7_SET(gen7, 3DSTATE_SAMPLER_STATE_POINTERS_GS);
   GEN7_SET(gen7, 3DSTATE_SAMPLER_STATE_POINTERS_PS);
   GEN7_SET(gen7, 3DSTATE_URB_VS);
   GEN7_SET(gen7, 3DSTATE_URB_HS);
   GEN7_SET(gen7, 3DSTATE_URB_DS);
   GEN7_SET(gen7, 3DSTATE_URB_GS);
   GEN7_USE(gen7, 3DSTATE_DRAWING_RECTANGLE, gen6);
   GEN7_USE(gen7, 3DSTATE_POLY_STIPPLE_OFFSET, gen6);
   GEN7_USE(gen7, 3DSTATE_POLY_STIPPLE_PATTERN, gen6);
   GEN7_USE(gen7, 3DSTATE_LINE_STIPPLE, gen6);
   GEN7_USE(gen7, 3DSTATE_AA_LINE_PARAMETERS, gen6);
   GEN7_USE(gen7, 3DSTATE_MULTISAMPLE, gen6);
   GEN7_SET(gen7, 3DSTATE_PUSH_CONSTANT_ALLOC_VS);
   GEN7_SET(gen7, 3DSTATE_PUSH_CONSTANT_ALLOC_HS);
   GEN7_SET(gen7, 3DSTATE_PUSH_CONSTANT_ALLOC_DS);
   GEN7_SET(gen7, 3DSTATE_PUSH_CONSTANT_ALLOC_GS);
   GEN7_SET(gen7, 3DSTATE_PUSH_CONSTANT_ALLOC_PS);
   GEN7_SET(gen7, 3DSTATE_SO_DECL_LIST);
   GEN7_SET(gen7, 3DSTATE_SO_BUFFER);
   GEN7_USE(gen7, PIPE_CONTROL, gen6);
   GEN7_SET(gen7, 3DPRIMITIVE);
   GEN7_USE(gen7, INTERFACE_DESCRIPTOR_DATA, gen6);
   GEN7_SET(gen7, SF_CLIP_VIEWPORT);
   GEN7_USE(gen7, CC_VIEWPORT, gen6);
   GEN7_USE(gen7, COLOR_CALC_STATE, gen6);
   GEN7_USE(gen7, BLEND_STATE, gen6);
   GEN7_USE(gen7, DEPTH_STENCIL_STATE, gen6);
   GEN7_USE(gen7, SCISSOR_RECT, gen6);
   GEN7_USE(gen7, BINDING_TABLE_STATE, gen6);
   GEN7_SET(gen7, surf_SURFACE_STATE);
   GEN7_SET(gen7, view_SURFACE_STATE);
   GEN7_SET(gen7, cbuf_SURFACE_STATE);
   GEN7_USE(gen7, SAMPLER_STATE, gen6);
   GEN7_SET(gen7, SAMPLER_BORDER_COLOR_STATE);
   GEN7_USE(gen7, push_constant_buffer, gen6);
#undef GEN7_USE
#undef GEN7_SET
}

static struct ilo_gpe_gen7 gen7_gpe;

const struct ilo_gpe_gen7 *
ilo_gpe_gen7_get(void)
{
   if (!gen7_gpe.estimate_command_size)
      gen7_init(&gen7_gpe);

   return &gen7_gpe;
}

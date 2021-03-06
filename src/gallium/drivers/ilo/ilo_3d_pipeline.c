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

#include "util/u_prim.h"
#include "intel_winsys.h"

#include "ilo_context.h"
#include "ilo_cp.h"
#include "ilo_state.h"
#include "ilo_3d_pipeline_gen6.h"
#include "ilo_3d_pipeline_gen7.h"
#include "ilo_3d_pipeline.h"

/* in U0.4 */
struct sample_position {
   uint8_t x, y;
};

/* \see gen6_get_sample_position() */
static const struct sample_position sample_position_1x[1] = {
   {  8,  8 },
};

static const struct sample_position sample_position_4x[4] = {
   {  6,  2 }, /* distance from the center is sqrt(40) */
   { 14,  6 }, /* distance from the center is sqrt(40) */
   {  2, 10 }, /* distance from the center is sqrt(40) */
   { 10, 14 }, /* distance from the center is sqrt(40) */
};

static const struct sample_position sample_position_8x[8] = {
   {  7,  9 }, /* distance from the center is sqrt(2) */
   {  9, 13 }, /* distance from the center is sqrt(26) */
   { 11,  3 }, /* distance from the center is sqrt(34) */
   { 13, 11 }, /* distance from the center is sqrt(34) */
   {  1,  7 }, /* distance from the center is sqrt(50) */
   {  5,  1 }, /* distance from the center is sqrt(58) */
   { 15,  5 }, /* distance from the center is sqrt(58) */
   {  3, 15 }, /* distance from the center is sqrt(74) */
};

struct ilo_3d_pipeline *
ilo_3d_pipeline_create(struct ilo_cp *cp, int gen, int gt)
{
   struct ilo_3d_pipeline *p;
   int i;

   p = CALLOC_STRUCT(ilo_3d_pipeline);
   if (!p)
      return NULL;

   p->cp = cp;
   p->gen = gen;

   switch (p->gen) {
   case ILO_GEN(6):
      ilo_3d_pipeline_init_gen6(p);
      break;
   case ILO_GEN(7):
      ilo_3d_pipeline_init_gen7(p);
      break;
   default:
      assert(!"unsupported GEN");
      FREE(p);
      return NULL;
      break;
   }

   p->gpe.gen = p->gen;
   p->gpe.gt = gt;

   p->invalidate_flags = ILO_3D_PIPELINE_INVALIDATE_ALL;

   p->workaround_bo = p->cp->winsys->alloc_buffer(p->cp->winsys,
         "PIPE_CONTROL workaround", 4096, 0);
   if (!p->workaround_bo) {
      ilo_warn("failed to allocate PIPE_CONTROL workaround bo\n");
      FREE(p);
      return NULL;
   }

   p->packed_sample_position_1x =
      sample_position_1x[0].x << 4 |
      sample_position_1x[0].y;

   /* pack into dwords */
   for (i = 0; i < 4; i++) {
      p->packed_sample_position_4x |=
         sample_position_4x[i].x << (8 * i + 4) |
         sample_position_4x[i].y << (8 * i);

      p->packed_sample_position_8x[0] |=
         sample_position_8x[i].x << (8 * i + 4) |
         sample_position_8x[i].y << (8 * i);

      p->packed_sample_position_8x[1] |=
         sample_position_8x[4 + i].x << (8 * i + 4) |
         sample_position_8x[4 + i].y << (8 * i);
   }

   return p;
}

void
ilo_3d_pipeline_destroy(struct ilo_3d_pipeline *p)
{
   if (p->workaround_bo)
      p->workaround_bo->unreference(p->workaround_bo);

   FREE(p);
}

static void
handle_invalid_batch_bo(struct ilo_3d_pipeline *p, bool unset)
{
   if (p->invalidate_flags & ILO_3D_PIPELINE_INVALIDATE_BATCH_BO) {
      if (p->gen == ILO_GEN(6))
         p->state.has_gen6_wa_pipe_control = false;

      if (unset)
         p->invalidate_flags &= ~ILO_3D_PIPELINE_INVALIDATE_BATCH_BO;
   }
}

/* XXX move to u_prim.h */
static unsigned
prim_count(unsigned prim, unsigned num_verts)
{
   unsigned num_prims;

   u_trim_pipe_prim(prim, &num_verts);

   switch (prim) {
   case PIPE_PRIM_POINTS:
      num_prims = num_verts;
      break;
   case PIPE_PRIM_LINES:
      num_prims = num_verts / 2;
      break;
   case PIPE_PRIM_LINE_LOOP:
      num_prims = num_verts;
      break;
   case PIPE_PRIM_LINE_STRIP:
      num_prims = num_verts - 1;
      break;
   case PIPE_PRIM_TRIANGLES:
      num_prims = num_verts / 3;
      break;
   case PIPE_PRIM_TRIANGLE_STRIP:
   case PIPE_PRIM_TRIANGLE_FAN:
      num_prims = num_verts - 2;
      break;
   case PIPE_PRIM_QUADS:
      num_prims = (num_verts / 4) * 2;
      break;
   case PIPE_PRIM_QUAD_STRIP:
      num_prims = (num_verts / 2 - 1) * 2;
      break;
   case PIPE_PRIM_POLYGON:
      num_prims = num_verts - 2;
      break;
   case PIPE_PRIM_LINES_ADJACENCY:
      num_prims = num_verts / 4;
      break;
   case PIPE_PRIM_LINE_STRIP_ADJACENCY:
      num_prims = num_verts - 3;
      break;
   case PIPE_PRIM_TRIANGLES_ADJACENCY:
      /* u_trim_pipe_prim is wrong? */
      num_verts += 1;

      num_prims = num_verts / 6;
      break;
   case PIPE_PRIM_TRIANGLE_STRIP_ADJACENCY:
      /* u_trim_pipe_prim is wrong? */
      if (num_verts >= 6)
         num_verts -= (num_verts % 2);
      else
         num_verts = 0;

      num_prims = (num_verts / 2 - 2);
      break;
   default:
      assert(!"unknown pipe prim");
      num_prims = 0;
      break;
   }

   return num_prims;
}

/**
 * Emit context states and 3DPRIMITIVE.
 */
bool
ilo_3d_pipeline_emit_draw(struct ilo_3d_pipeline *p,
                          const struct ilo_context *ilo,
                          const struct pipe_draw_info *info,
                          int *prim_generated, int *prim_emitted)
{
   bool success;

   /*
    * We keep track of the SVBI in the driver, so that we can restore it when
    * the HW context is invalidated (by another process).  The value needs to
    * be reset when the stream output targets are changed.
    */
   if (ilo->dirty & ILO_DIRTY_STREAM_OUTPUT_TARGETS)
      p->state.so_num_vertices = 0;

   while (true) {
      struct ilo_cp_jmp_buf jmp;
      int err;

      /* we will rewind if aperture check below fails */
      ilo_cp_setjmp(p->cp, &jmp);

      handle_invalid_batch_bo(p, false);

      /* draw! */
      ilo_cp_assert_no_implicit_flush(p->cp, true);
      p->emit_draw(p, ilo, info);
      ilo_cp_assert_no_implicit_flush(p->cp, false);

      err = ilo->winsys->check_aperture_space(ilo->winsys, &p->cp->bo, 1);
      if (!err) {
         success = true;
         break;
      }

      /* rewind */
      ilo_cp_longjmp(p->cp, &jmp);

      if (ilo_cp_empty(p->cp)) {
         success = false;
         break;
      }
      else {
         /* flush and try again */
         ilo_cp_flush(p->cp);
      }
   }

   if (success) {
      const int num_verts = u_vertices_per_prim(u_reduced_prim(info->mode));
      const int max_emit =
         (p->state.so_max_vertices - p->state.so_num_vertices) / num_verts;
      const int generated = prim_count(info->mode, info->count);
      const int emitted = MIN2(generated, max_emit);

      p->state.so_num_vertices += emitted * num_verts;

      if (prim_generated)
         *prim_generated = generated;

      if (prim_emitted)
         *prim_emitted = emitted;
   }

   p->invalidate_flags = 0x0;

   return success;
}

/**
 * Emit PIPE_CONTROL to flush all caches.
 */
void
ilo_3d_pipeline_emit_flush(struct ilo_3d_pipeline *p)
{
   handle_invalid_batch_bo(p, true);
   p->emit_flush(p);
}

/**
 * Emit PIPE_CONTROL with PIPE_CONTROL_WRITE_TIMESTAMP post-sync op.
 */
void
ilo_3d_pipeline_emit_write_timestamp(struct ilo_3d_pipeline *p,
                                     struct intel_bo *bo, int index)
{
   handle_invalid_batch_bo(p, true);
   p->emit_write_timestamp(p, bo, index);
}

/**
 * Emit PIPE_CONTROL with PIPE_CONTROL_WRITE_DEPTH_COUNT post-sync op.
 */
void
ilo_3d_pipeline_emit_write_depth_count(struct ilo_3d_pipeline *p,
                                       struct intel_bo *bo, int index)
{
   handle_invalid_batch_bo(p, true);
   p->emit_write_depth_count(p, bo, index);
}

void
ilo_3d_pipeline_get_sample_position(struct ilo_3d_pipeline *p,
                                    unsigned sample_count,
                                    unsigned sample_index,
                                    float *x, float *y)
{
   const struct sample_position *pos;

   switch (sample_count) {
   case 1:
      assert(sample_index < Elements(sample_position_1x));
      pos = sample_position_1x;
      break;
   case 4:
      assert(sample_index < Elements(sample_position_4x));
      pos = sample_position_4x;
      break;
   case 8:
      assert(sample_index < Elements(sample_position_8x));
      pos = sample_position_8x;
      break;
   default:
      assert(!"unknown sample count");
      *x = 0.5f;
      *y = 0.5f;
      return;
      break;
   }

   *x = (float) pos[sample_index].x / 16.0f;
   *y = (float) pos[sample_index].y / 16.0f;
}

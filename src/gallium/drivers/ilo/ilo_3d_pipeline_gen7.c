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

#include "util/u_dual_blend.h"
#include "intel_reg.h"

#include "ilo_common.h"
#include "ilo_context.h"
#include "ilo_cp.h"
#include "ilo_gpe_gen7.h"
#include "ilo_shader.h"
#include "ilo_state.h"
#include "ilo_3d_pipeline.h"
#include "ilo_3d_pipeline_gen6.h"
#include "ilo_3d_pipeline_gen7.h"

static void
gen7_wa_pipe_control_cs_stall(struct ilo_3d_pipeline *p,
                              bool change_multisample_state,
                              bool change_depth_state)
{
   struct intel_bo *bo = NULL;
   uint32_t dw1 = PIPE_CONTROL_CS_STALL;

   assert(p->gen == ILO_GEN(7));

   /* emit once */
   if (p->state.has_gen6_wa_pipe_control)
      return;
   p->state.has_gen6_wa_pipe_control = true;

   /*
    * From the Ivy Bridge PRM, volume 2 part 1, page 258:
    *
    *     "Due to an HW issue driver needs to send a pipe control with stall
    *      when ever there is state change in depth bias related state"
    *
    * From the Ivy Bridge PRM, volume 2 part 1, page 292:
    *
    *     "A PIPE_CONTOL command with the CS Stall bit set must be programmed
    *      in the ring after this instruction
    *      (3DSTATE_PUSH_CONSTANT_ALLOC_PS)."
    *
    * From the Ivy Bridge PRM, volume 2 part 1, page 304:
    *
    *     "Driver must ierarchi that all the caches in the depth pipe are
    *      flushed before this command (3DSTATE_MULTISAMPLE) is parsed. This
    *      requires driver to send a PIPE_CONTROL with a CS stall along with a
    *      Depth Flush prior to this command.
    *
    * From the Ivy Bridge PRM, volume 2 part 1, page 315:
    *
    *     "Driver must send a least one PIPE_CONTROL command with CS Stall and
    *      a post sync operation prior to the group of depth
    *      commands(3DSTATE_DEPTH_BUFFER, 3DSTATE_CLEAR_PARAMS,
    *      3DSTATE_STENCIL_BUFFER, and 3DSTATE_HIER_DEPTH_BUFFER)."
    */

   if (change_multisample_state)
      dw1 |= PIPE_CONTROL_DEPTH_CACHE_FLUSH;

   if (change_depth_state) {
      dw1 |= PIPE_CONTROL_WRITE_IMMEDIATE;
      bo = p->workaround_bo;
   }

   p->gen6_PIPE_CONTROL(&p->gpe, dw1, bo, 0, false, p->cp);
}

static void
gen7_wa_pipe_control_vs_depth_stall(struct ilo_3d_pipeline *p)
{
   assert(p->gen == ILO_GEN(7));

   /*
    * From the Ivy Bridge PRM, volume 2 part 1, page 106:
    *
    *     "A PIPE_CONTROL with Post-Sync Operation set to 1h and a depth stall
    *      needs to be sent just prior to any 3DSTATE_VS, 3DSTATE_URB_VS,
    *      3DSTATE_CONSTANT_VS, 3DSTATE_BINDING_TABLE_POINTER_VS,
    *      3DSTATE_SAMPLER_STATE_POINTER_VS command.  Only one PIPE_CONTROL
    *      needs to be sent before any combination of VS associated 3DSTATE."
    */
   p->gen6_PIPE_CONTROL(&p->gpe,
         PIPE_CONTROL_DEPTH_STALL |
         PIPE_CONTROL_WRITE_IMMEDIATE,
         p->workaround_bo, 0, false, p->cp);
}

static void
gen7_wa_pipe_control_wm_depth_stall(struct ilo_3d_pipeline *p,
                                    bool change_depth_buffer)
{
   assert(p->gen == ILO_GEN(7));

   /*
    * From the Ivy Bridge PRM, volume 2 part 1, page 276:
    *
    *     "The driver must make sure a PIPE_CONTROL with the Depth Stall
    *      Enable bit set after all the following states are programmed:
    *
    *       * 3DSTATE_PS
    *       * 3DSTATE_VIEWPORT_STATE_POINTERS_CC
    *       * 3DSTATE_CONSTANT_PS
    *       * 3DSTATE_BINDING_TABLE_POINTERS_PS
    *       * 3DSTATE_SAMPLER_STATE_POINTERS_PS
    *       * 3DSTATE_CC_STATE_POINTERS
    *       * 3DSTATE_BLEND_STATE_POINTERS
    *       * 3DSTATE_DEPTH_STENCIL_STATE_POINTERS"
    *
    * From the Ivy Bridge PRM, volume 2 part 1, page 315:
    *
    *     "Restriction: Prior to changing Depth/Stencil Buffer state (i.e.,
    *      any combination of 3DSTATE_DEPTH_BUFFER, 3DSTATE_CLEAR_PARAMS,
    *      3DSTATE_STENCIL_BUFFER, 3DSTATE_HIER_DEPTH_BUFFER) SW must first
    *      issue a pipelined depth stall (PIPE_CONTROL with Depth Stall bit
    *      set), followed by a pipelined depth cache flush (PIPE_CONTROL with
    *      Depth Flush Bit set, followed by another pipelined depth stall
    *      (PIPE_CONTROL with Depth Stall Bit set), unless SW can otherwise
    *      guarantee that the pipeline from WM onwards is already flushed
    *      (e.g., via a preceding MI_FLUSH)."
    */
   p->gen6_PIPE_CONTROL(&p->gpe,
         PIPE_CONTROL_DEPTH_STALL,
         NULL, 0, false, p->cp);

   if (!change_depth_buffer)
      return;

   p->gen6_PIPE_CONTROL(&p->gpe,
         PIPE_CONTROL_DEPTH_CACHE_FLUSH,
         NULL, 0, false, p->cp);

   p->gen6_PIPE_CONTROL(&p->gpe,
         PIPE_CONTROL_DEPTH_STALL,
         NULL, 0, false, p->cp);
}

static void
gen7_wa_pipe_control_wm_max_threads_stall(struct ilo_3d_pipeline *p)
{
   assert(p->gen == ILO_GEN(7));

   /*
    * From the Ivy Bridge PRM, volume 2 part 1, page 286:
    *
    *     "If this field (Maximum Number of Threads in 3DSTATE_WM) is changed
    *      between 3DPRIMITIVE commands, a PIPE_CONTROL command with Stall at
    *      Pixel Scoreboard set is required to be issued."
    */
   p->gen6_PIPE_CONTROL(&p->gpe,
         PIPE_CONTROL_STALL_AT_SCOREBOARD,
         NULL, 0, false, p->cp);

}

#define DIRTY(state) (session->pipe_dirty & ILO_DIRTY_ ## state)

static void
gen7_pipeline_common_urb(struct ilo_3d_pipeline *p,
                         const struct ilo_context *ilo,
                         struct gen6_pipeline_session *session)
{
   /* 3DSTATE_URB_{VS,GS,HS,DS} */
   if (DIRTY(VERTEX_ELEMENTS) || DIRTY(VS)) {
      const struct ilo_shader *vs = (ilo->vs) ? ilo->vs->shader : NULL;
      /* the first 16KB are reserved for VS and PS PCBs */
      const int offset = 16 * 1024;
      int vs_entry_size, vs_total_size;

      vs_entry_size = (vs) ? vs->out.count : 0;

      /*
       * From the Ivy Bridge PRM, volume 2 part 1, page 35:
       *
       *     "Programming Restriction: As the VS URB entry serves as both the
       *      per-vertex input and output of the VS shader, the VS URB
       *      Allocation Size must be sized to the maximum of the vertex input
       *      and output structures."
       */
      if (vs_entry_size < ilo->vertex_elements->num_elements)
         vs_entry_size = ilo->vertex_elements->num_elements;

      vs_entry_size *= sizeof(float) * 4;
      vs_total_size = ilo->urb.size * 1024 - offset;

      gen7_wa_pipe_control_vs_depth_stall(p);

      p->gen7_3DSTATE_URB_VS(&p->gpe,
            offset, vs_total_size, vs_entry_size, p->cp);

      p->gen7_3DSTATE_URB_GS(&p->gpe, offset, 0, 0, p->cp);
      p->gen7_3DSTATE_URB_HS(&p->gpe, offset, 0, 0, p->cp);
      p->gen7_3DSTATE_URB_DS(&p->gpe, offset, 0, 0, p->cp);
   }
}

static void
gen7_pipeline_common_pcb_alloc(struct ilo_3d_pipeline *p,
                               const struct ilo_context *ilo,
                               struct gen6_pipeline_session *session)
{
   /* 3DSTATE_PUSH_CONSTANT_ALLOC_{VS,PS} */
   if (session->hw_ctx_changed) {
      /*
       * push constant buffers are only allowed to take up at most the first
       * 16KB of the URB
       */
      p->gen7_3DSTATE_PUSH_CONSTANT_ALLOC_VS(&p->gpe,
            0, 8192, p->cp);

      p->gen7_3DSTATE_PUSH_CONSTANT_ALLOC_PS(&p->gpe,
            8192, 8192, p->cp);

      gen7_wa_pipe_control_cs_stall(p, true, true);
   }
}

static void
gen7_pipeline_common_pointers_1(struct ilo_3d_pipeline *p,
                                const struct ilo_context *ilo,
                                struct gen6_pipeline_session *session)
{
   /* 3DSTATE_VIEWPORT_STATE_POINTERS_{CC,SF_CLIP} */
   if (session->viewport_state_changed) {
      p->gen7_3DSTATE_VIEWPORT_STATE_POINTERS_CC(&p->gpe,
            p->state.CC_VIEWPORT, p->cp);

      p->gen7_3DSTATE_VIEWPORT_STATE_POINTERS_SF_CLIP(&p->gpe,
            p->state.SF_CLIP_VIEWPORT, p->cp);
   }
}

static void
gen7_pipeline_common_pointers_2(struct ilo_3d_pipeline *p,
                                const struct ilo_context *ilo,
                                struct gen6_pipeline_session *session)
{
   /* 3DSTATE_BLEND_STATE_POINTERS */
   if (session->cc_state_blend_changed) {
      p->gen7_3DSTATE_BLEND_STATE_POINTERS(&p->gpe,
            p->state.BLEND_STATE, p->cp);
   }

   /* 3DSTATE_CC_STATE_POINTERS */
   if (session->cc_state_cc_changed) {
      p->gen7_3DSTATE_CC_STATE_POINTERS(&p->gpe,
            p->state.COLOR_CALC_STATE, p->cp);
   }

   /* 3DSTATE_DEPTH_STENCIL_STATE_POINTERS */
   if (session->cc_state_dsa_changed) {
      p->gen7_3DSTATE_DEPTH_STENCIL_STATE_POINTERS(&p->gpe,
            p->state.DEPTH_STENCIL_STATE, p->cp);
   }
}

static void
gen7_pipeline_vs(struct ilo_3d_pipeline *p,
                 const struct ilo_context *ilo,
                 struct gen6_pipeline_session *session)
{
   const bool emit_3dstate_binding_table = session->binding_table_vs_changed;
   const bool emit_3dstate_sampler_state = session->sampler_state_vs_changed;
   /* see gen6_pipeline_vs() */
   const bool emit_3dstate_constant_vs = session->pcb_state_vs_changed;
   const bool emit_3dstate_vs = (DIRTY(VS) || DIRTY(VERTEX_SAMPLERS));

   /* emit depth stall before any of the VS commands */
   if (emit_3dstate_binding_table || emit_3dstate_sampler_state ||
       emit_3dstate_constant_vs || emit_3dstate_vs)
       gen7_wa_pipe_control_vs_depth_stall(p);

   /* 3DSTATE_BINDING_TABLE_POINTERS_VS */
   if (emit_3dstate_binding_table) {
      p->gen7_3DSTATE_BINDING_TABLE_POINTERS_VS(&p->gpe,
            p->state.vs.BINDING_TABLE_STATE, p->cp);
   }

   /* 3DSTATE_SAMPLER_STATE_POINTERS_VS */
   if (emit_3dstate_sampler_state) {
      p->gen7_3DSTATE_SAMPLER_STATE_POINTERS_VS(&p->gpe,
            p->state.vs.SAMPLER_STATE, p->cp);
   }

   gen6_pipeline_vs(p, ilo, session);
}

static void
gen7_pipeline_hs(struct ilo_3d_pipeline *p,
                 const struct ilo_context *ilo,
                 struct gen6_pipeline_session *session)
{
   /* 3DSTATE_CONSTANT_HS and 3DSTATE_HS */
   if (session->hw_ctx_changed) {
      p->gen7_3DSTATE_CONSTANT_HS(&p->gpe, 0, 0, 0, p->cp);
      p->gen7_3DSTATE_HS(&p->gpe, NULL, 0, 0, p->cp);
   }

   /* 3DSTATE_BINDING_TABLE_POINTERS_HS */
   if (session->hw_ctx_changed)
      p->gen7_3DSTATE_BINDING_TABLE_POINTERS_HS(&p->gpe, 0, p->cp);
}

static void
gen7_pipeline_te(struct ilo_3d_pipeline *p,
                 const struct ilo_context *ilo,
                 struct gen6_pipeline_session *session)
{
   /* 3DSTATE_TE */
   if (session->hw_ctx_changed)
      p->gen7_3DSTATE_TE(&p->gpe, p->cp);
}

static void
gen7_pipeline_ds(struct ilo_3d_pipeline *p,
                 const struct ilo_context *ilo,
                 struct gen6_pipeline_session *session)
{
   /* 3DSTATE_CONSTANT_DS and 3DSTATE_DS */
   if (session->hw_ctx_changed) {
      p->gen7_3DSTATE_CONSTANT_DS(&p->gpe, 0, 0, 0, p->cp);
      p->gen7_3DSTATE_DS(&p->gpe, NULL, 0, 0, p->cp);
   }

   /* 3DSTATE_BINDING_TABLE_POINTERS_DS */
   if (session->hw_ctx_changed)
      p->gen7_3DSTATE_BINDING_TABLE_POINTERS_DS(&p->gpe, 0, p->cp);

}

static void
gen7_pipeline_gs(struct ilo_3d_pipeline *p,
                 const struct ilo_context *ilo,
                 struct gen6_pipeline_session *session)
{
   /* 3DSTATE_CONSTANT_GS and 3DSTATE_GS */
   if (session->hw_ctx_changed) {
      p->gen6_3DSTATE_CONSTANT_GS(&p->gpe, 0, 0, 0, p->cp);
      p->gen7_3DSTATE_GS(&p->gpe, NULL, 0, 0, p->cp);
   }

   /* 3DSTATE_BINDING_TABLE_POINTERS_GS */
   if (session->binding_table_gs_changed) {
      p->gen7_3DSTATE_BINDING_TABLE_POINTERS_GS(&p->gpe,
            p->state.gs.BINDING_TABLE_STATE, p->cp);
   }
}

static void
gen7_pipeline_sol(struct ilo_3d_pipeline *p,
                  const struct ilo_context *ilo,
                  struct gen6_pipeline_session *session)
{
   if (session->hw_ctx_changed) {
      if (ilo->stream_output_targets.num_targets) {
         int i;

         for (i = 0; i < 4; i++)
            p->gen7_3DSTATE_SO_BUFFER(&p->gpe, i, false, p->cp);

         p->gen7_3DSTATE_SO_DECL_LIST(&p->gpe, p->cp);
      }

      p->gen7_3DSTATE_STREAMOUT(&p->gpe, false, false, false, p->cp);
   }
}

static void
gen7_pipeline_sf(struct ilo_3d_pipeline *p,
                 const struct ilo_context *ilo,
                 struct gen6_pipeline_session *session)
{
   /* 3DSTATE_SBE */
   if (DIRTY(RASTERIZER) || DIRTY(VS) || DIRTY(GS) || DIRTY(FS)) {
      const struct ilo_shader *fs = (ilo->fs)? ilo->fs->shader : NULL;
      const struct ilo_shader *last_sh =
         (ilo->gs)? ilo->gs->shader :
         (ilo->vs)? ilo->vs->shader : NULL;

      p->gen7_3DSTATE_SBE(&p->gpe,
            ilo->rasterizer, fs, last_sh, p->cp);
   }

   /* 3DSTATE_SF */
   if (DIRTY(RASTERIZER) || DIRTY(FRAMEBUFFER)) {
      gen7_wa_pipe_control_cs_stall(p, true, true);

      p->gen7_3DSTATE_SF(&p->gpe,
            ilo->rasterizer, ilo->framebuffer.zsbuf, p->cp);
   }
}

static void
gen7_pipeline_wm(struct ilo_3d_pipeline *p,
                 const struct ilo_context *ilo,
                 struct gen6_pipeline_session *session)
{
   /* 3DSTATE_WM */
   if (DIRTY(FS) || DIRTY(BLEND) || DIRTY(DEPTH_STENCIL_ALPHA) ||
       DIRTY(RASTERIZER)) {
      const struct ilo_shader *fs = (ilo->fs)? ilo->fs->shader : NULL;
      const bool cc_may_kill = (ilo->depth_stencil_alpha->alpha.enabled ||
                                   ilo->blend->alpha_to_coverage);

      if (fs)
         assert(!fs->pcb.clip_state_size);

      if (p->gen == ILO_GEN(7) && session->hw_ctx_changed)
         gen7_wa_pipe_control_wm_max_threads_stall(p);

      p->gen7_3DSTATE_WM(&p->gpe,
            fs, ilo->rasterizer, cc_may_kill, p->cp);
   }

   /* 3DSTATE_BINDING_TABLE_POINTERS_PS */
   if (session->binding_table_fs_changed) {
      p->gen7_3DSTATE_BINDING_TABLE_POINTERS_PS(&p->gpe,
            p->state.wm.BINDING_TABLE_STATE, p->cp);
   }

   /* 3DSTATE_SAMPLER_STATE_POINTERS_PS */
   if (session->sampler_state_fs_changed) {
      p->gen7_3DSTATE_SAMPLER_STATE_POINTERS_PS(&p->gpe,
            p->state.wm.SAMPLER_STATE, p->cp);
   }

   /* 3DSTATE_CONSTANT_PS */
   if (session->pcb_state_fs_changed)
      p->gen6_3DSTATE_CONSTANT_PS(&p->gpe, NULL, NULL, 0, p->cp);

   /* 3DSTATE_PS */
   if (DIRTY(FS) || DIRTY(FRAGMENT_SAMPLERS) ||
       DIRTY(BLEND)) {
      const struct ilo_shader *fs = (ilo->fs)? ilo->fs->shader : NULL;
      const int num_samplers =
         ilo->samplers[PIPE_SHADER_FRAGMENT].num_samplers;
      const bool dual_blend = (!ilo->blend->logicop_enable &&
                                  ilo->blend->rt[0].blend_enable &&
                                  util_blend_state_is_dual(ilo->blend, 0));

      if (fs)
         assert(!fs->pcb.clip_state_size);

      p->gen7_3DSTATE_PS(&p->gpe,
            fs, ilo->max_wm_threads, num_samplers,
            dual_blend, p->cp);
   }

   /* 3DSTATE_SCISSOR_STATE_POINTERS */
   if (session->scissor_state_changed) {
      p->gen6_3DSTATE_SCISSOR_STATE_POINTERS(&p->gpe,
            p->state.SCISSOR_RECT, p->cp);
   }

   /* XXX what is the best way to know if this workaround is needed? */
   {
      const bool emit_3dstate_ps = (DIRTY(FS) ||
                                       DIRTY(FRAGMENT_SAMPLERS) ||
                                       DIRTY(BLEND));
      const bool emit_3dstate_depth_buffer =
         (DIRTY(FRAMEBUFFER) || DIRTY(DEPTH_STENCIL_ALPHA) ||
          session->state_bo_changed);

      if (emit_3dstate_ps ||
          emit_3dstate_depth_buffer ||
          session->pcb_state_fs_changed ||
          session->viewport_state_changed ||
          session->binding_table_fs_changed ||
          session->sampler_state_fs_changed ||
          session->cc_state_cc_changed ||
          session->cc_state_blend_changed ||
          session->cc_state_dsa_changed)
         gen7_wa_pipe_control_wm_depth_stall(p, emit_3dstate_depth_buffer);
   }

   /*
    * glCopyPixels() with GL_DEPTH, which flushes the context before copying
    * the depth buffer to a temporary texture, could not update the depth
    * buffer _sometimes_.  Reissuing 3DSTATE_DEPTH_BUFFER in the new batch
    * makes the problem gone.
    */

   /* 3DSTATE_DEPTH_BUFFER and 3DSTATE_CLEAR_PARAMS */
   if (DIRTY(FRAMEBUFFER) || DIRTY(DEPTH_STENCIL_ALPHA) ||
       session->state_bo_changed) {
      p->gen7_3DSTATE_DEPTH_BUFFER(&p->gpe,
            ilo->framebuffer.zsbuf,
            ilo->depth_stencil_alpha,
            false, p->cp);

      /* TODO */
      p->gen6_3DSTATE_CLEAR_PARAMS(&p->gpe, 0, p->cp);
   }
}

static void
gen7_pipeline_wm_multisample(struct ilo_3d_pipeline *p,
                             const struct ilo_context *ilo,
                             struct gen6_pipeline_session *session)
{
   /* 3DSTATE_MULTISAMPLE and 3DSTATE_SAMPLE_MASK */
   if (DIRTY(SAMPLE_MASK) || DIRTY(FRAMEBUFFER)) {
      const uint32_t *packed_sample_pos;
      int num_samples = 1;

      gen7_wa_pipe_control_cs_stall(p, true, true);

      if (ilo->framebuffer.nr_cbufs)
         num_samples = ilo->framebuffer.cbufs[0]->texture->nr_samples;

      packed_sample_pos =
         (num_samples > 4) ? p->packed_sample_position_8x :
         (num_samples > 1) ? &p->packed_sample_position_4x :
         &p->packed_sample_position_1x;

      p->gen6_3DSTATE_MULTISAMPLE(&p->gpe, num_samples, packed_sample_pos,
            ilo->rasterizer->half_pixel_center, p->cp);

      p->gen7_3DSTATE_SAMPLE_MASK(&p->gpe,
            (num_samples > 1) ? ilo->sample_mask : 0x1,
            num_samples, p->cp);
   }
}

static void
gen7_pipeline_commands(struct ilo_3d_pipeline *p,
                       const struct ilo_context *ilo,
                       struct gen6_pipeline_session *session)
{
   /*
    * We try to keep the order of the commands match, as closely as possible,
    * that of the classic i965 driver.  It allows us to compare the command
    * streams easily.
    */
   gen6_pipeline_common_select(p, ilo, session);
   gen6_pipeline_common_sip(p, ilo, session);
   gen6_pipeline_vf_statistics(p, ilo, session);
   gen7_pipeline_common_pcb_alloc(p, ilo, session);
   gen6_pipeline_common_base_address(p, ilo, session);
   gen7_pipeline_common_pointers_1(p, ilo, session);
   gen7_pipeline_common_urb(p, ilo, session);
   gen7_pipeline_common_pointers_2(p, ilo, session);
   gen7_pipeline_wm_multisample(p, ilo, session);
   gen7_pipeline_gs(p, ilo, session);
   gen7_pipeline_hs(p, ilo, session);
   gen7_pipeline_te(p, ilo, session);
   gen7_pipeline_ds(p, ilo, session);
   gen7_pipeline_vs(p, ilo, session);
   gen7_pipeline_sol(p, ilo, session);
   gen6_pipeline_clip(p, ilo, session);
   gen7_pipeline_sf(p, ilo, session);
   gen7_pipeline_wm(p, ilo, session);
   gen6_pipeline_wm_raster(p, ilo, session);
   gen6_pipeline_sf_rect(p, ilo, session);
   gen6_pipeline_vf(p, ilo, session);
   gen6_pipeline_vf_draw(p, ilo, session);
}

static void
ilo_3d_pipeline_emit_draw_gen7(struct ilo_3d_pipeline *p,
                               const struct ilo_context *ilo,
                               const struct pipe_draw_info *info)
{
   struct gen6_pipeline_session session;

   gen6_pipeline_prepare(p, ilo, info, &session);

   session.emit_draw_states = gen6_pipeline_states;
   session.emit_draw_commands = gen7_pipeline_commands;

   gen6_pipeline_draw(p, ilo, &session);
   gen6_pipeline_end(p, ilo, &session);
}

static int
gen7_pipeline_estimate_commands(const struct ilo_3d_pipeline *p,
                                const struct ilo_gpe_gen7 *gen7,
                                const struct ilo_context *ilo)
{
   static int size;
   enum ilo_gpe_gen7_command cmd;

   if (size)
      return size;

   for (cmd = 0; cmd < ILO_GPE_GEN7_COMMAND_COUNT; cmd++) {
      int count;

      switch (cmd) {
      case ILO_GPE_GEN7_PIPE_CONTROL:
         /* for the workaround */
         count = 2;
         /* another one after 3DSTATE_URB */
         count += 1;
         /* and another one after 3DSTATE_CONSTANT_VS */
         count += 1;
         break;
      case ILO_GPE_GEN7_3DSTATE_VERTEX_BUFFERS:
         count = 33;
         break;
      case ILO_GPE_GEN7_3DSTATE_VERTEX_ELEMENTS:
         count = 34;
         break;
      case ILO_GPE_GEN7_MEDIA_VFE_STATE:
      case ILO_GPE_GEN7_MEDIA_CURBE_LOAD:
      case ILO_GPE_GEN7_MEDIA_INTERFACE_DESCRIPTOR_LOAD:
      case ILO_GPE_GEN7_MEDIA_STATE_FLUSH:
      case ILO_GPE_GEN7_GPGPU_WALKER:
         /* media commands */
         count = 0;
         break;
      default:
         count = 1;
         break;
      }

      if (count) {
         size += gen7->estimate_command_size(&p->gpe,
               cmd, count);
      }
   }

   return size;
}

static int
gen7_pipeline_estimate_states(const struct ilo_3d_pipeline *p,
                              const struct ilo_gpe_gen7 *gen7,
                              const struct ilo_context *ilo)
{
   static int static_size;
   int shader_type, count, size;

   if (!static_size) {
      struct {
         enum ilo_gpe_gen7_state state;
         int count;
      } static_states[] = {
         /* viewports */
         { ILO_GPE_GEN7_SF_CLIP_VIEWPORT,            1 },
         { ILO_GPE_GEN7_CC_VIEWPORT,                 1 },
         /* cc */
         { ILO_GPE_GEN7_COLOR_CALC_STATE,            1 },
         { ILO_GPE_GEN7_BLEND_STATE,                 ILO_MAX_DRAW_BUFFERS },
         { ILO_GPE_GEN7_DEPTH_STENCIL_STATE,         1 },
         /* scissors */
         { ILO_GPE_GEN7_SCISSOR_RECT,                1 },
         /* binding table (vs, gs, fs) */
         { ILO_GPE_GEN7_BINDING_TABLE_STATE,         ILO_MAX_VS_SURFACES },
         { ILO_GPE_GEN7_BINDING_TABLE_STATE,         ILO_MAX_GS_SURFACES },
         { ILO_GPE_GEN7_BINDING_TABLE_STATE,         ILO_MAX_WM_SURFACES },
      };
      int i;

      for (i = 0; i < Elements(static_states); i++) {
         static_size += gen7->estimate_state_size(&p->gpe,
               static_states[i].state,
               static_states[i].count);
      }
   }

   size = static_size;

   /*
    * render targets (fs)
    * sampler views (vs, fs)
    * constant buffers (vs, fs)
    */
   count = ilo->framebuffer.nr_cbufs;
   for (shader_type = 0; shader_type < PIPE_SHADER_TYPES; shader_type++) {
      count += ilo->sampler_views[shader_type].num_views;
      count += ilo->constant_buffers[shader_type].num_buffers;
   }

   if (count) {
      size += gen7->estimate_state_size(&p->gpe,
            ILO_GPE_GEN7_SURFACE_STATE, count);
   }

   /* samplers (vs, fs) */
   for (shader_type = 0; shader_type < PIPE_SHADER_TYPES; shader_type++) {
      count = ilo->samplers[shader_type].num_samplers;
      if (count) {
         size += gen7->estimate_state_size(&p->gpe,
               ILO_GPE_GEN7_SAMPLER_BORDER_COLOR_STATE, count);
         size += gen7->estimate_state_size(&p->gpe,
               ILO_GPE_GEN7_SAMPLER_STATE, count);
      }
   }

   /* pcb (vs) */
   if (ilo->vs && ilo->vs->shader->pcb.clip_state_size) {
      const int pcb_size = ilo->vs->shader->pcb.clip_state_size;

      size += gen7->estimate_state_size(&p->gpe,
            ILO_GPE_GEN7_PUSH_CONSTANT_BUFFER, pcb_size);
   }

   return size;
}

static int
ilo_3d_pipeline_estimate_size_gen7(struct ilo_3d_pipeline *p,
                                   enum ilo_3d_pipeline_action action,
                                   const void *arg)
{
   const struct ilo_gpe_gen7 *gen7 = ilo_gpe_gen7_get();
   int size;

   switch (action) {
   case ILO_3D_PIPELINE_DRAW:
      {
         const struct ilo_context *ilo = arg;

         size = gen7_pipeline_estimate_commands(p, gen7, ilo) +
            gen7_pipeline_estimate_states(p, gen7, ilo);
      }
      break;
   case ILO_3D_PIPELINE_FLUSH:
   case ILO_3D_PIPELINE_WRITE_TIMESTAMP:
   case ILO_3D_PIPELINE_WRITE_DEPTH_COUNT:
      size = gen7->estimate_command_size(&p->gpe,
            ILO_GPE_GEN7_PIPE_CONTROL, 1);
      break;
   default:
      assert(!"unknown 3D pipeline action");
      size = 0;
      break;
   }

   return size;
}

void
ilo_3d_pipeline_init_gen7(struct ilo_3d_pipeline *p)
{
   const struct ilo_gpe_gen7 *gen7 = ilo_gpe_gen7_get();

   p->estimate_size = ilo_3d_pipeline_estimate_size_gen7;
   p->emit_draw = ilo_3d_pipeline_emit_draw_gen7;
   p->emit_flush = ilo_3d_pipeline_emit_flush_gen6;
   p->emit_write_timestamp = ilo_3d_pipeline_emit_write_timestamp_gen6;
   p->emit_write_depth_count = ilo_3d_pipeline_emit_write_depth_count_gen6;

#define GEN6_USE(p, name, from) \
   p->gen6_ ## name = from->emit_ ## name
   GEN6_USE(p, STATE_BASE_ADDRESS, gen7);
   GEN6_USE(p, STATE_SIP, gen7);
   GEN6_USE(p, PIPELINE_SELECT, gen7);
   GEN6_USE(p, 3DSTATE_VERTEX_BUFFERS, gen7);
   GEN6_USE(p, 3DSTATE_VERTEX_ELEMENTS, gen7);
   GEN6_USE(p, 3DSTATE_INDEX_BUFFER, gen7);
   GEN6_USE(p, 3DSTATE_VF_STATISTICS, gen7);
   GEN6_USE(p, 3DSTATE_SCISSOR_STATE_POINTERS, gen7);
   GEN6_USE(p, 3DSTATE_VS, gen7);
   GEN6_USE(p, 3DSTATE_CLIP, gen7);
   GEN6_USE(p, 3DSTATE_CONSTANT_VS, gen7);
   GEN6_USE(p, 3DSTATE_CONSTANT_GS, gen7);
   GEN6_USE(p, 3DSTATE_CONSTANT_PS, gen7);
   GEN6_USE(p, 3DSTATE_DRAWING_RECTANGLE, gen7);
   GEN6_USE(p, 3DSTATE_POLY_STIPPLE_OFFSET, gen7);
   GEN6_USE(p, 3DSTATE_POLY_STIPPLE_PATTERN, gen7);
   GEN6_USE(p, 3DSTATE_LINE_STIPPLE, gen7);
   GEN6_USE(p, 3DSTATE_AA_LINE_PARAMETERS, gen7);
   GEN6_USE(p, 3DSTATE_MULTISAMPLE, gen7);
   GEN6_USE(p, 3DSTATE_STENCIL_BUFFER, gen7);
   GEN6_USE(p, 3DSTATE_HIER_DEPTH_BUFFER, gen7);
   GEN6_USE(p, 3DSTATE_CLEAR_PARAMS, gen7);
   GEN6_USE(p, PIPE_CONTROL, gen7);
   GEN6_USE(p, 3DPRIMITIVE, gen7);
   GEN6_USE(p, INTERFACE_DESCRIPTOR_DATA, gen7);
   GEN6_USE(p, CC_VIEWPORT, gen7);
   GEN6_USE(p, COLOR_CALC_STATE, gen7);
   GEN6_USE(p, BLEND_STATE, gen7);
   GEN6_USE(p, DEPTH_STENCIL_STATE, gen7);
   GEN6_USE(p, SCISSOR_RECT, gen7);
   GEN6_USE(p, BINDING_TABLE_STATE, gen7);
   GEN6_USE(p, surf_SURFACE_STATE, gen7);
   GEN6_USE(p, view_SURFACE_STATE, gen7);
   GEN6_USE(p, cbuf_SURFACE_STATE, gen7);
   GEN6_USE(p, SAMPLER_STATE, gen7);
   GEN6_USE(p, SAMPLER_BORDER_COLOR_STATE, gen7);
   GEN6_USE(p, push_constant_buffer, gen7);
#undef GEN6_USE

#define GEN7_USE(p, name, from) \
   p->gen7_ ## name = from->emit_ ## name
   GEN7_USE(p, 3DSTATE_DEPTH_BUFFER, gen7);
   GEN7_USE(p, 3DSTATE_CC_STATE_POINTERS, gen7);
   GEN7_USE(p, 3DSTATE_GS, gen7);
   GEN7_USE(p, 3DSTATE_SF, gen7);
   GEN7_USE(p, 3DSTATE_WM, gen7);
   GEN7_USE(p, 3DSTATE_SAMPLE_MASK, gen7);
   GEN7_USE(p, 3DSTATE_CONSTANT_HS, gen7);
   GEN7_USE(p, 3DSTATE_CONSTANT_DS, gen7);
   GEN7_USE(p, 3DSTATE_HS, gen7);
   GEN7_USE(p, 3DSTATE_TE, gen7);
   GEN7_USE(p, 3DSTATE_DS, gen7);
   GEN7_USE(p, 3DSTATE_STREAMOUT, gen7);
   GEN7_USE(p, 3DSTATE_SBE, gen7);
   GEN7_USE(p, 3DSTATE_PS, gen7);
   GEN7_USE(p, 3DSTATE_VIEWPORT_STATE_POINTERS_SF_CLIP, gen7);
   GEN7_USE(p, 3DSTATE_VIEWPORT_STATE_POINTERS_CC, gen7);
   GEN7_USE(p, 3DSTATE_BLEND_STATE_POINTERS, gen7);
   GEN7_USE(p, 3DSTATE_DEPTH_STENCIL_STATE_POINTERS, gen7);
   GEN7_USE(p, 3DSTATE_BINDING_TABLE_POINTERS_VS, gen7);
   GEN7_USE(p, 3DSTATE_BINDING_TABLE_POINTERS_HS, gen7);
   GEN7_USE(p, 3DSTATE_BINDING_TABLE_POINTERS_DS, gen7);
   GEN7_USE(p, 3DSTATE_BINDING_TABLE_POINTERS_GS, gen7);
   GEN7_USE(p, 3DSTATE_BINDING_TABLE_POINTERS_PS, gen7);
   GEN7_USE(p, 3DSTATE_SAMPLER_STATE_POINTERS_VS, gen7);
   GEN7_USE(p, 3DSTATE_SAMPLER_STATE_POINTERS_HS, gen7);
   GEN7_USE(p, 3DSTATE_SAMPLER_STATE_POINTERS_DS, gen7);
   GEN7_USE(p, 3DSTATE_SAMPLER_STATE_POINTERS_GS, gen7);
   GEN7_USE(p, 3DSTATE_SAMPLER_STATE_POINTERS_PS, gen7);
   GEN7_USE(p, 3DSTATE_URB_VS, gen7);
   GEN7_USE(p, 3DSTATE_URB_HS, gen7);
   GEN7_USE(p, 3DSTATE_URB_DS, gen7);
   GEN7_USE(p, 3DSTATE_URB_GS, gen7);
   GEN7_USE(p, 3DSTATE_PUSH_CONSTANT_ALLOC_VS, gen7);
   GEN7_USE(p, 3DSTATE_PUSH_CONSTANT_ALLOC_HS, gen7);
   GEN7_USE(p, 3DSTATE_PUSH_CONSTANT_ALLOC_DS, gen7);
   GEN7_USE(p, 3DSTATE_PUSH_CONSTANT_ALLOC_GS, gen7);
   GEN7_USE(p, 3DSTATE_PUSH_CONSTANT_ALLOC_PS, gen7);
   GEN7_USE(p, 3DSTATE_SO_DECL_LIST, gen7);
   GEN7_USE(p, 3DSTATE_SO_BUFFER, gen7);
   GEN7_USE(p, SF_CLIP_VIEWPORT, gen7);
#undef GEN7_USE
}

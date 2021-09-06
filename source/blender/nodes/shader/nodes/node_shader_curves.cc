/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.h"

/* **************** CURVE VEC  ******************** */
static bNodeSocketTemplate sh_node_curve_vec_in[] = {
    {SOCK_FLOAT, N_("Fac"), 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, PROP_FACTOR},
    {SOCK_VECTOR, N_("Vector"), 0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, PROP_NONE},
    {-1, ""},
};

static bNodeSocketTemplate sh_node_curve_vec_out[] = {
    {SOCK_VECTOR, N_("Vector")},
    {-1, ""},
};

static void node_shader_exec_curve_vec(void *UNUSED(data),
                                       int UNUSED(thread),
                                       bNode *node,
                                       bNodeExecData *UNUSED(execdata),
                                       bNodeStack **in,
                                       bNodeStack **out)
{
  float vec[3];

  /* stack order input:  vec */
  /* stack order output: vec */
  nodestack_get_vec(vec, SOCK_VECTOR, in[1]);
  BKE_curvemapping_evaluate3F((CurveMapping *)node->storage, out[0]->vec, vec);
}

static void node_shader_init_curve_vec(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->storage = BKE_curvemapping_add(3, -1.0f, -1.0f, 1.0f, 1.0f);
}

static int gpu_shader_curve_vec(GPUMaterial *mat,
                                bNode *node,
                                bNodeExecData *UNUSED(execdata),
                                GPUNodeStack *in,
                                GPUNodeStack *out)
{
  float *array, layer;
  int size;

  CurveMapping *cumap = (CurveMapping *)node->storage;

  BKE_curvemapping_table_RGBA(cumap, &array, &size);
  GPUNodeLink *tex = GPU_color_band(mat, size, array, &layer);

  float ext_xyz[3][4];
  float range_xyz[3];

  for (int a = 0; a < 3; a++) {
    const CurveMap *cm = &cumap->cm[a];
    ext_xyz[a][0] = cm->mintable;
    ext_xyz[a][2] = cm->maxtable;
    range_xyz[a] = 1.0f / max_ff(1e-8f, cm->maxtable - cm->mintable);
    /* Compute extrapolation gradients. */
    if ((cumap->flag & CUMA_EXTEND_EXTRAPOLATE) != 0) {
      ext_xyz[a][1] = (cm->ext_in[0] != 0.0f) ? (cm->ext_in[1] / (cm->ext_in[0] * range_xyz[a])) :
                                                1e8f;
      ext_xyz[a][3] = (cm->ext_out[0] != 0.0f) ?
                          (cm->ext_out[1] / (cm->ext_out[0] * range_xyz[a])) :
                          1e8f;
    }
    else {
      ext_xyz[a][1] = 0.0f;
      ext_xyz[a][3] = 0.0f;
    }
  }

  return GPU_stack_link(mat,
                        node,
                        "curves_vec",
                        in,
                        out,
                        tex,
                        GPU_constant(&layer),
                        GPU_uniform(range_xyz),
                        GPU_uniform(ext_xyz[0]),
                        GPU_uniform(ext_xyz[1]),
                        GPU_uniform(ext_xyz[2]));
}

class CurveVecFunction : public blender::fn::MultiFunction {
 private:
  const CurveMapping &cumap_;

 public:
  CurveVecFunction(const CurveMapping &cumap) : cumap_(cumap)
  {
    static blender::fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static blender::fn::MFSignature create_signature()
  {
    blender::fn::MFSignatureBuilder signature{"Curve Vec"};
    signature.single_input<float>("Fac");
    signature.single_input<blender::float3>("Vector");
    signature.single_output<blender::float3>("Vector");
    return signature.build();
  }

  void call(blender::IndexMask mask,
            blender::fn::MFParams params,
            blender::fn::MFContext UNUSED(context)) const override
  {
    const blender::VArray<float> &fac = params.readonly_single_input<float>(0, "Fac");
    const blender::VArray<blender::float3> &vec_in = params.readonly_single_input<blender::float3>(
        1, "Vector");
    blender::MutableSpan<blender::float3> vec_out =
        params.uninitialized_single_output<blender::float3>(2, "Vector");

    for (int64_t i : mask) {
      BKE_curvemapping_evaluate3F(&cumap_, vec_out[i], vec_in[i]);
      if (fac[i] != 1.0f) {
        interp_v3_v3v3(vec_out[i], vec_in[i], vec_out[i], fac[i]);
      }
    }
  }
};

static void sh_node_curve_vec_build_multi_function(
    blender::nodes::NodeMultiFunctionBuilder &builder)
{
  bNode &bnode = builder.node();
  CurveMapping *cumap = (CurveMapping *)bnode.storage;
  BKE_curvemapping_init(cumap);
  builder.construct_and_set_matching_fn<CurveVecFunction>(*cumap);
}

void register_node_type_sh_curve_vec(void)
{
  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_CURVE_VEC, "Vector Curves", NODE_CLASS_OP_VECTOR, 0);
  node_type_socket_templates(&ntype, sh_node_curve_vec_in, sh_node_curve_vec_out);
  node_type_init(&ntype, node_shader_init_curve_vec);
  node_type_size_preset(&ntype, NODE_SIZE_LARGE);
  node_type_storage(&ntype, "CurveMapping", node_free_curves, node_copy_curves);
  node_type_exec(&ntype, node_initexec_curves, nullptr, node_shader_exec_curve_vec);
  node_type_gpu(&ntype, gpu_shader_curve_vec);
  ntype.build_multi_function = sh_node_curve_vec_build_multi_function;

  nodeRegisterType(&ntype);
}

/* **************** CURVE RGB  ******************** */
static bNodeSocketTemplate sh_node_curve_rgb_in[] = {
    {SOCK_FLOAT, N_("Fac"), 1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, PROP_FACTOR},
    {SOCK_RGBA, N_("Color"), 0.0f, 0.0f, 0.0f, 1.0f},
    {-1, ""},
};

static bNodeSocketTemplate sh_node_curve_rgb_out[] = {
    {SOCK_RGBA, N_("Color")},
    {-1, ""},
};

static void node_shader_exec_curve_rgb(void *UNUSED(data),
                                       int UNUSED(thread),
                                       bNode *node,
                                       bNodeExecData *UNUSED(execdata),
                                       bNodeStack **in,
                                       bNodeStack **out)
{
  float vec[3];
  float fac;

  /* stack order input:  vec */
  /* stack order output: vec */
  nodestack_get_vec(&fac, SOCK_FLOAT, in[0]);
  nodestack_get_vec(vec, SOCK_VECTOR, in[1]);
  BKE_curvemapping_evaluateRGBF((CurveMapping *)node->storage, out[0]->vec, vec);
  if (fac != 1.0f) {
    interp_v3_v3v3(out[0]->vec, vec, out[0]->vec, fac);
  }
}

static void node_shader_init_curve_rgb(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->storage = BKE_curvemapping_add(4, 0.0f, 0.0f, 1.0f, 1.0f);
}

static int gpu_shader_curve_rgb(GPUMaterial *mat,
                                bNode *node,
                                bNodeExecData *UNUSED(execdata),
                                GPUNodeStack *in,
                                GPUNodeStack *out)
{
  float *array, layer;
  int size;
  bool use_opti = true;

  CurveMapping *cumap = (CurveMapping *)node->storage;

  BKE_curvemapping_init(cumap);
  BKE_curvemapping_table_RGBA(cumap, &array, &size);
  GPUNodeLink *tex = GPU_color_band(mat, size, array, &layer);

  float ext_rgba[4][4];
  float range_rgba[4];

  for (int a = 0; a < CM_TOT; a++) {
    const CurveMap *cm = &cumap->cm[a];
    ext_rgba[a][0] = cm->mintable;
    ext_rgba[a][2] = cm->maxtable;
    range_rgba[a] = 1.0f / max_ff(1e-8f, cm->maxtable - cm->mintable);
    /* Compute extrapolation gradients. */
    if ((cumap->flag & CUMA_EXTEND_EXTRAPOLATE) != 0) {
      ext_rgba[a][1] = (cm->ext_in[0] != 0.0f) ?
                           (cm->ext_in[1] / (cm->ext_in[0] * range_rgba[a])) :
                           1e8f;
      ext_rgba[a][3] = (cm->ext_out[0] != 0.0f) ?
                           (cm->ext_out[1] / (cm->ext_out[0] * range_rgba[a])) :
                           1e8f;
    }
    else {
      ext_rgba[a][1] = 0.0f;
      ext_rgba[a][3] = 0.0f;
    }

    /* Check if rgb comps are just linear. */
    if (a < 3) {
      if (range_rgba[a] != 1.0f || ext_rgba[a][1] != 1.0f || ext_rgba[a][2] != 1.0f ||
          cm->totpoint != 2 || cm->curve[0].x != 0.0f || cm->curve[0].y != 0.0f ||
          cm->curve[1].x != 1.0f || cm->curve[1].y != 1.0f) {
        use_opti = false;
      }
    }
  }

  if (use_opti) {
    return GPU_stack_link(mat,
                          node,
                          "curves_rgb_opti",
                          in,
                          out,
                          tex,
                          GPU_constant(&layer),
                          GPU_uniform(range_rgba),
                          GPU_uniform(ext_rgba[3]));
  }

  return GPU_stack_link(mat,
                        node,
                        "curves_rgb",
                        in,
                        out,
                        tex,
                        GPU_constant(&layer),
                        GPU_uniform(range_rgba),
                        GPU_uniform(ext_rgba[0]),
                        GPU_uniform(ext_rgba[1]),
                        GPU_uniform(ext_rgba[2]),
                        GPU_uniform(ext_rgba[3]));
}

class CurveRGBFunction : public blender::fn::MultiFunction {
 private:
  const CurveMapping &cumap_;

 public:
  CurveRGBFunction(const CurveMapping &cumap) : cumap_(cumap)
  {
    static blender::fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static blender::fn::MFSignature create_signature()
  {
    blender::fn::MFSignatureBuilder signature{"Curve RGB"};
    signature.single_input<float>("Fac");
    signature.single_input<blender::ColorGeometry4f>("Color");
    signature.single_output<blender::ColorGeometry4f>("Color");
    return signature.build();
  }

  void call(blender::IndexMask mask,
            blender::fn::MFParams params,
            blender::fn::MFContext UNUSED(context)) const override
  {
    const blender::VArray<float> &fac = params.readonly_single_input<float>(0, "Fac");
    const blender::VArray<blender::ColorGeometry4f> &col_in =
        params.readonly_single_input<blender::ColorGeometry4f>(1, "Color");
    blender::MutableSpan<blender::ColorGeometry4f> col_out =
        params.uninitialized_single_output<blender::ColorGeometry4f>(2, "Color");

    for (int64_t i : mask) {
      BKE_curvemapping_evaluateRGBF(&cumap_, col_out[i], col_in[i]);
      if (fac[i] != 1.0f) {
        interp_v3_v3v3(col_out[i], col_in[i], col_out[i], fac[i]);
      }
    }
  }
};

static void sh_node_curve_rgb_build_multi_function(
    blender::nodes::NodeMultiFunctionBuilder &builder)
{
  bNode &bnode = builder.node();
  CurveMapping *cumap = (CurveMapping *)bnode.storage;
  BKE_curvemapping_init(cumap);
  builder.construct_and_set_matching_fn<CurveRGBFunction>(*cumap);
}

void register_node_type_sh_curve_rgb(void)
{
  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_CURVE_RGB, "RGB Curves", NODE_CLASS_OP_COLOR, 0);
  node_type_socket_templates(&ntype, sh_node_curve_rgb_in, sh_node_curve_rgb_out);
  node_type_init(&ntype, node_shader_init_curve_rgb);
  node_type_size_preset(&ntype, NODE_SIZE_LARGE);
  node_type_storage(&ntype, "CurveMapping", node_free_curves, node_copy_curves);
  node_type_exec(&ntype, node_initexec_curves, nullptr, node_shader_exec_curve_rgb);
  node_type_gpu(&ntype, gpu_shader_curve_rgb);
  ntype.build_multi_function = sh_node_curve_rgb_build_multi_function;

  nodeRegisterType(&ntype);
}
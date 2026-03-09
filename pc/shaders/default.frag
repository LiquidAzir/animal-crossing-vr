#version 330 core
in vec4 v_color;
in vec2 v_texcoord0;
in vec2 v_texcoord1;
in vec3 v_normal;
in float v_fog_z;

uniform int u_fog_type;
uniform float u_fog_start;
uniform float u_fog_end;
uniform vec4 u_fog_color;

/* TEV registers: PREV, REG0, REG1(=PRIM), REG2(=ENV) */
uniform vec4 u_tev_prev;
uniform vec4 u_tev_reg0;
uniform vec4 u_tev_reg1;
uniform vec4 u_tev_reg2;

/* TEV stage inputs (x=a, y=b, z=c, w=d) */
uniform ivec4 u_tev0_color_in;
uniform ivec4 u_tev0_alpha_in;
uniform int u_tev0_color_op;
uniform int u_tev0_alpha_op;
uniform ivec4 u_tev1_color_in;
uniform ivec4 u_tev1_alpha_in;
uniform int u_tev1_color_op;
uniform int u_tev1_alpha_op;
uniform ivec4 u_tev2_color_in;
uniform ivec4 u_tev2_alpha_in;
uniform int u_tev2_color_op;
uniform int u_tev2_alpha_op;

uniform int u_num_tev_stages;
uniform sampler2D u_texture0;
uniform sampler2D u_texture1;
uniform sampler2D u_texture2;
uniform int u_use_texture0;
uniform int u_use_texture1;
uniform int u_use_texture2;
uniform int u_tev0_tc_src;  /* texcoord index (0 or 1) per stage */
uniform int u_tev1_tc_src;
uniform int u_tev2_tc_src;

/* Lighting / color channels */
uniform int u_lighting_enabled;
uniform vec4 u_mat_color;
uniform vec4 u_amb_color;
uniform int u_chan_mat_src;   /* 0=REG, 1=VTX */
uniform int u_chan_amb_src;
uniform int u_num_chans;
uniform int u_alpha_lighting_enabled;
uniform int u_alpha_mat_src;
uniform int u_light_mask;
uniform vec3 u_light_pos[8];
uniform vec4 u_light_color[8];

/* KONST colors and per-stage selection */
uniform vec4 u_kcolor[4];
uniform ivec3 u_tev_ksel[3];   /* x=kcolor_sel, y=kalpha_sel */

/* Per-stage bias/scale: x=color_bias, y=color_scale, z=alpha_bias, w=alpha_scale */
uniform ivec4 u_tev0_bsc;
uniform ivec4 u_tev1_bsc;
uniform ivec4 u_tev2_bsc;
/* Per-stage output: x=color_clamp, y=alpha_clamp, z=color_out_reg, w=alpha_out_reg */
uniform ivec4 u_tev0_out;
uniform ivec4 u_tev1_out;
uniform ivec4 u_tev2_out;
/* Swap tables (channel remapping) and per-stage swap selection */
uniform ivec4 u_swap_table[4];
uniform ivec2 u_tev0_swap;
uniform ivec2 u_tev1_swap;
uniform ivec2 u_tev2_swap;

/* Alpha compare */
uniform int u_alpha_comp0;
uniform int u_alpha_ref0;
uniform int u_alpha_op;
uniform int u_alpha_comp1;
uniform int u_alpha_ref1;

/* Indirect textures */
uniform int u_num_ind_stages;
uniform sampler2D u_ind_tex0;
uniform sampler2D u_ind_tex1;
uniform vec2 u_ind_scale[4];
uniform vec3 u_ind_mtx_r0[3];
uniform vec3 u_ind_mtx_r1[3];
uniform ivec4 u_tev0_ind_cfg;  /* x=ind_stage, y=ind_mtx, z=ind_bias, w=ind_alpha */
uniform ivec4 u_tev1_ind_cfg;
uniform ivec4 u_tev2_ind_cfg;
uniform ivec3 u_tev0_ind_wrap; /* x=wrap_s, y=wrap_t, z=add_prev */
uniform ivec3 u_tev1_ind_wrap;
uniform ivec3 u_tev2_ind_wrap;

out vec4 fragColor;

/* GXTevKColorSel enum: 0-7=constant fractions, 12-15=K0-K3 RGB, 16+=K0-K3 single channels */
vec3 getKonstC(int sel) {
    if (sel <= 7) {
        float v = float(8 - sel) / 8.0;
        return vec3(v);
    }
    if (sel <= 11) return vec3(0.0);
    if (sel == 12) return u_kcolor[0].rgb;
    if (sel == 13) return u_kcolor[1].rgb;
    if (sel == 14) return u_kcolor[2].rgb;
    if (sel == 15) return u_kcolor[3].rgb;
    int ki = (sel - 16) & 3;
    int ch = (sel - 16) >> 2;
    float c = (ch == 0) ? u_kcolor[ki].r :
              (ch == 1) ? u_kcolor[ki].g :
              (ch == 2) ? u_kcolor[ki].b : u_kcolor[ki].a;
    return vec3(c);
}

/* GXTevKAlphaSel enum */
float getKonstA(int sel) {
    if (sel <= 7) return float(8 - sel) / 8.0;
    if (sel <= 15) return 0.0;
    int ki = (sel - 16) & 3;
    int ch = (sel - 16) >> 2;
    return (ch == 0) ? u_kcolor[ki].r :
           (ch == 1) ? u_kcolor[ki].g :
           (ch == 2) ? u_kcolor[ki].b : u_kcolor[ki].a;
}

/* GXTevColorArg enum (0-15) */
vec3 getTevC(int id, vec4 prev, vec4 tex, vec4 ras,
             vec4 r0, vec4 r1, vec4 r2, vec3 konst) {
    if (id == 0)  return prev.rgb;       /* CPREV */
    if (id == 1)  return vec3(prev.a);   /* APREV */
    if (id == 2)  return r0.rgb;         /* C0 */
    if (id == 3)  return vec3(r0.a);     /* A0 */
    if (id == 4)  return r1.rgb;         /* C1 */
    if (id == 5)  return vec3(r1.a);     /* A1 */
    if (id == 6)  return r2.rgb;         /* C2 */
    if (id == 7)  return vec3(r2.a);     /* A2 */
    if (id == 8)  return tex.rgb;        /* TEXC */
    if (id == 9)  return vec3(tex.a);    /* TEXA */
    if (id == 10) return ras.rgb;        /* RASC */
    if (id == 11) return vec3(ras.a);    /* RASA */
    if (id == 12) return vec3(1.0);      /* ONE */
    if (id == 13) return vec3(0.5);      /* HALF */
    if (id == 14) return konst;          /* KONST */
    return vec3(0.0);                    /* ZERO */
}

/* GXTevAlphaArg enum (0-7) */
float getTevA(int id, float prev, float tex, float ras,
              float r0, float r1, float r2, float konst) {
    if (id == 0) return prev;    /* APREV */
    if (id == 1) return r0;     /* A0 */
    if (id == 2) return r1;     /* A1 */
    if (id == 3) return r2;     /* A2 */
    if (id == 4) return tex;    /* TEXA */
    if (id == 5) return ras;    /* RASA */
    if (id == 6) return konst;  /* KONST */
    return 0.0;                 /* ZERO */
}

vec4 applySwap(vec4 v, ivec4 sw) {
    return vec4(v[sw.x], v[sw.y], v[sw.z], v[sw.w]);
}

/* TEV formula: d OP ((1-c)*a + c*b) */
vec4 tevStage(ivec4 cin, int cop, ivec4 ain, int aop,
              vec4 prev, vec4 tex, vec4 ras,
              vec4 r0, vec4 r1, vec4 r2,
              vec3 konstC, float konstA) {
    vec3 ca = getTevC(cin.x, prev, tex, ras, r0, r1, r2, konstC);
    vec3 cb = getTevC(cin.y, prev, tex, ras, r0, r1, r2, konstC);
    vec3 cc = getTevC(cin.z, prev, tex, ras, r0, r1, r2, konstC);
    vec3 cd = getTevC(cin.w, prev, tex, ras, r0, r1, r2, konstC);
    vec3 blend = mix(ca, cb, cc);
    vec3 cResult;
    if (cop == 1) { cResult = cd - blend; }
    else          { cResult = cd + blend; }

    float aa = getTevA(ain.x, prev.a, tex.a, ras.a, r0.a, r1.a, r2.a, konstA);
    float ab = getTevA(ain.y, prev.a, tex.a, ras.a, r0.a, r1.a, r2.a, konstA);
    float ac = getTevA(ain.z, prev.a, tex.a, ras.a, r0.a, r1.a, r2.a, konstA);
    float ad = getTevA(ain.w, prev.a, tex.a, ras.a, r0.a, r1.a, r2.a, konstA);
    float aBlend = mix(aa, ab, ac);
    float aResult;
    if (aop == 1) { aResult = ad - aBlend; }
    else          { aResult = ad + aBlend; }

    return vec4(cResult, aResult);
}

/* bias: 0=none, 1=+0.5, 2=-0.5; scale: 0=x1, 1=x2, 2=x4, 3=x0.5 */
vec4 applyBSC(vec4 v, ivec4 bsc) {
    if (bsc.x == 1) v.rgb += 0.5;
    else if (bsc.x == 2) v.rgb -= 0.5;
    if (bsc.y == 1) v.rgb *= 2.0;
    else if (bsc.y == 2) v.rgb *= 4.0;
    else if (bsc.y == 3) v.rgb *= 0.5;
    if (bsc.z == 1) v.a += 0.5;
    else if (bsc.z == 2) v.a -= 0.5;
    if (bsc.w == 1) v.a *= 2.0;
    else if (bsc.w == 2) v.a *= 4.0;
    else if (bsc.w == 3) v.a *= 0.5;
    return v;
}

/* out_cfg.zw selects target register: 0=PREV, 1=REG0, 2=REG1, 3=REG2 */
void writeToReg(vec4 val, ivec4 out_cfg,
                inout vec4 prev, inout vec4 r0, inout vec4 r1, inout vec4 r2) {
    vec3 rgb = (out_cfg.x != 0) ? clamp(val.rgb, 0.0, 1.0) : val.rgb;
    float a  = (out_cfg.y != 0) ? clamp(val.a,   0.0, 1.0) : val.a;
    if (out_cfg.z == 0) prev.rgb = rgb;
    else if (out_cfg.z == 1) r0.rgb = rgb;
    else if (out_cfg.z == 2) r1.rgb = rgb;
    else r2.rgb = rgb;
    if (out_cfg.w == 0) prev.a = a;
    else if (out_cfg.w == 1) r0.a = a;
    else if (out_cfg.w == 2) r1.a = a;
    else r2.a = a;
}

vec3 sampleIndTex(int ind_stage, vec2 tc0, vec2 tc1) {
    vec2 tc = (ind_stage == 0) ? tc0 : tc1;
    vec4 s = (ind_stage == 0) ? texture(u_ind_tex0, tc) : texture(u_ind_tex1, tc);
    return s.rgb;
}

vec2 applyIndirect(ivec4 ind_cfg, ivec3 ind_wrap, vec2 coord,
                   vec2 prev_offset, vec2 tc0, vec2 tc1) {
    int ind_mtx_id = ind_cfg.y;
    if (ind_mtx_id == 0) return coord;  /* GX_ITM_OFF */

    vec3 indSample = sampleIndTex(ind_cfg.x, tc0, tc1);

    /* Bias: shift [0,1] to [-0.5,0.5] per axis (GX_ITB bitmask: S=1, T=2, U=4) */
    int bias = ind_cfg.z;
    vec3 biased = indSample;
    if ((bias & 1) != 0) biased.x -= 0.5;
    if ((bias & 2) != 0) biased.y -= 0.5;
    if ((bias & 4) != 0) biased.z -= 0.5;

    /* GX_ITM 1-3=full 2D, 5-7=S-only, 9-11=T-only */
    vec2 offset = vec2(0.0);
    int idx;
    if (ind_mtx_id >= 1 && ind_mtx_id <= 3) {
        idx = ind_mtx_id - 1;
        offset.x = dot(u_ind_mtx_r0[idx], biased);
        offset.y = dot(u_ind_mtx_r1[idx], biased);
    } else if (ind_mtx_id >= 5 && ind_mtx_id <= 7) {
        idx = ind_mtx_id - 5;
        offset.x = dot(u_ind_mtx_r0[idx], biased);
        offset.y = 0.0;
    } else if (ind_mtx_id >= 9 && ind_mtx_id <= 11) {
        idx = ind_mtx_id - 9;
        offset.x = 0.0;
        offset.y = dot(u_ind_mtx_r1[idx], biased);
    }

    if (ind_wrap.z != 0) offset += prev_offset;

    /* Wrap base texcoord: 0=off, 1-5=256..16 texels, 6=zero.
       Uses floor-based wrap instead of mod() to handle negative inputs. */
    vec2 wrappedCoord = coord;
    if (ind_wrap.x == 6) wrappedCoord.x = 0.0;
    else if (ind_wrap.x >= 1 && ind_wrap.x <= 5) {
        float wrapSize = float(1 << (9 - ind_wrap.x));  /* 256,128,64,32,16 */
        float w = coord.x * 256.0;
        wrappedCoord.x = (w - wrapSize * floor(w / wrapSize)) / 256.0;
    }
    if (ind_wrap.y == 6) wrappedCoord.y = 0.0;
    else if (ind_wrap.y >= 1 && ind_wrap.y <= 5) {
        float wrapSize = float(1 << (9 - ind_wrap.y));
        float w = coord.y * 256.0;
        wrappedCoord.y = (w - wrapSize * floor(w / wrapSize)) / 256.0;
    }

    return wrappedCoord + offset;
}

/* EQUAL/NEQUAL use epsilon for float precision after interpolation */
bool alphaTest(int comp, float val, float ref) {
    const float EPS = 0.5 / 255.0;
    if (comp == 0) return false;                   /* NEVER */
    if (comp == 1) return val < ref;               /* LESS */
    if (comp == 2) return abs(val - ref) < EPS;    /* EQUAL */
    if (comp == 3) return val <= ref;              /* LEQUAL */
    if (comp == 4) return val > ref;               /* GREATER */
    if (comp == 5) return abs(val - ref) >= EPS;   /* NEQUAL */
    if (comp == 6) return val >= ref;              /* GEQUAL */
    return true;                                   /* ALWAYS */
}

void main() {
    vec2 tc0 = v_texcoord0;
    vec2 tc1 = v_texcoord1;
    vec2 ind_prev_offset = vec2(0.0);

    /* Per-stage texcoord with indirect offset */
    vec2 stc0 = (u_tev0_tc_src == 0) ? tc0 : tc1;
    if (u_num_ind_stages > 0 && u_tev0_ind_cfg.y != 0) {
        stc0 = applyIndirect(u_tev0_ind_cfg, u_tev0_ind_wrap, stc0, ind_prev_offset, tc0, tc1);
        ind_prev_offset = stc0 - ((u_tev0_tc_src == 0) ? tc0 : tc1);
    }
    vec2 stc1 = (u_tev1_tc_src == 0) ? tc0 : tc1;
    if (u_num_ind_stages > 0 && u_num_tev_stages > 1 && u_tev1_ind_cfg.y != 0) {
        stc1 = applyIndirect(u_tev1_ind_cfg, u_tev1_ind_wrap, stc1, ind_prev_offset, tc0, tc1);
        ind_prev_offset = stc1 - ((u_tev1_tc_src == 0) ? tc0 : tc1);
    }
    vec2 stc2 = (u_tev2_tc_src == 0) ? tc0 : tc1;
    if (u_num_ind_stages > 0 && u_num_tev_stages > 2 && u_tev2_ind_cfg.y != 0) {
        stc2 = applyIndirect(u_tev2_ind_cfg, u_tev2_ind_wrap, stc2, ind_prev_offset, tc0, tc1);
        ind_prev_offset = stc2 - ((u_tev2_tc_src == 0) ? tc0 : tc1);
    }

    vec4 texColor0 = vec4(1.0);
    vec4 texColor1 = vec4(1.0);
    vec4 texColor2 = vec4(1.0);
    if (u_use_texture0 != 0) texColor0 = texture(u_texture0, stc0);
    if (u_use_texture1 != 0) texColor1 = texture(u_texture1, stc1);
    if (u_use_texture2 != 0) texColor2 = texture(u_texture2, stc2);

    /* Rasterized color: GX lighting model */
    vec4 rasColor;
    if (u_num_chans == 0) {
        rasColor = vec4(1.0);
    } else {
        vec3 matC = (u_chan_mat_src != 0) ? v_color.rgb : u_mat_color.rgb;
        vec3 ambC = (u_chan_amb_src != 0) ? v_color.rgb : u_amb_color.rgb;
        if (u_lighting_enabled != 0) {
            vec3 lightAccum = ambC;
            for (int i = 0; i < 8; i++) {
                if ((u_light_mask & (1 << i)) != 0) {
                    vec3 L = normalize(u_light_pos[i]);
                    float diff = clamp(dot(v_normal, L), 0.0, 1.0);
                    lightAccum += diff * u_light_color[i].rgb;
                }
            }
            rasColor.rgb = matC * clamp(lightAccum, 0.0, 1.0);
        } else {
            rasColor.rgb = matC;
        }
        float matA = (u_alpha_mat_src != 0) ? v_color.a : u_mat_color.a;
        if (u_alpha_lighting_enabled != 0) {
            rasColor.a = matA * u_amb_color.a;
        } else {
            rasColor.a = matA;
        }
    }

    vec4 prev = u_tev_prev;
    vec4 r0 = u_tev_reg0;
    vec4 r1 = u_tev_reg1;
    vec4 r2 = u_tev_reg2;

    /* TEV stage 0 */
    {
        vec4 sTex = applySwap(texColor0, u_swap_table[u_tev0_swap.y]);
        vec4 sRas = applySwap(rasColor,  u_swap_table[u_tev0_swap.x]);
        vec3 kc0 = getKonstC(u_tev_ksel[0].x);
        float ka0 = getKonstA(u_tev_ksel[0].y);
        vec4 s0 = tevStage(u_tev0_color_in, u_tev0_color_op,
                            u_tev0_alpha_in, u_tev0_alpha_op,
                            prev, sTex, sRas, r0, r1, r2, kc0, ka0);
        s0 = applyBSC(s0, u_tev0_bsc);
        writeToReg(s0, u_tev0_out, prev, r0, r1, r2);
    }

    /* TEV stage 1 */
    if (u_num_tev_stages > 1) {
        vec4 sTex = applySwap(texColor1, u_swap_table[u_tev1_swap.y]);
        vec4 sRas = applySwap(rasColor,  u_swap_table[u_tev1_swap.x]);
        vec3 kc1 = getKonstC(u_tev_ksel[1].x);
        float ka1 = getKonstA(u_tev_ksel[1].y);
        vec4 s1 = tevStage(u_tev1_color_in, u_tev1_color_op,
                            u_tev1_alpha_in, u_tev1_alpha_op,
                            prev, sTex, sRas, r0, r1, r2, kc1, ka1);
        s1 = applyBSC(s1, u_tev1_bsc);
        writeToReg(s1, u_tev1_out, prev, r0, r1, r2);
    }

    /* TEV stage 2 */
    if (u_num_tev_stages > 2) {
        vec4 sTex = applySwap(texColor2, u_swap_table[u_tev2_swap.y]);
        vec4 sRas = applySwap(rasColor,  u_swap_table[u_tev2_swap.x]);
        vec3 kc2 = getKonstC(u_tev_ksel[2].x);
        float ka2 = getKonstA(u_tev_ksel[2].y);
        vec4 s2 = tevStage(u_tev2_color_in, u_tev2_color_op,
                            u_tev2_alpha_in, u_tev2_alpha_op,
                            prev, sTex, sRas, r0, r1, r2, kc2, ka2);
        s2 = applyBSC(s2, u_tev2_bsc);
        writeToReg(s2, u_tev2_out, prev, r0, r1, r2);
    }

    /* Alpha compare */
    if (u_alpha_comp0 != 7 || u_alpha_comp1 != 7) {
        float ref0 = float(u_alpha_ref0) / 255.0;
        float ref1 = float(u_alpha_ref1) / 255.0;
        bool pass0 = alphaTest(u_alpha_comp0, prev.a, ref0);
        bool pass1 = alphaTest(u_alpha_comp1, prev.a, ref1);
        bool pass;
        if (u_alpha_op == 0)      pass = pass0 && pass1;
        else if (u_alpha_op == 1) pass = pass0 || pass1;
        else if (u_alpha_op == 2) pass = pass0 != pass1;
        else                      pass = pass0 == pass1;
        if (!pass) discard;
    }

    fragColor = prev;

    /* Fog */
    if (u_fog_type != 0) {
        float fog_denom = max(u_fog_end - u_fog_start, 1e-6);
        float fog_factor = clamp((v_fog_z - u_fog_start) / fog_denom, 0.0, 1.0);
        fragColor.rgb = mix(fragColor.rgb, u_fog_color.rgb, fog_factor);
    }
}

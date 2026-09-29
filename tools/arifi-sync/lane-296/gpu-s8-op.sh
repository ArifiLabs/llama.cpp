#!/usr/bin/env bash
# lane-296c stage 8 op level: bias decomposition of the float path (H-A) and the f32-accumulation witness.
# PRICE: about 15-40 s x 11 runs = about 5 min; memory under 1 GB; blocks the GPU slot for that time.
# PRE-REGISTERED: float path systematic if |1-beta| >= 5e-3 (most of its 7.7e-3), random if |1-beta| < 1e-3.
#   Witness: off + GGML_ARIFI_F32ACC=ssm_out on ssm_out L0 drops from 7.66e-3 toward about 2e-4 (f16-act sim 1.88e-4).
set -u
N=/c/ArifiLabs/cache/lane296/night; C=$N/ev12/cap
SX8=C:/ArifiLabs/models/hf/marlalabsAI/Qwen3.8-27B-SX8/Qwen3.8-27B-SX8v43-id57.gguf
for t in ssm_out:0 ssm_out:16 ssm_out:32 ssm_out:48 ssm_out:62 attn_output:31; do r=${t%%:*}; l=${t##*:}; w=blk.$l.$r.weight
  $N/probe-run.sh s8-off-$r-$l GGML_ARIFI_Q8_0_CM1=off -- replay off $SX8 $w $C/$w.x
done
for t in ssm_out:0 ssm_out:62 attn_output:31; do r=${t%%:*}; l=${t##*:}; w=blk.$l.$r.weight
  $N/probe-run.sh s8-f32acc-$r-$l GGML_ARIFI_Q8_0_CM1=off GGML_ARIFI_F32ACC=ssm_out,attn_output -- replay f32acc $SX8 $w $C/$w.x
done
for l in 0 62; do w=blk.$l.ssm_out.weight
  $N/probe-run.sh s8-2d8-ssm_out-$l GGML_ARIFI_Q8_0_CM1=on GGML_ARIFI_Q8_0_CM1_2D=ssm_out -- replay 2d-s8 $SX8 $w $C/$w.x
done
echo "done $(date -Is)" > $N/int8goal-s8op.done

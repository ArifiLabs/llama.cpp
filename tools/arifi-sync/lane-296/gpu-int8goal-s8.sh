#!/usr/bin/env bash
# lane-296c int8 goal STAGE 8: why exact ssm_out scores worse. CORRECTNESS, not timed. Marker cache/GPU-BUSY-lane296c.marker;
# pid 13696 (PAUSED CPU leg) and GPU-BUSY-lane296.marker (09:05, own lane's old leg) ignored as in every lane-296c chain.
# PRICE: 3 x 64-chunk S-X8 legs about 20 min each + 1 x 16-chunk S-X8 KL leg about 7 min + (if the bar holds) Q4 and GSQ
#        16-chunk KL legs about 4 min each = about 75 min; memory held about 26 GB; blocks the GPU slot.
# Binary: b-int8c relinked 17:4x with GGML_ARIFI_F32ACC (float-path f32 accumulation by weight role, default off; the
#         default path is the pre-S8 path, the knob only changes prec for listed roles).
# Op level already CHECKED (ev12/s8-*): float path (f16 accumulation) ssm_out error 7.66e-3..7.93e-3, random, not a bias
#   (|1-beta| <= 2.6e-4, share along ref <= 0.1%); f32 accumulation 2.66e-4 (L0), 2.80e-4 (L62); B5 1.38e-4 / 4.5e-5.
# PRE-REGISTERED (written before launch; pairs with ev14 a64 6.5390 / b564 6.5511, 64 chunks, same text and seed):
#   c64 = A + ssm_out f32 accumulation (float path made exact, no int8 on ssm_out).
#     c64 - a64 >= +0.009 (2 SE): exactness of ssm_out itself costs PPL (not an int8 property).
#     |c64 - a64| < 0.009 and b564 - c64 >= +0.009: B5's int8 path carries an in-graph effect the op witnesses missed.
#     c64 - a64 <= -0.009: exact ssm_out HELPS; then B5's +0.0121 is int8-path specific (same next step as line 2).
#   d64 = candidate: B5 (int8 on every Q8_0 role, ssm_out two digits) + f32 accumulation on every remaining float path.
#     PASS if chunk-16 running PPL <= 7.4163 and d64 <= r64 (r86i, 64 chunks). Q4 + GSQ no-harm legs run only if the
#     chunk-16 value holds 7.4163 (bars: Q4 <= r86i 7.4918, GSQ <= r86i 7.5549).
#   x16 = exact-arithmetic reference: int8 off + f32 accumulation everywhere, KL vs r86i base. Prediction from the
#     2-chunk smoke (4.2685 / 6.0557 vs off 4.3127 / 6.0863): PPL below off 7.408170.
# Done file: night/int8goal-s8.done ; per-leg night/int8goal-s8-<leg>.done ; evidence ev15s8/
set -u
S=${STUDIO_ROOT:?set STUDIO_ROOT to the studio root (receipts name the original machine paths)}
N=$S/cache/lane296/night; EV=$N/ev15s8; mkdir -p $EV; W=$EV/window.txt
export PATH="${MINGW_BIN:+$MINGW_BIN:}$PATH"  # WinLibs POSIX UCRT LLVM mingw64/bin
export ARIFI_GPU_BUDGET_GIB=26
MK=$S/cache/GPU-BUSY-lane296c.marker; FB=$N/b-int8c/bin; RB=$S/models/engines/arifi-b10825-r86i-6fb7a425a
SX8=$S/models/hf/marlalabsAI/Qwen3.8-27B-SX8/Qwen3.8-27B-SX8v43-id57.gguf
Q4=$S/models/hf/huihui-ai/Huihui-Qwen3.8-27B-abliterated-UD-GGUF/Huihui-Qwen3.8-27B-abliterated-UD-Q4_K_XL.gguf
GSQ=$S/models/hf/ISTA-DASLab/Qwen3.8-27B-GSQ-RCO-GGUF/Qwen3.8-27B-GSQ-RCO-IQ3_S-mtp.gguf
BQ4=$N/kl/base-Q4-R.kld; BSN=$N/kl/base-SX8-R-nf.kld; BG=$N/kl/base-GSQ-R.kld
WT=$S/cache/r53-pca-wt/r53-evidence/wikitext/wikitext-2-raw/wiki.test.raw
CLR="-u GGML_ARIFI_Q8_0_CM1 -u GGML_ARIFI_Q8_0_CM1_ONLY -u GGML_ARIFI_Q8_0_CM1_SKIP -u GGML_ARIFI_Q8_1_SCALE -u GGML_ARIFI_Q8_0_CM1_2D -u GGML_ARIFI_Q8_0_CM1_2D_SHIFT -u GGML_ARIFI_F32ACC"
CAND="GGML_ARIFI_Q8_0_CM1=on GGML_ARIFI_Q8_0_CM1_2D=ssm_out GGML_ARIFI_F32ACC=all"
others(){ ls $S/cache/GPU-TIMED-*.marker $S/cache/GPU-BUSY-*.marker 2>/dev/null | grep -v -e 'GPU-BUSY-lane296\.marker$' -e 'GPU-BUSY-lane296c\.marker$'
          tasklist //FO CSV //NH 2>/dev/null | grep -i '"llama-' | grep -v '"13696"'; }
leg(){ n=$1; shift; [ -e $N/int8goal-s8-$n.done ] && return 0
  while [ -n "$(others)" ]; do echo "YIELD $(date -Is) $(others | tr '\n' ' ')" >> $W; sleep 60; done
  echo "lane-296c int8 goal S8 (CORRECTNESS, not timed): $n $(date -Is)" > $MK; echo "$n START $(date -Is)" >> $W
  env $CLR "$@" > $EV/$n.txt 2> $EV/$n.err < /dev/null
  rc=$?; echo "$n END $(date -Is) rc=$rc" >> $W; rm -f $MK; sleep 10
  cat $EV/$n.txt $EV/$n.err | tr -d '\r' | grep -q "Final estimate\|Mean *PPL" && echo "done $(date -Is)" > $N/int8goal-s8-$n.done; }
P64="-f $WT -ngl 999 -c 512 --chunks 64 --seed 1"
K="--kl-divergence -ngl 999 -c 512 --chunks 16 --seed 1"
echo "START $(date -Is) src-urev $(git -C $N/src-urev rev-parse --short=10 HEAD) (binary relinked from 3e85f0cb6e sources)" >> $W
leg c64 GGML_ARIFI_F32ACC=ssm_out $FB/llama-perplexity.exe -m $SX8 $P64
leg d64 $CAND $FB/llama-perplexity.exe -m $SX8 $P64
leg x16 GGML_ARIFI_Q8_0_CM1=off GGML_ARIFI_F32ACC=all $FB/llama-perplexity.exe -m $SX8 --kl-divergence-base $BSN $K
p=$(cat $EV/d64.txt $EV/d64.err 2>/dev/null | tr -d '\r' | grep -oE '\[16\][0-9]+\.[0-9]+' | head -1 | sed 's/\[16\]//')
echo "d64 chunk-16 running PPL: ${p:-missing}" >> $W
if awk -v p="$p" 'BEGIN{exit !(p != "" && p <= 7.4163)}'; then
  echo "candidate HOLDS 7.4163 at chunk 16: Q4 + GSQ legs" >> $W
  leg d-q4  $CAND $FB/llama-perplexity.exe -m $Q4  --kl-divergence-base $BQ4 $K
  leg d-gsq $CAND $FB/llama-perplexity.exe -m $GSQ --kl-divergence-base $BG  $K
else
  echo "candidate does NOT hold 7.4163 at chunk 16: no Q4 / GSQ legs" >> $W
fi
leg r64 $RB/llama-perplexity.exe -m $SX8 $P64
echo "DONE $(date -Is)" >> $W; echo "done $(date -Is)" > $N/int8goal-s8.done

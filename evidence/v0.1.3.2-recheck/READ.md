# v0.1.3.2 recheck vs v0.1.3.0, 2026-10-11 (Radeon 890M, Minisforum AI X1 Pro 470)

Side a = v0.1.3.0 build, side b = v0.1.3.2 build. Reader: `recheck_read.py`.

| model | mode | prompt | v0.1.3.0 -> v0.1.3.2 (decode t/s; prompt t/s) | greedy ids |
|---|---|---|---|---|
| fn | plain | short | decode 11.95 -> 11.95 (-0.04%, band 2.5%, tie); prompt 30.73 -> 31.26 (+1.71%, band 2.5%, tie) | ids same |
| fn | plain | long | decode 11.82 -> 11.83 (+0.12%, band 2.9%, tie); prompt 152.13 -> 148.45 (-2.42%, band 6.2%, tie) | ids same |
| fn | MTP n3 | short | decode 19.93 -> 20.03 (+0.48%, band 2.1%, tie); prompt 30.38 -> 30.37 (-0.02%, band 2.9%, tie) | ids same |
| fn | MTP n3 | long | decode 18.08 -> 18.23 (+0.84%, band 1.1%, tie); prompt 143.73 -> 145.77 (+1.43%, band 14.0%, tie) | ids same |
| fn | MTP n3 + 40K vocab | short | decode 20.46 -> 20.48 (+0.08%, band 2.1%, tie); prompt 30.00 -> 30.40 (+1.35%, band 2.8%, tie) | ids same |
| fn | MTP n3 + 40K vocab | long | decode 19.69 -> 19.84 (+0.75%, band 1.0%, tie); prompt 145.53 -> 145.96 (+0.30%, band 8.0%, tie) | ids same |
| orn | plain | short | decode 29.65 -> 29.61 (-0.13%, band 1.3%, tie); prompt 58.10 -> 57.03 (-1.84%, band 8.9%, tie) | ids same |
| orn | plain | long | decode 29.16 -> 29.16 (+0.01%, band 1.0%, tie); prompt 418.16 -> 420.93 (+0.66%, band 14.2%, tie) | ids same |
| orn | MTP n3 | short | decode 34.07 -> 34.37 (+0.87%, band 9.4%, tie); prompt 57.43 -> 56.84 (-1.03%, band 5.0%, tie) | ids diff@[None, 53, None, 53] |
| orn | MTP n3 | long | decode 35.49 -> 35.15 (-0.96%, band 1.8%, tie); prompt 415.48 -> 415.34 (-0.03%, band 9.4%, tie) | ids same |
| orn | DFlash2 | short | decode 32.48 -> 32.38 (-0.31%, band 1.5%, tie); prompt 59.81 -> 59.27 (-0.89%, band 2.3%, tie) | ids same |
| orn | DFlash2 | long | decode 33.76 -> 33.67 (-0.24%, band 1.0%, tie); prompt 390.07 -> 397.79 (+1.98%, band 20.9%, tie) | ids same |
| q27 | plain | short | decode 4.51 -> 4.57 (+1.33%, band 4.2%, tie); prompt 18.18 -> 18.05 (-0.75%, band 12.6%, tie) | ids same |
| q27 | plain | long | decode 4.53 -> 4.52 (-0.14%, band 1.9%, tie); prompt 104.87 -> 105.47 (+0.58%, band 4.8%, tie) | ids same |
| q27 | MTP n3 | short | decode 10.05 -> 9.97 (-0.75%, band 6.5%, tie); prompt 16.49 -> 16.55 (+0.39%, band 15.9%, tie) | ids same |
| q27 | MTP n3 | long | decode 9.76 -> 9.59 (-1.80%, band 4.0%, tie); prompt 102.77 -> 102.03 (-0.72%, band 8.1%, tie) | ids same |
| q27 | DFlash2 | short | decode 13.86 -> 13.72 (-1.03%, band 6.7%, tie); prompt 16.10 -> 16.23 (+0.77%, band 13.5%, tie) | ids same |
| q27 | DFlash2 | long | decode 11.31 -> 11.61 (+2.69%, band 1.9%, WIN); prompt 100.78 -> 100.64 (-0.14%, band 11.1%, tie) | ids same |
| sx8 | plain | short | decode 2.94 -> 2.94 (+0.02%, band 1.0%, tie); prompt 14.36 -> 15.56 (+8.36%, band 1.0%, WIN) | ids same |
| sx8 | plain | long | decode 2.93 -> 2.93 (-0.05%, band 1.0%, tie); prompt 93.33 -> 92.93 (-0.42%, band 6.7%, tie) | ids same |
| sx8 | DFlash2 | short | decode 9.55 -> 10.96 (+14.73%, band 1.5%, WIN); prompt 13.41 -> 14.42 (+7.51%, band 2.4%, WIN) | ids same |
| sx8 | DFlash2 | long | decode 8.60 -> 9.72 (+13.07%, band 3.4%, WIN); prompt 88.74 -> 88.02 (-0.81%, band 11.4%, tie) | ids same |
VERDICT BLOCKER
 - fn plain, v0.1.3.0 README flags (mmap -c 2048): load missing/failed [('O-a1-g', 'FAILED-RAM-ALARM available 1.09 GiB (under 2 GB)'), ('O-b1-p', 'FAILED-RAM-ALARM available 1.21 GiB (under 2 GB)'), ('O-b2-p', 'FAILED-RAM-ALARM available 1.27 GiB (under 2 GB)'), ('O-a2-g', 'FAILED-RAM-ALARM available 0.56 GiB (under 2 GB)')]

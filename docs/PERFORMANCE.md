# Performance baseline — 2026-09-06

`dopa_benchmark` は16同時ボイス、1秒のaudio、waveform saw/table、3rates、PUNCH=0で壁時計時間を測る。ファイルI/Oと初期table生成を計時に含めない。CPU/OS負荷/熱による変動があるため、このPCでの参考値でありDAWのCPU表示や保証上限ではない。

| sample rate | table / before (s) | table / final SIMD run (s) |
|---|---:|---:|
| 48kHz | 1.433298 | 0.254923 |
| 96kHz | 2.710005 | 0.547487 |
| 192kHz | 5.463461 | 0.753794 |

1秒より大きいとこの条件ではリアルタイム処理に間に合わない。初回測定でこれを発見し、重複係数計算、純table時のVA生成、readerのphase/log計算を減らし、4隣接tableのcubic補間をSSEでまとめた。最終単回測定では全3rateが1秒未満。ただし別instrument併用、PUNCHあり、最大effects、低buffer等では余裕が減る。長時間/全設定のCPU保証は未完。

生データは `BENCHMARK_BEFORE.csv`, `BENCHMARK_AFTER.csv`（scalar整理後）, `BENCHMARK_SIMD.csv`。全条件を同時刻・同一負荷で測ったわけではないため、表の比率だけで厳密な最適化倍率を主張しない。

GUIはbuilt-in16frameの背景pathを一度計算し、線分をまとめて描く。audioとUIの合計負荷の実DAW測定はまだない。

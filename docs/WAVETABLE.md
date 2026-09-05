# Wavetable engine — v0.3 development

`src/core/Wavetable.h` は独自のharmonic coefficientsから16frameを生成する。2048 samples/cycle、最大512partial、10段階のoctave mipを持つ。各levelでは生成時に高域partialを取り除く。元frameのpeakに対する同じ倍率を全mipへ適用し、帯域切替で勝手に音量を再正規化しない。

再生はperiodic cubic Hermite補間、隣接frameの線形モーフ、隣接mipのguarded crossfade。mip coordinateは `log2(delta * 512 / 0.225)`。粗い側のmipと交差する直前でも細かい側の最大partialが0.45 cycles/sample以下になる。frameをモーフしても双方が安全なbandwidthに制限される。

共有bankは初回prepare/GUIアクセスで生成し、その後不変。audioではテーブルを生成せず、sample補間だけを行う。`Reader` は先に決めたframe/mipポインターと補間比率を保持する軽量viewで、所有権を持たない。周期正規化済みphaseを渡す。

x86では4つの隣接frame/mipをSSEで同時補間する。その他ではscalar fallback。両方に同じ数学的検査を適用。浮動小数点丸めのためbackend間の最下位bit一致は保証せず、同一backendでの状態再現性を試験する。

## 音源の操作

- TABLE MIX: 0=旧VA、1=wavetable。中間はlinear blend。
- FRAME: 0〜15の連続位置。
- FRAME MOTION: 共通LFOがframeへ±0.5×depthを加える。端はclamp。
- START PHASE: 次のnote-onのphase。既存VAにも適用。phase 1は0と同じ。
- WAVEタブの波形面を横dragでframe、縦dragでmix。図は実テーブルのbase-frame previewで、oscilloscopeや現在の演奏波形ではない。

## 検証

`dopa_wavetable_tests`: 全160 cycleのDC/有限値/peak/周期境界、sine基準、16高音域条件の非調波残差、frame/mip境界連続性、同じMIDI/位相条件のframe差による音声変化、v2 preset移行。

実DLLテストはWAVE面のdrag→parameter変更を確認。coreは96factory×6ratesを検査する。単一条件の残差が小さいことは、全pitch・動的frame scan・音源/FX全体のalias-freeを意味しない。

## 未完（目標から外さない）

任意wavetable import、sample→table、harmonic/FFT editor、warp、FM/PM/ring/sync、高速変調時のqualityモード、table assetを含むstate、workerでの再生成とリアルタイム安全なasset交換。現在は独自built-in bankのみ。

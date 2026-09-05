# Real-time safety

- steady-state process/tickは固定配列とprepare済みdelay bufferだけを使用。
- wavetable bankはprepare/GUIで一度生成して共有。read-only readerの補間はaudio上で確保しない。note frequency、unison tuning、pan、ADSR係数の重複計算を削減。SSEは4隣接tableへ同じ補間式を適用し、scalar fallbackも数値検査する。
- heap確保はsetupProcessing、プリセット初回構築、UI描画/ファイル操作で実行。audio callbackにファイル/ログ/ロックは無い。
- イベントはhostのsampleOffsetで処理。automation pointは該当sampleで目標値を更新し連続値を8ms one-pole平滑化する。VST3の点間直線補間は未実装。
- 最大16 voices x 5 oscillators。metal oscillatorは最大7partial。コストは固定の音源上限で制限。ホストが渡すイベント数の処理はホストデータに比例。
- delayはfeedback最大0.82、SVFはカットオフをrate*0.2に制限。出力をtanhで有界化し非有限値を出力しない。filterの極小値はゼロに丸める。
- meter/playheadはoutputParameterChanges。試奏messageはaudio外で検証してatomic<uint64_t>の37bit集合に書く（lock-freeをstatic_assert）。audio側はblock頭に差分を最大37鍵で評価し、note-off queue overflowが無い。旧atomic<int>単音経路も維持。音声block間より短いtapは省略される可能性があるが、保持集合が残ることはない。
- state get/setはホストのライフサイクル契約上、processingと競合させない前提。並行state操作を独自に行うホストへの検証は未実施。

未完: allocation監視テスト、スレッドサニタイザー、denormal性能測定、CPUプロファイル、歪みoversampling、声盗みクロスフェード。`std::sin/exp/tan/pow/tanh`のsampleごとの使用が多いため、多ボイス高rateでは負荷が高い。出力が有限であることは低CPUを保証しない。

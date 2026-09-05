# Architecture / 引き継ぎ用地図

```
DAW MIDI + automation -> plugin/Processor -> core/Engine -> stereo/mono
DAW controller       -> plugin/Controller -> ui/Editor
core/Parameters      -> all three layers
core/Presets         -> editor + standalone tests
```

`src/core` はSDKにもWindowsにも依存しない。`src/plugin` がVST3の分離processor/controller、I/O、MIDI、状態を担当。`src/ui` のみWin32/GDI+に依存する。UIはホストのbeginEdit/performEdit/endEditを使い、engineのメモリを直接触らない。

パラメータID 100〜144は永続契約。`Parameters.h` の registryが唯一の名前・初期値・離散段階の台帳。GUIの配置/用途は `ui/Editor.cpp` の easy / advanced / fixed が台帳。Bend/PedalはMIDI用。122〜140はシーケンサー。141〜144はwavetable。1000/1001は読み取り専用出力メーター/再生位置。

各ボイスにADSR、5phase、subphase、2ch SVFの状態を持つ。16ボイス固定。空きボイス優先、埋まったら最古を盗む。note IDとchannelでnote-offを対応付ける。LFO/FXはグローバル。

固定経路: source + sub -> voice SVF -> voice amp/gate -> mix -> heat -> echo -> master soft clip。ノードを並べ替える仕組みはまだ無い。将来グラフ化する際はDSPから先に小クラスへ抽出し、既存プリセットの固定経路を既定グラフとして移行する。

状態には音色とsequence設定を保存し、再生途中の発振位相・delay buffer・保持ノートは保存しない。同じ状態・同じ新規MIDI列・同じサンプルレートで決定的な音を生成する。sequenceはaudio tickで内部16分音符clockを評価する。試奏はIMessage `dopa.keys` の37bit保持キー集合をprocessorのatomic<uint64_t>へ渡す。旧 `dopa.audition` 単音mailboxも互換維持。audioはblock開始時に読む。確保はmessage生成側のみ。負の予約ID -100（旧単音）/-101（sequence）/-200〜-236（鍵盤）で外部MIDIから区別する。

現在の実装は読みやすい小規模ヘッダー構成。機能追加時は巨大なEngine::tickを延ばさず、envelope / oscillator / filter / effect単位に抽出する。UIの装飾コードはDSPに移さない。


UIの新構造・座標・操作・互換性: [UI_V04.md](UI_V04.md)。

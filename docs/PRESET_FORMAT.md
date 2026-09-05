# Preset / state v3 (v1/v2 migration)

`.dopa` はUTF-8/ASCIIテキスト。先頭は `DOPAGAKI 3`、以降は `parameterID normalizedValue`、45行。C locale、double 17桁。全ID必須、重複・未知ID・範囲外・非有限・破損・16KiB超を拒否。読込は一旦一時Valuesへ構築してから全体を反映する。

DAW stateはlittle-endian: uint32 magic `0x4450474B`、uint32 version 3、uint32 count 45、続いて `{uint32 id,double value}` を45組。合計552 bytes。processorとcontrollerが同じreaderを使う。旧v1は22値/276bytes、v2は41値/504bytesを受理し、追加parameterは既定値へ移行。table mix=0のため旧パッチで新音源を勝手に有効化しない。text/binaryの両方で移行を試験。未知versionは拒否。既存IDを変更・再利用しない。

工場プリセット名とcategoryは `Presets.h` のメタデータ。ユーザーファイル名がユーザープリセット名を兼ねる。ファイル内タグ、author、グラフ、サンプル参照はまだ無い。factory選択は45個のホスト編集として送るため、ホストによって一時的な中間状態があり得る。原子的パッチ切替は将来課題。

ID122〜137はstep01〜16、0=休符、1/12〜12/12=C3〜B3。ID138=run、139=tempo(60+180*n BPM)、140=gate(0.05+0.9*n step)。これらは保存される。試奏鍵盤の保持状態、再生head、UNDO、選択中preset名は保存されない。v0.2はDSPを修正したため旧版と音響的な同一性は無い。

ID141=TABLE MIX、142=FRAME (15*n)、143=FRAME MOTION、144=START PHASE (360*n degrees)。built-in tableそのものは決定的生成で、ファイルに波形を埋め込まない。任意table assetを扱う将来versionにはasset schema追加が必要。

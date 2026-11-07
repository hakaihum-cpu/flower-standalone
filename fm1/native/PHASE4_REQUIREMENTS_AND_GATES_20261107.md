# FM-1 / MIYAKO — ①〜⑧の唯一の現行管理表

更新: 2026-11-07。元のMIYAKO一次ソース:
`hakaihum-cpu/Vstplugin@feature/miyako-ep-sampler-16part-20261030`。
FM-1作業ブランチ:
`hakaihum-cpu/flower-standalone@feature/fm1-miyako-recorder-eureka-fx-20261107`。

**この文書が現行①〜⑧の唯一の定義です。過去の番号・overwrite解釈は廃止しました。**
`main`には触れない。既存CircleCI構成を変更しない。ビルドはユーザーの明示許可がある場合だけ。

| ID | 確定要件 | ソース実装状況 | 未検証／問題 |
| --- | --- | --- | --- |
| ① | 全体的な音のブツブツ音を解消 | **原因不明につき作業保留**（ユーザーの2026-11-07指示）。音声DSPへの憶測による追加修正は禁止 | 原因が分かる実機ログ・比較が得られるまで「解消」としない |
| ② | FX画面：左パラメータ／右は最大限の画像、PREVIEW/AUTO/EUREKA/VHS/8mm等の説明文を表示しない | FX画面右の画像枠116×116、左側4ノブ。余計な字幕・フッターを削除。既存128×78映像は元の縦横比を維持 | 240×240実機上の見切れと操作確認 |
| ③ | DREAMY／EUREKAでPITCHなど4つ以上の本物のFXパラメータを操作する | FX選択直後はPITCH/REVERSE/SPREAD-or-GRAIN/FEEDBACKを表示。PRESETSノブでMODE/WET/従来2項目へ切替。DSPに接続済み | 実機でのピッチとノイズ、DREAMY/EUREKAの元JUCE音との比較 |
| ④ | 提供画像の人物が判別できる。通常EFFECTSと同じ画像変換方式を使用 | 提供6シート全488タイルを48×48・16色 RGB565、8×4／256コードブックVQで収録。既存EFFECTSと同じC配列のパレット／タイルブック／マップ。32×32白黒版とランタイムBase64復号経路は削除 | 実機表示での顔・目・輪郭判別と色の出方 |
| ⑤ | FXをOFFにしたらSELで通常アニメーションに戻れ、解除不能なロックを残さない | SELを開くとFX側の手動映像選択を解除。FX OFF時は通常映像を優先 | FX A/B、各ページからの実機ボタン遷移 |
| ⑥ | HOMEのWAVEにMULTIを追加。HOMEのMODEでOSC1/2を切替、WET位置でDETUNEを変更 | WAVE=4 MULTI、MODEでosc_page切替。実際のOSC2発音はMULTI時だけ有効。EDITページで波形・レベル・オクターブを設定。MI13→MI14保存互換を実装 | 実機の発音・CPU負荷・旧保存データの移行 |
| ⑦ | 統合したアニメーションを**通常のSELアニメーション**で使用する（FX専用にしない） | SEL AUTOで収録済み8バンク（提供6タイル群＋EFFECTS144＋EUREKA576）を演奏に合わせて切替。FX OFFでもSELのSELECTで8バンクを個別固定可能。元MIYAKOのEUREKA36フレーム×16クリップの再生ロジックを移植。EFFECTSの乱数初期値も元実装に合わせた | **未完了**：元MIYAKOのTOP/6 SCENES/CRUSH/SAMPLER/TWILIGHT/KNOWING/CLOUDS DOT/DREAMY専用/EP映像は未収録。すべてを統合済みとは言わない |
| ⑧ | RECを押すたびに**多重録音（OVERDUB）**。長押しでCLEAR（「消して上書き」は誤り） | 非空クリップへのRECで既存PCMを保持し、新しい音を加算。書込前ループをモニター、2秒22.05kHzリングを保持。REC長押し700msでオーディオ側CLEAR | 多層クリップ、クリッピング、ループ境界、操作レイテンシを実機確認 |

## ⑦：元MIYAKOとの差分と検証

- 元の参照: `Source/VisualizerComponent.cpp` の `setAudioActivity`。
- EUREKA: 入力音量0.006以上、オンセット条件0.008かつ直前比1.45倍+0.003、フレーム進行1〜4、16クリップ×36枚、無音・一定音量では停止。元のロジックに合わせて `mi_video.c` を修正。
- EFFECTS: 元の144フレームを維持し、元の乱数初期値を使用。正常なSEL演奏時の再生に接続。
- 写真タイル488枚: ソースにある画像のみ、既存のEFFECTS VQ方式で表示。SELをFX画面の添え物にしない。
- **実ソーステスト合格**: `mi_video.c`・`mi_video.h`・`mi_memory.h`・`test_mi_video.c`をGitHub blob SHAと照合してローカルの同一性を確認し、GCC/Clang双方でC99 `-Wall -Wextra -Werror -fsanitize=address,undefined` を実行。無音／一定入力停止、動的クリップ遷移、FX状態分離を含むテストがPASS。
- その他の元映像は、GitHub内で所在を確認しただけで現行FM-1には未移植。既存の方法で素材の完全性と対象フレーム数を確認し、**容量不足なら独自の保存機構を勝手に採用せず保留**。

## フラッシュ容量の条件

現在、既存EFFECTS 144枚=62,592 bytes、EUREKA 576枚=250,368 bytes、提供488枚のVQ=92,928 bytes。
映像データだけで **405,888 bytes**。FM-1の既存アプリ枠は **581,564 bytes**。
差分 **175,676 bytes** にはプログラム・フォント・ほかのデータも必要。追加映像の収録可能性は実ターゲットリンクで未立証。

## ビルド前の残課題

- ①は指示どおり**保留**。原因不明の音切れを推測修正しない。
- フルMIYAKOのすべての画像が物理容量内で収録できるとは主張しない。
- 既存テストの検査条件を現行C実装に揃え、静的検査・Cホストテスト・ターゲットリンクサイズを確認する。
- JieLi実ターゲットコンパイル、最終リンク／FWSC、実機UIと音の動作は**未実施**。
- CircleCI `fm1_manual` / `fm1_native_build` / `run_build=true` は**未実行**。

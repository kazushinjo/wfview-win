# wfview-win

`wfview-win` は、Icom / Kenwood / Yaesu などのアマチュア無線機を Windows から操作するための wfview 派生版です。
このリポジトリでは Windows 向けのアプリ名、操作 UI、周波数操作、ウォーターフォール調整などを調整しています。

元プロジェクト: [wfview](https://gitlab.com/eliggett/wfview)
公式サイト: [wfview.org](https://wfview.org/)

![wfview screenshot](screenshots/wfview-main.png)

## この版の主な変更点

- Windows 用実行ファイル名を `wfview-win.exe` に変更。
- 送信、TUNE、CW、Rpt/Split、Memories、Power On / Off、Split など主要操作ボタンを角丸表示に変更。
- `送信` ボタンは薄緑色の背景で表示し、送信中は `送信中` の文字を赤で表示。
- RF / AF / SQL などのスライダー群に `WF` スライダーを追加。ウォーターフォールの表示レベルをアプリ上から調整できます。
- 起動時に保存済みの WF レベルが反映されます。
- 周波数表示の桁をクリックすると、その桁が周波数アップ / ダウンのステップになります。
- VFO A / B 表示の横に周波数ダウン `▼` と周波数アップ `▲` ボタンを追加。
- 周波数アップ / ダウンボタンは長押しで連続操作できます。
- 周波数ロック中は周波数アップ / ダウンボタンが無効化され、誤操作を防ぎます。
- ダイアル下のステップ周波数プルダウンは非表示にし、周波数表示の桁クリックでステップを選ぶ操作に統一。
- バンド選択ボタンと周波数入力ポップアップを小型化。
- 起動時はメイン画面を最大化して表示。
- Settings の `Connection Profile` で複数の接続設定を保存、選択、削除できます。
- `Connect` 実行時は、画面に表示されている現在の接続設定を使用します。
- 終了時に発生していた Qt audio 系のクラッシュを回避。
- Yaesu FT-817 / FT-817ND / FT-818 に対応。USB シリアル CAT ケーブルで接続すると、本体の前面パネルを模した専用画面で操作できます（周波数、モード、バンド、VFO A/B、スプリット、クラリファイア、レピーター、トーン、送信、電源など）。
- FT-817 画面に、指定した周波数範囲のスキャン機能を追加（ステップ周波数、信号で止まった後の再開方法、停止時間を指定可能）。
- 異常終了したときに、スタックトレースをログファイルへ書き出し、クラッシュダンプ（.dmp）を保存するようにしました。

## インストール

配布されている Windows 用インストーラーを実行し、画面の案内に従ってインストールします。
インストール後は、スタートメニューまたはインストール先フォルダから `wfview-win.exe` を起動します。

## 基本操作

1. `wfview-win.exe` を起動します。
2. `Settings` で無線機、接続方式、ポート、CI-V アドレスなどを設定します。
3. `Connect` で無線機に接続します。
4. 必要に応じて `Power On` で無線機の電源を入れます。
5. 周波数表示の桁をクリックしてステップ周波数を選択します。
6. VFO 表示横の `▼` / `▲` で、選択した桁のステップ幅だけ周波数を下げる / 上げることができます。ボタンを押したままにすると連続して変化します。
7. `送信`、`TUNE`、`CW`、`Rpt/Split`、`Memories`、`Split` などのボタンで運用操作を行います。`送信` ボタンは薄緑色で、送信中は `送信中` の文字が赤になります。

詳しい操作は [操作説明書](docs/操作説明書.md) を参照してください。

## 接続プロファイル

Settings の `Connection Profile` では、接続設定を名前付きプロファイルとして複数保存できます。

- プロファイル名を入力して `Save` を押すと、現在の接続設定を保存します。
- 既存プロファイルを選択すると、保存されている接続情報が入力欄へ反映されます。
- 同じ名前で `Save` すると上書き保存します。
- `Delete` で選択中のプロファイルを削除します。
- `Connect` を押すと、画面に表示されている現在の設定で接続します。

保存対象には、メーカー、CI-V アドレス、シリアルポート、ボーレート、LAN 有効 / 無効、IP アドレス、各 LAN ポート、接続タイプ、ユーザー名、音声設定などが含まれます。

## 周波数ステップ操作

周波数表示の任意の桁をクリックすると、その桁が現在のステップ周波数になります。
例えば 100 Hz の桁をクリックした後に `▲` を押すと 100 Hz 上がり、`▼` を押すと 100 Hz 下がります。
VFO A / B 表示の横にある `▲` / `▼` ボタンを押したままにすると、選択中のステップ幅で連続してアップ / ダウンします。
周波数ロック中は `▲` / `▼` ボタンがグレーアウトし、周波数変更はできません。

従来のステップ周波数プルダウンは表示から外しています。内部状態の同期には残していますが、通常操作では周波数表示の桁クリックを使ってください。

## ウォーターフォール表示

`WF` スライダーでウォーターフォールの色レベルを調整できます。
値は保存され、次回起動時にも反映されます。

## 注意

- 実際に送信する前に、無線機、アンテナ、ダミーロード、出力設定、周波数、モードを確認してください。
- `送信` と `TUNE` は無線機を送信状態にする操作です。誤操作を避けるため、運用前に接続先とバンドを必ず確認してください。
- 無線機の機種や対応コマンドによって、一部のボタンや機能は表示されない、または無効になることがあります。

## 免責事項

本ソフトウェアは現状有姿（AS IS）で提供され、明示黙示を問わずいかなる保証もありません。
本ソフトウェアの使用または使用不能により生じたいかなる損害（無線機・周辺機器の故障、データの損失、送信操作に起因するものを含みますがこれらに限られません）についても、作者は一切の責任を負いません。
本ソフトウェアは自己責任にてご利用ください。

## 関連ドキュメント

- [操作説明書](docs/操作説明書.md)
- [FT-817 / FT-818 操作説明書（Word）](docs/FT-817_操作説明書.docx) / [PDF](docs/FT-817_操作説明書.pdf)
- [FT-817 / FT-818 CAT 配線表](docs/FT-817_CAT配線表.md)
- [インストール情報](INSTALL.md)
- [wfserver 情報](WFSERVER.md)

## ライセンス

wfview-win は wfview の派生物であり、wfview と同じ **GNU General Public License version 3（GPLv3）** で配布します。
ライセンスの全文はリポジトリ直下の [LICENSE](LICENSE) を参照してください。

- wfview: Copyright 2017-2026 Elliott H. Liggett (W6EL) and Phil E. Taylor (M0VSE)
- wfview-win での追加・変更部分（FT-817 / FT-818 画面、レンジスキャン、クラッシュログを含む）: Copyright 2026 JA6FUF/JH1XHX

このプログラムはフリーソフトウェアです。GPLv3 の条件に従って、再配布や改変ができます。
バイナリを再配布する場合は、GPLv3 に従って対応するソースコードも入手できるようにしてください。
このプログラムは有用であることを願って配布しますが、**いかなる保証もありません**。商品性や特定目的への適合性についての暗黙の保証もありません。詳しくは GPLv3 を参照してください。

同梱・利用している各ライブラリは、それぞれのライセンスに従います（下記クレジットを参照）。

## クレジット

### 元プロジェクト

- **wfview**: Elliott H. Liggett (W6EL)、Phil E. Taylor (M0VSE) と wfview の貢献者・翻訳者の皆さん — <https://wfview.org/>

### wfview が利用しているライブラリ・コード

- **Qt**（The Qt Company、LGPLv3）
- **QCustomPlot**（Emanuel Eichhammer、GPL）— スペクトラム・ウォーターフォール表示
- **PortAudio**（The PortAudio Community、MIT 系ライセンス）/ **RtAudio**（Gary P. Scavone）
- **Opus**（Xiph.Org Foundation ほか、BSD）/ **Speex** の DSP・リサンプラー（Jean-Marc Valin、BSD）
- **hidapi**（Signal 11 Software ほか）/ **Eigen**（MPL2）/ **r8brain-free-src**（Aleksey Vaneev、MIT）
- **PocketFFT**（Martin Reinecke、BSD 3-Clause）
- **Hamlib** 由来の rigctl 互換コード（The Hamlib Group）
- **Audacity Noise Reduction** 由来コード（Dominic Mazzoni、Paul Licameli、GPL）
- **adpcm-xq**（David Bryant）、**QDarkStyleSheet**（Colin Duquesnoy、MIT）
- 周波数表示コード（Moe Wheatley AE4JY、Alexandru Csete OZ9AEC、Simplified BSD）

各ライブラリの著作権表示とライセンス条文は、wfview の About 画面と各ソースファイルに記載されています。

### FT-817 / FT-818 対応

- CAT コマンドの形式と EEPROM のアドレスを確かめるために、次の資料を参照しました（ソースコードは取り込んでいません）。
  - [Hamlib](https://hamlib.github.io/) の ft817 ドライバー（The Hamlib Group、LGPL）
  - CPAN の Perl モジュール [Ham::Device::FT817COMM](https://metacpan.org/pod/Ham::Device::FT817COMM)
- ACC 端子のピン配置: 八重洲無線 FT-817ND 取扱説明書（Operating Manual）
- Yaesu、FT-817、FT-818 は八重洲無線株式会社の商標または製品名です。wfview-win は八重洲無線とは関係のない非公式のソフトウェアです。

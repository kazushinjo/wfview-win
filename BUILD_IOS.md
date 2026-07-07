# wfview iOS / iPadOS ビルドガイド

wfview（Qt Widgets 製のデスクトップ無線機制御アプリ）を **iPad / iPhone 用ネイティブアプリ**として
ビルドするための手順とポーティング内容をまとめたものです。

**構成方針:** ネットワーク接続専用（Icom LAN / wfserver）。
iOS はシリアル / USB / HID を提供しないため、それらの機能は無効化しています。
オーディオは Qt Multimedia（AVFoundation/CoreAudio = darwin バックエンド）を使用します。

**動作確認環境:** Apple Silicon (arm64) / Xcode 26.x (SDK 26.5) / Qt 6.8.1 (ios kit) / iOS 14+ ターゲット

---

## ⚠️ 重要: 非ASCIIパスでビルド不可

qmake は Xcode プロジェクト生成時、日本語などの非ASCIIパスを誤エンコードし
（`アプリ開発` → `u30a2u30d7u30ea…`）、`Qt Preprocess` スクリプトフェーズの `make -C` が失敗します。
**プロジェクトは必ず ASCII のみのパスに置いてください。**

本リポジトリは元々 `/Users/kazuichishinjo/アプリ開発/wfview-ipad` にありましたが、
ビルドのため実体を `/Users/kazuichishinjo/wfview-ipad`（ASCII）へ移動し、
元の場所にはシンボリックリンクを残しています。兄弟依存も ASCII 親から参照:

```
/Users/kazuichishinjo/wfview-ipad          ← 実体（ASCII）
/Users/kazuichishinjo/qcustomplot          → アプリ開発/qcustomplot
/Users/kazuichishinjo/r8brain-free-src     → アプリ開発/r8brain-free-src
```

---

## 前提条件

### 1. Qt 6.8.1 の ios キット（aqtinstall）
```bash
pip3 install aqtinstall
aqt install-qt mac ios 6.8.1 -m qtmultimedia qtwebsockets -O ~/Qt
```
※ iOS キットに **qtserialport は存在しない**（`ios/compat` のスタブで代替）。

### 2. libopus を iOS(arm64/device) 向けにクロスビルド
```bash
# opus 1.6.1 を CMake + Xcode ジェネレータでビルド
cmake -S opus-1.6.1 -B build-ios -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO \
  -DBUILD_SHARED_LIBS=OFF -DOPUS_BUILD_PROGRAMS=OFF -DOPUS_BUILD_TESTING=OFF
cmake --build build-ios --config Release
# 生成物を配置
cp build-ios/Release-iphoneos/libopus.a  ios/lib/
cp opus-1.6.1/include/*.h                ios/include/opus/
```

### 3. eigen（ヘッダオンリー）
Homebrew の eigen3 をローカルにシンボリックリンク（`/opt/homebrew/include` を直接
INCLUDEPATH に足すと **Homebrew版Qt が iOS版Qt ヘッダを汚染**するため厳禁）:
```bash
ln -sfn /opt/homebrew/include/eigen3 ios/include/eigen3
```

---

## ビルド手順

```bash
cd /Users/kazuichishinjo/wfview-ipad
mkdir -p build-ios && cd build-ios
export LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8

~/Qt/6.8.1/ios/bin/qmake ../wfview.pro -spec macx-ios-clang \
    CONFIG+=release CONFIG+=sdk_no_version_check

# 署名なしでコンパイル/リンク確認（実機配布には署名が必要）
xcodebuild -project wfview.xcodeproj -scheme wfview -configuration Release \
    -sdk iphoneos -destination 'generic/platform=iOS' \
    CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO CODE_SIGN_IDENTITY="" build
```

出力: `build-ios/Release-iphoneos/wfview.app`（arm64 実機用）。

### 実機へインストールする場合（要 Apple Developer アカウント）
上記の `CODE_SIGNING_ALLOWED=NO …` を外し、開発チームを指定:
```bash
xcodebuild ... -sdk iphoneos \
    DEVELOPMENT_TEAM=<あなたのTeamID> \
    CODE_SIGN_STYLE=Automatic
```
Xcode で `wfview.xcodeproj` を開き、Signing & Capabilities でチームを選ぶのが簡単です。

### シミュレータで動かす場合
本ガイドの `libopus.a` は **実機(arm64/iphoneos)専用**です。シミュレータ用には
`-sdk iphonesimulator` で opus を別途ビルドし、`.pro` の `ios:LIBS` を切り替える必要があります。

---

## ポーティング内容（wfview.pro の `ios { }` ブロックと #ifdef WFVIEW_IOS）

| 項目 | 対処 |
|---|---|
| QtSerialPort（キットに無い） | `ios/compat/` に QIODevice 派生の互換スタブを実装。実機ではポートを開けず穏当に失敗 |
| USB/HID コントローラ | `USB_CONTROLLER` 未定義化、`usbcontroller.cpp` をビルド除外 |
| FTDI FT4222 | QLibrary 動的ロード方式なのでビルドは維持（実機では no-op） |
| PortAudio / RtAudio | ビルド除外し、生成箇所を `#ifndef WFVIEW_IOS` でガード。音声は Qt(darwin) |
| pttyhandler（擬似端末） | iOS を Windows 分岐と同一化（スタブ QSerialPort 経由、PTY不使用） |
| QProcess（ログ外部起動） | `#ifndef WFVIEW_IOS` でガード |
| libopus | iOS arm64 をローカルビルドし `ios/lib/libopus.a` を静的リンク |
| QCustomPlot | 共有ライブラリの代わりに `qcustomplot.cpp` をアプリへ直接組込 |
| Qt Multimedia backend | FFmpeg プラグインは .prl 不備でリンク不可 → `QTPLUGIN.multimedia = darwinmediaplugin` |
| `__yield` ビルドエラー | Xcode26+Qt6.8 の不具合。`-Wno-error=implicit-function-declaration` |
| Info.plist / アイコン / LaunchScreen | `ios/` に新規作成（Bundle ID `org.wfview.wfview`, iPhone+iPad） |

---

## 既知の残作業

- **実機インストールには Apple Developer 署名が必要**（未署名バイナリは配布不可）。
- **UI はデスクトップ Qt Widgets のまま**。タッチ操作・iPad画面比率への最適化は今後の課題。
- シリアル/USB 機能は iOS では利用不可（ネットワーク接続のみ）。

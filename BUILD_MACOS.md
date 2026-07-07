# wfview macOS ビルドガイド

このガイドは Windows のソースを Mac（Apple Silicon）でビルドするための手順です。
Windows 側で行った freqctrl（▲▼ボタン）の改造が含まれています。

**動作確認環境:** Apple Silicon (arm64) / Xcode（SDK 26.5）/ Qt 6.8.1 / macOS 12+ ターゲット

> ⚠️ このガイドは実際のビルドで判明したハマりどころ（qt@5競合・AGL廃止・universal問題など）を
> すべて反映済みです。新しい Xcode/SDK では下記の追加対処が必須です。

---

## 前提条件

### 1. Xcode Command Line Tools
```bash
xcode-select --install
```

### 2. Homebrew（未インストールの場合）
```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```
Apple Silicon では `/opt/homebrew`、Intel Mac では `/usr/local` にインストールされます。

### 3. 依存ライブラリ（Homebrew）
```bash
brew install opus portaudio hidapi rtaudio eigen cmake
```

> ⚠️ **qt@5 競合に注意**: `brew list | grep qt` で `qt@5` 等が入っている場合、
> `/opt/homebrew/include/QtCore/qconfig.h` が正規の Qt6 より優先されてビルドが壊れます。
> 本ガイドでは `-I` ではなく `-isystem` で Homebrew を低優先に指定して回避します（後述）。

### 4. Qt 6.8.1 をインストール（aqtinstall 使用）
```bash
pip3 install aqtinstall

# ★追加モジュール serialport / multimedia / websockets が必須
aqt install-qt mac desktop 6.8.1 clang_64 \
    -m qtserialport qtmultimedia qtwebsockets -O ~/Qt
```
qmake は `~/Qt/6.8.1/macos/bin/qmake` にあります。

---

## ディレクトリ構造

`wfview` の **1つ上のフォルダ** に依存ライブラリを置きます（wfview.pro が `../` で参照）。

```
（親フォルダ）/
├── wfview/              ← このフォルダ
├── qcustomplot/         ← 別途クローン
├── r8brain-free-src/    ← 別途クローン
└── rtaudio/             ← 別途クローン（★Homebrew版だけでは不十分）
```

```bash
cd ..    # wfview の1つ上へ

# ★qcustomplot は公式版(qcustomplot.com)を使うこと（下記の警告参照）
mkdir -p qcustomplot
curl -L "https://www.qcustomplot.com/release/2.1.1/QCustomPlot.tar.gz" -o qcustomplot.tar.gz
tar -xzf qcustomplot.tar.gz -C qcustomplot --strip-components=1

git clone --depth 1 https://github.com/avaneev/r8brain-free-src.git
git clone --depth 1 https://github.com/thestk/rtaudio.git
```

> ⚠️ **qcustomplot は公式版(qcustomplot.com/release/2.1.1)を使う**:
> GitHub の legerch fork 版（QCustomPlot-library）だと**コンパイル・起動は成功するが、
> 実行時にスペクトラムスコープ／ウォーターフォールが描画されない**不具合があります。
> Windows版と同じ公式版を使ってください。公式版は `config.h` を必要とせず、
> `qcustomplot.h` / `qcustomplot.cpp` がアーカイブ直下に展開されます。

> ⚠️ **rtaudio ソースが必須**: macOS でも `wfview.pro:74` が `../rtaudio/RTAudio.cpp` を
> 直接コンパイルします（macはファイル名の大文字小文字を区別しないので `RtAudio.cpp` に一致）。
> Homebrew の rtaudio（ライブラリ）だけでは足りません。

---

## ビルド手順

### 1. qcustomplot をビルド

legerch 版は `lib/` 配下にソースがあり、`config.h`（CMake生成物）を必要とします。
手動で配置・生成します。

```bash
cd ../qcustomplot

# lib配下のソースをトップへコピー
cp lib/qcustomplot.h lib/qcustomplot.cpp .

# config.h を手動生成（本来 CMake が config.h.in から生成）
cat > config.h << 'EOF'
#ifndef QCP_LIB_CONFIG_H
#define QCP_LIB_CONFIG_H
#define QCP_LIB_VERSION "2.1.1.1"
#define QCP_LIB_VERSION_MAJOR "2"
#define QCP_LIB_VERSION_MINOR "1"
#define QCP_LIB_VERSION_PATCH "1"
#define QCP_LIB_VERSION_TWEAK "1"
#endif
EOF
cp config.h lib/config.h

# sharedlib 用 pro を作成
mkdir -p qcustomplot-sharedlib/build
cat > qcustomplot-sharedlib/qcustomplot-sharedlib.pro << 'EOF'
TEMPLATE = lib
TARGET = qcustomplot
DESTDIR = build
QT += widgets printsupport
CONFIG += release shared
DEFINES += QCUSTOMPLOT_COMPILE_LIBRARY
SOURCES += ../qcustomplot.cpp
HEADERS += ../qcustomplot.h
QMAKE_CXXFLAGS += -include arm_acle.h
EOF

cd qcustomplot-sharedlib
~/Qt/6.8.1/macos/bin/qmake qcustomplot-sharedlib.pro CONFIG+=release
# ★新SDKで廃止された AGL フレームワーク参照を Makefile から除去
sed -i '' -e 's/ -framework AGL//g' \
          -e 's#-I[^ ]*AGL.framework/Headers/ ##g' Makefile
make -j$(sysctl -n hw.logicalcpu)
```
→ `build/libqcustomplot.dylib` が生成されます。

### 2. wfview をビルド

```bash
cd ../../wfview   # wfview のトップへ
rm -f .qmake.stash Makefile

ISYS="-isystem /opt/homebrew/include -isystem /opt/homebrew/include/opus -isystem /opt/homebrew/include/hidapi"

~/Qt/6.8.1/macos/bin/qmake wfview.pro CONFIG+=release "CONFIG+=sdk_no_version_check" \
    "INCLUDEPATH += /opt/homebrew/include/eigen3 ../qcustomplot" \
    "QMAKE_CXXFLAGS += $ISYS -include arm_acle.h" \
    "QMAKE_CFLAGS += $ISYS -include arm_acle.h" \
    "QMAKE_OBJECTIVE_CFLAGS += $ISYS -include arm_acle.h" \
    "LIBS += -L/opt/homebrew/lib"

# ★Makefile 後処理（arm64単一化・AGL除去・SSE定義除去）
sed -i '' \
  -e 's/^EXPORT_VALID_ARCHS = x86_64 arm64/EXPORT_VALID_ARCHS = arm64/' \
  -e 's/ -framework AGL//g' \
  -e 's#-I[^ ]*AGL.framework/Headers/ ##g' \
  -e 's/ -DUSE_SSE2//g' -e 's/ -DUSE_SSE//g' -e 's/ -DEIGEN_VECTORIZE_SSE3//g' \
  Makefile

make clean
make -j$(sysctl -n hw.logicalcpu)
```
成功すると `wfview.app/Contents/MacOS/wfview`（arm64）が生成されます。

**各対処の理由:**
- `-isystem /opt/homebrew/include` … `qt@5` の qconfig.h 競合を回避（`-I` だと Qt5 が優先されて壊れる）
- `-isystem .../opus`, `.../hidapi` … wfview が `"opus.h"` `"hidapi.h"` を直接 include するため
- `-include arm_acle.h` … Qt6.8 の `qyieldcpu.h` が使う `__yield` intrinsic を有効化
- `EXPORT_VALID_ARCHS = arm64` … `wfview.pro:120` が universal をハードコードしているため arm64 単一に固定（universal だと x86 側で arm_acle.h が失敗し、`-DUSE_SSE` も不正になる）
- AGL 除去 … 新 SDK で AGL.framework が廃止されたため
- USE_SSE 除去 … qmake が QT_ARCH を x86_64 と誤判定して付与する SSE 定義（arm64 では不正）

### 3. アプリのバンドル化と DMG 作成

```bash
cd ../wfview
~/Qt/6.8.1/macos/bin/macdeployqt wfview.app

# ★qcustomplot は install_name にパスが無く macdeployqt が拾えないので手動で組み込む
cp ../qcustomplot/qcustomplot-sharedlib/build/libqcustomplot.1.0.0.dylib \
   wfview.app/Contents/Frameworks/libqcustomplot.1.dylib
install_name_tool -id @rpath/libqcustomplot.1.dylib \
   wfview.app/Contents/Frameworks/libqcustomplot.1.dylib
install_name_tool -change libqcustomplot.1.dylib @rpath/libqcustomplot.1.dylib \
   wfview.app/Contents/MacOS/wfview

# ★ad-hoc 署名（Apple Silicon では未署名 arm64 は起動時に kill される）
#   install_name_tool で署名が壊れるので必ず最後に署名する
codesign --force --deep -s - wfview.app
codesign --verify --verbose wfview.app   # "valid on disk" を確認

# ★DMG は署名済み app から hdiutil で作成
#   （macdeployqt -dmg は app を再処理して署名を壊すため使わない）
hdiutil create -volname wfview -srcfolder wfview.app -ov -format UDZO wfview.dmg
```
→ `wfview.dmg` が生成されます。

### 4. 起動

```bash
open wfview.app
```

---

## トラブルシューティング

| エラー / 症状 | 対処 |
|--------|------|
| `Qt major version not 6 or 7` / `QT_CORE_REMOVED_SINCE is not defined` | qt@5 競合。`-I/opt/homebrew/include` を `-isystem` に変更（手順2参照） |
| `config.h file not found`（qcustomplot） | 手順1の config.h を手動生成 |
| `framework 'AGL' not found` | Makefile から `-framework AGL` を sed 除去 |
| `__yield` implicitly declaring / `ACLE intrinsics support not enabled` | `-include arm_acle.h` を付与し、かつ **arm64 単一**でビルド（universal 不可） |
| `Unknown module(s) in QT: serialport multimedia websockets` | `aqt install-qt ... -m qtserialport qtmultimedia qtwebsockets` |
| `opus.h` / `hidapi.h` file not found | `-isystem /opt/homebrew/include/opus`（および `/hidapi`）を追加 |
| `RTAudio.cpp` not found | rtaudio 公式（thestk/rtaudio）を `../rtaudio` にクローン |
| 起動直後に落ちる（`code object is not signed`） | `codesign --force --deep -s - wfview.app` を最後に実行 |
| `libqcustomplot.1.dylib` が見つからず起動失敗 | 手順3の手動組み込み＋rpath修正 |
| Bluetooth permission denied | `System Settings > Privacy > Bluetooth` に wfview を追加 |

---

## Windows から Mac への変更済みファイル

以下は Windows 側で改造済み（ソースに含まれる）：

- `src/freqctrl.cpp` — `stepFreqUp()` / `stepFreqDown()` 追加
- `include/freqctrl.h` — 同 public slots 追加
- `src/receiverwidget.cpp` — ▲▼ ボタンを周波数ダイアル横に追加（自動連打対応）
- `include/receiverwidget.h` — ボタンメンバー追加

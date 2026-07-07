# wfview macOS ビルドガイド

このガイドは C:\claude\wfview（Windows）のソースを Mac でビルドするための手順です。
Windows 側で行った freqctrl（▲▼ボタン）の改造が含まれています。

---

## 前提条件

### 1. Xcode Command Line Tools をインストール
```bash
xcode-select --install
```

### 2. Homebrew をインストール（未インストールの場合）
```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

Apple Silicon (M1/M2/M3) の場合、Homebrew は `/opt/homebrew` にインストールされます。  
Intel Mac の場合は `/usr/local` です。

### 3. 依存ライブラリをインストール（Homebrew）
```bash
brew install opus portaudio hidapi rtaudio eigen cmake
```

### 4. Qt 6.8.1 をインストール（aqtinstall 使用）
```bash
pip3 install aqtinstall

# Apple Silicon (arm64) の場合:
aqt install-qt mac desktop 6.8.1 clang_64 -O ~/Qt

# Intel Mac の場合も clang_64 で同じ
```

Qt がインストールされた後、`~/Qt/6.8.1/macos/bin/qmake` にあります。

---

## ディレクトリ構造

Windowsと同じく、`wfview` の **1つ上のフォルダ** に依存ライブラリを置きます。

```
~/Documents/
├── wfview/              ← このフォルダをまるごと持ってくる
├── qcustomplot/         ← 別途クローンが必要
├── r8brain-free-src/    ← 別途クローンが必要
└── rtaudio/             ← Homebrew を使うので不要（後述）
```

### qcustomplot を準備
```bash
cd ~/Documents
git clone https://github.com/legerch/QCustomPlot-library.git qcustomplot
```

### r8brain-free-src を準備
```bash
cd ~/Documents
git clone https://github.com/avaneev/r8brain-free-src.git
```

---

## ビルド手順

### 1. qcustomplot をビルド

```bash
cd ~/Documents/qcustomplot

# sharedlib ビルド用ディレクトリを作成
mkdir -p qcustomplot-sharedlib/build
cd qcustomplot-sharedlib/build

# qcustomplot-sharedlib.pro が存在しない場合は作成
cat > ../qcustomplot-sharedlib.pro << 'EOF'
TEMPLATE = lib
TARGET = qcustomplot
DESTDIR = build
QT += widgets printsupport
CONFIG += release shared
DEFINES += QCUSTOMPLOT_COMPILE_LIBRARY
SOURCES += ../qcustomplot.cpp
HEADERS += ../qcustomplot.h
EOF

cd ..
~/Qt/6.8.1/macos/bin/qmake qcustomplot-sharedlib.pro CONFIG+=release
make -j$(sysctl -n hw.logicalcpu)
```

ビルド後、`~/Documents/qcustomplot/qcustomplot-sharedlib/build/libqcustomplot.dylib` が生成されます。

### 2. wfview をビルド

```bash
cd ~/Documents/wfview

# Apple Silicon の場合（Homebrew パスが /opt/homebrew）
~/Qt/6.8.1/macos/bin/qmake wfview.pro CONFIG+=release \
    INCLUDEPATH+=/opt/homebrew/include \
    LIBS+="-L/opt/homebrew/lib"

# Intel Mac の場合（/usr/local は wfview.pro に既定値あり）
~/Qt/6.8.1/macos/bin/qmake wfview.pro CONFIG+=release

make -j$(sysctl -n hw.logicalcpu)
```

成功すると `wfview.app` が生成されます。

### 3. macdeployqt でアプリをバンドル化

```bash
cd ~/Documents/wfview
~/Qt/6.8.1/macos/bin/macdeployqt wfview.app -dmg
```

`wfview.dmg` が生成されます。これを配布・インストールに使用できます。

---

## Apple Silicon でのビルド注意点

Homebrew が `/opt/homebrew` の場合、qmake コマンドに以下を追加してください：

```bash
~/Qt/6.8.1/macos/bin/qmake wfview.pro CONFIG+=release \
    "INCLUDEPATH += /opt/homebrew/include" \
    "LIBS += -L/opt/homebrew/lib"
```

または `wfview.pro` の `macx:INCLUDEPATH` / `macx:LIBS` を編集してください：
```
macx:INCLUDEPATH += /opt/homebrew/include
macx:LIBS += -L/opt/homebrew/lib
```

---

## トラブルシューティング

| エラー | 対処 |
|--------|------|
| `qcustomplot.h not found` | `../qcustomplot/qcustomplot-sharedlib/build` にビルド済み .dylib があるか確認 |
| `opus/opus.h not found` | `brew install opus` を実行 |
| `portaudio.h not found` | `brew install portaudio` を実行 |
| `hidapi.h not found` | `brew install hidapi` を実行 |
| `qmake: command not found` | `~/Qt/6.8.1/macos/bin/` をフルパスで指定 |
| Bluetooth permission denied | `System Settings > Privacy > Bluetooth` に wfview を追加 |

---

## Windows から Mac への変更済みファイル

以下のファイルが Windows 側で改造されています（ソースに含まれています）：

- `src/freqctrl.cpp` — `stepFreqUp()` / `stepFreqDown()` 追加
- `include/freqctrl.h` — 同 public slots 追加
- `src/receiverwidget.cpp` — ▲▼ ボタンを周波数ダイアル横に追加（自動連打対応）
- `include/receiverwidget.h` — ボタンメンバー追加

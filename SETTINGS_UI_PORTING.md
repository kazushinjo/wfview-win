# 設定画面の改修 移植手順（wfview-mac から）

wfview-mac の以下のコミットで行った設定画面の改修を、このリポジトリにも適用するための手順書。
参照: kazushinjo/wfview-mac@88374fb "Settings UI: fix profile save/connect data loss, CI-V model pulldown"
（同コミットはフォーマッタの影響でdiffが大きいため、実質的な変更点をこの文書にまとめる）

対象ファイル: `src/settingswidget.cpp` / `include/settingswidget.h` / `src/settingswidget.ui` / `src/wfmain.cpp`

---

## 1. Save/Connect 前に未確定の入力欄を確定する（データ消失バグ修正）

**症状**: CI-Vアドレス等のQLineEditに入力した直後にSaveやConnectボタンを押すと、
`editingFinished`が発火しておらず古い値のまま保存・接続される。
macOSではボタンクリックでフォーカスが移動しないため必発。タッチUI（iPad/iPhone）でも
フォーカスが残ったままボタンをタップすると同じことが起きる。Windowsでも
ツールバー等フォーカスを取らないコントロール経由で発生し得る。

`settingswidget` に以下を追加し、Connectボタンとプロファイル保存ボタンのハンドラ冒頭で呼ぶ:

```cpp
void settingswidget::commitPendingEdits()
{
    // Clicking a button does not always move focus out of a QLineEdit,
    // so editingFinished (which commits fields like the CI-V address into
    // prefs) may never fire. Force-commit the focused editor first.
    QWidget* editor = QApplication::focusWidget();
    if (editor != Q_NULLPTR && editor != this)
        editor->clearFocus();
}

void settingswidget::on_connectBtn_clicked()
{
    commitPendingEdits();
    emit connectButtonPressed();
}
```

プロファイルSaveボタンのlambda冒頭にも `commitPendingEdits();` を追加。
`#include <QApplication>` が必要。

## 2. Connect時にプロファイルを再読込しない（未保存編集の巻き戻し廃止）

**症状**: 設定画面のConnectボタンが接続前に選択中プロファイルを再読込するため、
プロファイル選択後に手で修正した値（CI-V、IP、ポート等）が保存済みの値で
上書きされてから接続してしまう。

`wfmain::handleExtConnectBtn()` から再読込ブロックを削除する:

```cpp
void wfmain::handleExtConnectBtn() {
    // from settings widget
    // Note: do NOT reload the selected connection profile here. The profile
    // is already applied when it is selected in the combo box; reloading it
    // on connect would silently discard any unsaved edits (CI-V address,
    // IP address, etc.) the user made after selecting the profile.
    on_connectBtn_clicked();
}
```

## 3. Icom選択時もCAT/Audioポート欄を表示

`settingswidget::on_manufacturerCombo_currentIndexChanged()` の Icom 分岐で
`catPortLabel/catPortTxt/audioPortLabel/audioPortTxt` を `setVisible(true)` にする
（scopePortはIcomでは未使用のため非表示のまま）。
注意: Icomプロトコルではserial/audioポートはログイン応答でサーバー側から
通知されるため、表示は参考値。実際に使われるのはControlポートのみ。

## 4. CI-Vアドレスをプルダウン化（メーカー連動）

### 4-1. .ui の変更

`rigCIVaddrHexLine`（QLineEdit）を編集可能なQComboBox `rigCIVaddrCombo` に置き換える。
レイアウトが狭いと表示が切れるため、チェックボックスとは別の行（gridの独立行）に配置する。

```xml
<item row="1" column="0">
 <widget class="QComboBox" name="rigCIVaddrCombo">
  <property name="enabled"><bool>false</bool></property>
  <property name="minimumSize"><size><width>170</width><height>0</height></size></property>
  <property name="editable"><bool>true</bool></property>
 </widget>
</item>
```

### 4-2. rigList（.rig定義ファイル）から機種一覧を作る

`settingswidget` に wfmain が持つ `QHash<quint16,rigInfo> rigList` へのポインタを渡す。

ヘッダ（public slots または public）:
```cpp
void acceptRigListPtr(QHash<quint16,rigInfo> *rptr);
void refreshCivAddrList();
```
プライベートに:
```cpp
QHash<quint16,rigInfo> *rigList = Q_NULLPTR;
void populateCivAddrCombo();
void setCivComboToAddress(quint16 addr);
```
スロット:
```cpp
void on_rigCIVaddrCombo_activated(int index);
void civAddrEditFinished();
```

`populateCivAddrCombo()` の要点:
- `rigList` が非空なら `rigList->values()` を機種名でソートし、
  `addItem("モデル名 (アドレスHEX)", civ)` で流し込む（civ==0はスキップ）。
  rigListはwfmainが選択メーカーでフィルタ済みなので、Icom以外（Kenwood/Yaesu）でも
  そのメーカーの機種＋モデルIDが表示される。
- rigList未設定時のフォールバックとしてIcom機種の固定表に切り替え。
- `setSizeAdjustPolicy(QComboBox::AdjustToContents)` と
  `view()->setMinimumWidth(200)`（`#include <QAbstractItemView>`）で表示切れを防ぐ。
- 最後に `prefs->radioCIVAddr` に応じて選択状態を復元
  （0なら "auto" 表示＋disabled、非0なら該当項目を選択）。

`civAddrEditFinished()`（コンボのlineEditの`editingFinished`にconnect、
接続はコンストラクタで一度だけ行うこと）:
- 現在テキストが一覧項目に一致すればそのdata、そうでなければ16進として解釈。
- 変換失敗または0なら `setCivComboToAddress(prefs->radioCIVAddr)` で表示を戻す。
- YaesuのモデルIDは255を超えるため、アドレスは `quint16` で扱い、
  Icom用の `>= 0xE0` 弾きは撤廃。

`on_rigCIVManualAddrChk_clicked()` / `updateRaPrefs` の `ra_radioCIVAddr` ケースも
旧 `rigCIVaddrHexLine` 参照をコンボ操作（`setCivComboToAddress` / "auto"）に置換。
`ra_radioCIVAddr` では `quietlyUpdateCheckbox(ui->rigCIVManualAddrChk, prefs->radioCIVAddr != 0)`
でチェック状態も同期しておく。

### 4-3. wfmain側の配線

```cpp
// コンストラクタ、setupui生成直後:
setupui->acceptRigListPtr(&rigList);

// setManufacturer() の末尾（rigList再構築後）:
if (setupui != Q_NULLPTR)
    setupui->refreshCivAddrList();
```

`on_manufacturerCombo_currentIndexChanged()` の末尾（`emit changedRaPref(ra_manufacturer)`の直前）
にも `populateCivAddrCombo();` を追加（プロファイル読込経由の変更にも追従させるため）。

---

## 動作確認項目

1. CI-V欄に値を入力して即Save → プロファイルに新しい値が保存されること
2. プロファイル選択後にポート等を手修正して即Connect → 修正値のまま接続されること
3. メーカーをIcom/Kenwood/Yaesuに切り替えると、CI-Vプルダウンの機種一覧が連動して変わること
4. プルダウンから機種を選ぶと `prefs->radioCIVAddr` に反映されること（手入力の16進も可）
5. Icom選択時にControl/CAT/Audioの3ポート欄が表示・編集・保存できること

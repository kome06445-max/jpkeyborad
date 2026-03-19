# BSDJP - FreeBSD Japanese Input Method

FreeBSD + KDE Plasma (X11) 向けの自作日本語入力メソッド (IME)。
XIMプロトコルを介して、ローマ字入力からひらがな・カタカナ・漢字変換を行います。

## 機能

- **ローマ字→かな変換**: 全ローマ字パターン対応（拗音、促音、撥音含む）
- **かな→漢字変換**: SKK辞書ベースの変換エンジン
- **予測変換**: 入力履歴に基づく候補提案
- **学習機能**: ユーザー辞書による頻度ベースの候補並べ替え
- **候補ウィンドウ**: Cairo + Pango による日本語テキスト描画
- **JP106キーボード**: 日本語キーボード完全対応

## 動作環境

- FreeBSD 13.x / 14.x
- KDE Plasma (X11)
- JP106 キーボード

## 依存関係

```
xcb-imdkit    - XIMプロトコル実装
libxcb        - X11接続
xcb-util      - XCBユーティリティ
xcb-util-keysyms - キーシンボル処理
cairo         - 2Dグラフィックス
pango         - テキストレンダリング
cmake         - ビルドシステム
pkgconf       - パッケージ設定
```

## セットアップ

### 自動セットアップ (推奨)

```sh
sudo sh scripts/setup_freebsd.sh
```

これにより以下が設定されます:
- `/etc/rc.conf` に JP106 キーマップ
- Xorg の日本語キーボード設定
- 必要な依存パッケージのインストール
- SKK辞書のダウンロード

### 手動セットアップ

```sh
# 依存パッケージのインストール
sudo pkg install xcb-imdkit libxcb xcb-util xcb-util-keysyms cairo pango cmake pkgconf

# SKK辞書のダウンロード
sh scripts/download_dict.sh
```

## ビルド

```sh
mkdir build && cd build
cmake ..
make
```

## インストール

```sh
cd build
sudo make install
```

## 使い方

### 起動

```sh
export XMODIFIERS=@im=BSDJP
bsdjp &
```

KDE Plasma の自動起動に追加する場合:

```sh
mkdir -p ~/.config/autostart
cat > ~/.config/autostart/bsdjp.desktop << EOF
[Desktop Entry]
Type=Application
Name=BSDJP Japanese IME
Exec=sh -c "export XMODIFIERS=@im=BSDJP && bsdjp"
X-KDE-autostart-after=panel
EOF
```

### キー操作

| キー | 動作 |
|------|------|
| ローマ字入力 | ひらがな/カタカナに変換 |
| Space | かな→漢字変換開始 |
| Enter | 確定 |
| Escape | 変換キャンセル / 入力クリア |
| Backspace | 一文字削除 |
| 上下キー | 候補選択 |
| 1-9 | 候補のダイレクト選択 |
| F6 | ひらがなモード |
| F7 | カタカナ変換 |
| F10 | ASCII/日本語モード切替 |
| 全角/半角キー | 日本語/ASCIIモード切替 |

## プロジェクト構成

```
BSDJP/
├── CMakeLists.txt          ビルドシステム
├── README.md               このファイル
├── src/
│   ├── main.c              エントリポイント
│   ├── xim_server.c/.h     XIMプロトコル処理
│   ├── engine.c/.h         変換エンジン中核
│   ├── romaji.c/.h         ローマ字→かな変換
│   ├── kana_kanji.c/.h     かな→漢字変換
│   ├── predict.c/.h        予測変換
│   ├── dict.c/.h           SKK辞書管理
│   ├── user_dict.c/.h      ユーザー辞書
│   ├── candidate_window.c/.h 候補ウィンドウUI
│   └── util.c/.h           ユーティリティ
├── data/
│   ├── romaji_table.txt    ローマ字変換テーブル
│   └── SKK-JISYO.L         SKK辞書 (要ダウンロード)
├── scripts/
│   ├── setup_freebsd.sh    環境セットアップ
│   └── download_dict.sh    辞書ダウンロード
└── test/
    ├── test_romaji.c       ローマ字変換テスト
    └── test_dict.c         辞書テスト
```

## テスト

```sh
cd build
ctest
```

## ライセンス

MIT License

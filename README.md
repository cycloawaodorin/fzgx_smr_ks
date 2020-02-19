# SMR for F-ZERO GX
An output plugin which read speedometer numbers of F-ZERO GX and output them into text-based format.

## Installation
Copy `fzgx_smr_ks.auo` file into the root or plugins directory of AviUtl. If you are not familiar with AviUtl, read `USAGE.md` for the instruction.

## Building
You do not need to build for using this plugin. If you want to build yourself, enter the `src` directory and execute `make`. The Makefile is written for using g++ on MSYS2. You should rewrite Makefile for environments other than MSYS2.

## Notice
The input video should be 720x480 and F-ZERO GX should be set for 16:9 monitor. If the video is for 4:3 monitor or resized to other than 720x480 resolation, you must resize the input video before using this plugin. The brief instruction is written in the `Resizing` section of `USAGE.md`

## Setting
### ウィンドウ / Position of the window
Indicate the left-top coordinate of the window which covers the numbers of the speedometer. The default value will be appropriate. You can correct the value in the preview dialog box if needed.

### 出力時にプレビューを表示 / Use the preview dialog box
The preview dialog box will be shown if this item is checked. In the dialog box, you correct the window position

### 出力時にプレビューを表示
出力開始時にウィンドウの調整のためのプレビューダイアログを表示する．ダイアログでは，右下のプレビュー画像に 4 桁の数字が，右上の見本画像と同じような位置に表示されるようにウィンドウの座標を調整する．ボタンで 1 ずつ動かしてもよいし，数値を直接入力してもよい．フレーム番号を変えるとプレビューで表示する画像のフレームを変更でき，出力ファイルには影響しない．

### スレッド数
マルチスレッド処理に使うスレッド数の指定．0 以下の場合，指定値+論理コア数を代わりに用いる．通常はデフォルトの`0`を指定しておくとよい．

### 競合時ダイアログで人力確認
複数のモデルで全会一致にならなかった場合に確認ダイアログを表示する．「評価値x以下でも確認」では最大評価値(確信度)が指定値以下のときにも確認を行う．評価値は 0 から 1 の間の値を取る．「常に確認」にすると条件によらず常に確認ダイアログを表示する．

### フレーム番号を出力
チェックするとスピードメータ指示値とともにフレーム番号を出力する．オフセットで出力範囲の最初のフレーム番号を指定する．区切り文字はフレーム番号と指示値の間に出力する文字を指定する．

## Contributing
バグ報告等は https://github.com/cycloawaodorin/fzgx_smr_ks の Issue 等にて．

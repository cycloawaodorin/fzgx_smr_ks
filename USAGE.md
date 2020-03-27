# Usage
This document provides a brief instruction to use "SMR for F-ZERO GX," an output plugin of AviUtl.

## AviUtl
AviUtl is a video filtering tool for Windows. It is not so difficult to use "SMR for F-ZERO GX." However, AviUtl is complex because of its multifunctionality and only Japanese language is available. Thus, this section explains basic functions of AviUtl.

### Downloading and Installation
AviUtl can be downloaded from http://spring-fragrance.mints.ne.jp/aviutl/.

![aviutl](https://raw.githubusercontent.com/cycloawaodorin/fzgx_smr_ks/image/aviutl.png)

Version 1.10 is the latest (as of 27-Mar-2020), however, version 1.00 seems to be more stable. Both versions are suitable for "SMR for F-ZERO GX."

Just unzip the downloaded archive file for installation.

### Installing Plugins
Make `plugins` directory at the same hierarchy of aviutl.exe. Copy AUO (output plugins), AUI (input plugins) or AUF (filter plugins) files into the `plugins` directory.

```text
┏ plugins
┃ ┣ fzgx_smr_ks.auo
┃ ┗ (other plugin files)
┣ aviutl.exe
```

### Opening Videos
Drag-and-drop the video file onto the `aviutl.exe` window. Without input plugins, AviUtl can only open AVI files. Thus, we recommend to install [L-SMASH Works](#l-smash-works) to open such as MP4 files.

### Setting Output Frame Range
Use slider or frame-by-frame move button to change the current frame. Set the start and end frames by buttons for that. The buttons are located at right-bottom of the aviutl.exe window. Frames before the start frame and frames after the end frame will not be output.

![frame_controller](https://raw.githubusercontent.com/cycloawaodorin/fzgx_smr_ks/image/frame_control.png)

### Global System Setting
Open global system setting dialog box through "ファイル / File" > "環境設定 / Environment Setting" > "システム設定 / System Setting." We recommend to set "最大画像サイズ / Max Resolution" and "最大フレーム数 / Max Number of Frames" to large enough values. "最大画像サイズ / Max Resolution" is the buffer size. AviUtl can only handle images less than or equal to this size. If this value is set to a larger value, AviUtl requires more memory. The max available value of "最大フレーム数 / Max Number of Frames" is 320,000 and this will be appropriate. Built-in resize filter can only resize to the resolutions listed on "リサイズ設定の解像度リスト/ Resize Resolution List." These setting are applied after restart.

![system_setting](https://raw.githubusercontent.com/cycloawaodorin/fzgx_smr_ks/image/system_setting0.png)
![system_setting_db](https://raw.githubusercontent.com/cycloawaodorin/fzgx_smr_ks/image/system_setting.png)

## Using SMR for F-ZERO GX
### Plugin Output
[Open a video file](#opening_videos), [set output frame range](setting_output_frame_range), and then open the plugin output dialog box through "ファイル / File" > "プラグイン出力 / Plugin Output" > "SMR for F-ZERO GX."

![plugin_output](https://raw.githubusercontent.com/cycloawaodorin/fzgx_smr_ks/image/plugin_output0.png)
![plugin_output_db](https://raw.githubusercontent.com/cycloawaodorin/fzgx_smr_ks/image/plugin_output.png)

Press "ビデオ圧縮 / Video Compression" button to open the plugin setting dialog box. After the setting, choose an output directory, input save file name, and then press "保存(S) / Save" button to start outputting.

### Setting Dialog Box
See the [Setting section of README.md](README.md#setting) for detail.

![setting_db](https://raw.githubusercontent.com/cycloawaodorin/fzgx_smr_ks/image/setting.png)

### <a name="preview_dialog_box"></a>Preview Dialog Box
See the [Preview dialog box section of README.md](README.md#preview) for detail.

![preview_db](https://raw.githubusercontent.com/cycloawaodorin/fzgx_smr_ks/image/preview.png)
![preview_db2](https://raw.githubusercontent.com/cycloawaodorin/fzgx_smr_ks/image/preview2.png)

In this case, (539, 411) is prefer to (536, 413).

### Confirmation Dialog Box
The confirmation dialog box will be shown when the [specified conditions](README.md#confirm) are met. Correct the displayed number and press "OK" button, or press "中断 / Interrupt" button to interrupt outputting.

![confirmation_db](https://raw.githubusercontent.com/cycloawaodorin/fzgx_smr_ks/image/confirmation.png)

### Resizing
If you want to extract speed values from videos other than 720x480 videos for 16:9 monitors, you need to resize before using this plugin. To resize videos on AviUtl, you can use "クリッピング&リサイズ / Clipping and Resize" built-in filter or [Lanczos 3-lobed 拡大縮小](#lanczos3) plugin filter. To use these filters, open setting window through "設定 / Setting" > filter name, and check the right-top activation checkbox on the filter setting window. If you use resize filters, make sure that the resizing and window position are appropriate, by the [Preview Dialog Box](#preview_dialog_box).

![clipping_resize](https://raw.githubusercontent.com/cycloawaodorin/fzgx_smr_ks/image/clipping_resize0.png)
![clipping_resize_w](https://raw.githubusercontent.com/cycloawaodorin/fzgx_smr_ks/image/clipping_resize.png)

## Recommended Plugins
### [L-SMASH Works](http://avisynth.nl/index.php/LSMASHSource)
An input plugin which can read various video formats including MP4, MOV, QT, 3gp, 3g2, F4V and M4A.

### <a href="http://www.marumo.ne.jp/auf/#lanczos3" name="lanczos3">Lanczos 3-lobed 拡大縮小</a>
A filter plugin which can resize arbitrary resolutions.

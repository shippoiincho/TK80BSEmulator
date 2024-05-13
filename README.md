TK-80BS Emulator for CH32V203

---

# 概要

TK-80 エミュレータにつづいて TK-80BS のエミュレータです。

以下の機能を実装しています。

- i8080
- 16KB メモリ
- ROM (本体、L2BASIC、BS Monitor、拡張ROM)
- 本体 LED ディスプレイ
- 本体 キーボード
- 画面出力(NTSC)
- BS キーボード
- ブザー出力(TK-80 本体の 8255 PC1)
- テープ出力(BS のみ)

回路図は、[JR-100 エミュレータ](https://github.com/shippoiincho/jr100emulator_ch32v203)を参考にしてください。

# 操作方法

USBキーボードで本体・BSの両方のキーボードをエミュレートします。

キーボードは TK-80 モードと BS モードで処理が違います。
`ESC` キーで入力モードを切り替えます。

- TK-80 モード時は、0-9,A-F は TK-80 のキー入力として処理されます。
- TK-80 モード時は、複数のキー入力を受け付けますが、BS モード時には一つしか受け付けません。
- BS モード時でも、TK-80 のファンクションキーは有効です。
- BS モード時は `ひらがな/カタカナ` キーでカナ入力モードに切り替えます。
- BS モード時は `Pause/Break` キーは Break として動作します。
- キーリピートは未実装です。

ファンクションキーは以下のように動作します

- F1: RET
- F2: RUN
- F3: STORE DATA (テープ出力として動作しません)
- F4: LOAD DATA (テープ入力として動作しません)
- F5: ADDR SET
- F6: READ INC
- F7: READ DEC
- F8: WRITE INC
- F9: L1 BASIC/L2 BASIC 切り替え(TKモード時)
- F11: AUTO/STEP 切り替え(TKモード時)
- F12: RESET (TKモード時)

# ROM

実機の TK-80/BS の ROM が必要です。
`tk80rom_dummy.h` を `tk80rom.h` にリネームの上、ROMのデータを入れる必要があります。
L1 BASIC と L2 BASIC は、'F9' キーで切り替えできますが、当然ですが BASIC が動作していないときに切り替えないと暴走します。
なお[eTK-80BS のサイトで公開されている BS 互換 ROM](http://takeda-toshiya.my.coocan.jp/tk80bs/index.html)は、
 eTK-80BS の動作に特化しているので、そのままでは使用できません。

# テープ

BS のテープ出力に対応しています。
たんに 8251 の入出力を USART2 に変換しているだけです。
ロードする際には、ターミナルソフトの設定で入力間隔を 1ms くらいに設定してください。

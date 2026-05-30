# Alarm app for DS(i)

DSi向けの目覚ましアラームアプリです。
DSでも動作すると思いますが、検証不十分です。
RTC割込みと、スリープ処理を省電力化するために、devkitpro/calicoの `pm.c` を独自に改変して実装しています。

## 開発者より
久しぶりにDSiを引っ張り出して、アラームアプリを作りました。
ほぼ自分用なのですが、せっかくなので公開します。
同じようにDSiの開発に興味がある方や、モノづくりが好きな方とぜひ情報交換したいです！
また、ニッチなガジェット改造や、自作アプリ開発が好きな人とも繋がりたいなと思っています。

「コードのここどうなってるの？」といった質問や、改善の提案など、IssueやPull Request、コメントやリプライ、[X（旧Twitter）](https://x.com/miri_harusamee)または[discord](https://discord.gg/5JBwXjN53j)までお気軽にどうぞ！

## 使い方
<img width="258" height="386" alt="screenshot" src="https://github.com/user-attachments/assets/2d8f3ff2-75ab-4415-a2df-0733ae9d285a" />
<img width="258" height="385" alt="screenshot_alarm" src="https://github.com/user-attachments/assets/301539f9-18a7-450b-9d80-05f440762286" />
<img width="258" height="385" alt="screenshot_snooze" src="https://github.com/user-attachments/assets/0ac4121d-881a-40cd-963d-83b50f9e3d82" />

- アラームはBボタンを押すと止まり、スヌーズが設定されていれば自動的に次のスヌーズの時間にアラームがまたセットされます。スヌーズを解除するには、Aボタンを長押ししてください。
- 何もアラームを設定していない状態で本体を閉じると警告音（アラーム）が鳴ります。
- タッチパネルが使えなくても、十字キーまたはABXYキーでカーソル移動・設定ができるようになっています。
- 画面が片方映らなくても、SELECTキーで上下画面の表示を切り替えられます。
- LRキーは使用していません。
- ちなみにアラームを止めるには、本体を開閉することでも可能です。

## Credits & Licenses
このプロジェクトは以下の素晴らしいオープンソースライブラリ・ツールセットを使用（一部改変）しています。
- [devkitPro / libnds](https://devkitpro.org/) (zlib License)
- [Calico](https://github.com/devkitPro/calico) (ZPL 2.1) - ※一部ソースコード(`pm.c`)を改変して同梱しています。

それぞれのライセンスの詳細は libnds_license.txt、calico_COPYING.txt をご確認ください。

## 使用フォント
時刻表示用：コーポレート・ロゴ ver2

その他文字：MigMix 2P

# Alarm app for DS(i)

DSi向けの目覚ましアラームアプリです。
DSでも動作すると思いますが、検証不十分です。
RTC割込みと、電源周りの処理を省電力化するために、devkitpro/calicoの `pm.c` を独自に改変して実装しています。

## 開発者より
久しぶりにDSiを引っ張り出してきて、いろいろ詰め込んだアラームアプリを作ってみました。

同じようにDSiの開発に興味がある方や、モノづくりが好きな方とぜひ情報交換したいです！
また、ニッチなガジェット改造や、自作アプリ開発が好きな人とも繋がりたいなと思っています。

「コードのここどうなってるの？」といった質問や、改善の提案など、IssueやPull Request、コメントやリプライ、またはX（旧Twitter） [@harusameeeeeey](https://x.com/harusameeeeeey) までお気軽にどうぞ！

## Credits & Licenses
このプロジェクトは以下の素晴らしいオープンソースライブラリ・ツールセットを使用（一部改変）しています。
- [devkitPro / libnds](https://devkitpro.org/) (zlib License)
- [Calico](https://github.com/devkitPro/calico) (ZPL 2.1) - ※一部ソースコード(`pm.c`)を改変して同梱しています。

それぞれのライセンスの詳細は libnds_license.txt、calico_COPYING.txt をご確認ください。
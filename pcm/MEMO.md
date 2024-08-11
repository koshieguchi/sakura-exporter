# Bazel 途中経過 2202407.31

- pcmのbinaryをinsatllし、externalに展開することはできた
- しかし、pcmのlibraryはCMakeLists.txtにinstallするための手順が記述されていない
- そのため、cmake()でもそのルールを記述することができず、結果としてstatic libraryを利用できない

# 解決策

1. bazelでなんらかの方法でlibraryをinsatllできるように設定する（コマンドラインでinsatllの指定するのは無理そうだ）
2. pcmをforkし、CMakeLists.txtを追加・編集する（一番楽ではある）
3. bazelを使わずにCMakeまたはMakeを自力で書いて、bildする（一番現実的、楽ではある）

今回のlibraryのbuild errorはpcm側の問題なので、bazel側で解決するのが難しい、、

# 外部リポジトリとして、pcmを使ってinstallする場合

```bash
1. clone pcm
2. build pcm
3. ディレクトリ内にMakeファイルなどを作成して、pcmのlibpcm.aをincludeするようにする
```

## CPU architecture
- x1: Detected Intel(R) Xeon(R) Gold 6130 CPU @ 2.10GHz "Intel(r) microarchitecture codename Skylake-SP" stepping 4 microcode level 0x2007006
  - SKX: PurleyPlatform
- login: INTEL(R) XEON(R) PLATINUM 8580
  - SPR: Eagle
- gpu: Intel(R) Xeon(R) Platinum 8480+
  - SPR: Eagle

# DayNote —— 一台开口就能记事的 AI 电台终端

[中文](#中文) ｜ [日本語](#日本語) ｜ [English](#english)

## 中文

参赛队伍：hehaohanBI4MIB（何浩瀚，呼号 BI4MIB）
赛道：AI 硬件产品创新
运行平台：openvela（NuttX）on Gemini-S1，主控 Allwinner R528

### 这东西是干嘛的

在地里干活、野外架台、蹲在电台桌前的时候，手基本没空——握锄头、拧旋钮、捏话筒，
想记点什么只能靠脑子，回头就忘。DayNote 就是冲着这个来的：对着它把话说出来，
它自己录音、转成文字、分出类型、写好摘要，存成一条以后能翻、能搜的笔记。

它同时是个业余电台的网络终端：接一块自制的 AI Radio HAT，就能跑 MMDVM/DMR
数字模式，通话记录同样落到本地。两件事共用一套语音链路和一块 320×240 的屏。

### 设计上的几个取舍

- **端侧优先，网络是加分项。** 录音、VAD、本地分类、检索这些没网也得能用；
  ASR 和 LLM 放在云端，但请求走的是 OpenAI 兼容接口，换服务只改配置不改代码。
- **站在官方框架上写，不重造轮子。** 应用基于 openvela 的 ai_agent / mini_memo
  重构，LLM 路由、语音通道、消息总线都用框架的，自己只写业务层；nuttx、packages
  这些生产仓不动。
- **交互按小屏来。** 没有键盘，所有操作落在五个可滑动的页面和一个 PTT 按键上，
  信息以卡片呈现，一屏看完一条。

### 实际实现了什么

**语音记录**
- 两种触发：按住 PTT 录音；或后台 VAD 自动分段（RMS 能量阈值，约 60ms 语音起录、
  1.5s 静音停止，单段 0.5–30s），两条路互斥、共用麦克风。
- 16kHz/16bit/单声道采样，自动加 WAV 头。
- ASR：默认 SiliconFlow 的 `FunAudioLLM/SenseVoiceSmall`，multipart 直传；
  也可换成任何 OpenAI 兼容的 `/v1/audio/transcriptions` 端点。
- 理解：转录文本交给 LLM，输出类型（memo/todo/schedule）、短标题、摘要、关键词，
  日程类给出提醒时间；LLM 不通时用本地关键词规则兜底，记录不丢。

**界面与检索**
- LVGL 五页 tile：Home（时间线 + 统计）、Voice（录音）、Review（检索回看）、
  Settings、DMR（热点状态与通话记录）。
- FreeType 运行时加载 TTF 中文字体；笔记按时间线和卡片展示。
- 关键词 / 时间 / 类型检索全部在本机完成，数据不出设备。

**电台部分**
- MMDVM 网络客户端：连接 master、心跳、DMR 帧解析、当前通话状态，通话记录以 JSON
  持久化，重启不丢。
- BLE 配网与命令通道：Wi-Fi 配置、LLM/ASR/MMDVM 参数都从蓝牙下发，无需接键盘。

### 代码结构

```text
app/ai_radio_console/
├── daynote_main.c      入口、主循环、BLE 命令处理
├── daynote_core.c      笔记存储、本地分类、LLM 调用、VAD 线程
├── daynote_ui.c        五页 LVGL 界面
├── daynote_asr.c       SiliconFlow ASR 后端（TLS + multipart）
├── daynote_mmdvm.c     MMDVM 客户端、DMR 解析、通话记录
├── daynote_*.h
├── agent_skills/       三个技能的规格文件（见下）
├── Kconfig / Makefile / CMakeLists.txt
└── subset_font.py      字库裁剪工具
quickapp/ai_radio/      配套快应用（手机/手表端联动）
board/contest_board/    Gemini-S1 板级配置与 defconfig
hardware/ai_radio_hat/  扩展板工程（KiCad / 嘉立创 EDA）
logs/HaohanHe/          AI Coding 日志
```

### Agent Skills

三个技能以 Markdown 规格放在 `app/ai_radio_console/agent_skills/`，供 ai_agent
框架按意图加载，内容包含触发条件、调用步骤、降级策略和示例。

| 技能 | 什么时候触发 | 做什么 |
|---|---|---|
| daynote-record | "记录一下"、按 PTT、VAD 分段 | 录音 → ASR → LLM 分类摘要 → 写入 memos.json；断网走本地规则 |
| daynote-search | "找一下关于 X 的记录"、Review 页检索 | 按关键词/时间/类型端侧检索，卡片展示 |
| daynote-summary | "总结一下今天"、周期回顾（默认 4 小时） | 聚合当天笔记，输出高光、待办、关键词与日报 |

### 编译与烧录

拉取整套工程，本仓 manifest 会把各目录软链进编译树：

```bash
repo init -u https://github.com/open-vela/contest2026_095_hehaohanBI4MIB \
  -b dev-ai-contest-2026 -m contest2026_095_hehaohanBI4MIB.xml
repo sync -c -j8
```

在 openvela 工作区根目录（本仓的上一级）构建：

```bash
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay -j8
cd vendor/allwinnertech/lichee && source envsetup.sh && lunch_nuttx && pack
```

应用由 Kconfig 开关 `LVX_USE_DEMO_CONTEST2026_095_AI_RADIO_CONSOLE` 控制，
依赖 `EXAMPLES_AI_AGENT_VELA` 或 `VELACLAW_DAEMON`，自动选择 cJSON；
板级能力（BLE、音频栈、FreeType、mbedTLS、libuv、网络等）已在
`board/contest_board/configs/nsh/defconfig` 打开。

### 首次运行：通过 BLE 下发配置

连上设备的 BLE 后，以 JSON 发命令，参数会落到本地 config store，重启保留。

LLM / ASR：

```json
{
  "cmd": "llm_config",
  "api_key": "sk-你的大模型密钥",
  "asr_api_key": "sk-你的ASR密钥",
  "host": "api.siliconflow.cn",
  "path": "/v1/chat/completions",
  "model": "Qwen/Qwen2.5-7B-Instruct"
}
```

MMDVM：

```json
{
  "cmd": "mmdvm_config",
  "dmr_id": 460xxxxxx,
  "callsign": "BI4MIB",
  "master": "master 地址",
  "password": "密码"
}
```

字体：把 TTF（建议 5MB 以内，如思源黑体）放到 `/data/daynote/font.ttf`，
启动时自动加载。

### 数据存储

- 笔记：`/data/daynote/memos.json`，每条含
  id / type / content / transcript / summary / keywords / timestamp / duration_ms / is_read；
  上限 100 条，自动淘汰最旧的已读条目。
- 通话记录：MMDVM 模块单独以 JSON 保存。
- 数据目录可在 Kconfig 的 `DAYNOTE_DATA_DIR` 修改。

### AI Radio HAT

80mm×50mm 双层扩展板（KiCad 7 与嘉立创 EDA 双工程）：电台 LINEIN 进 R528 供 ASR、
R528 HPOUT 出至电台 MIC 做数字语音、光耦隔离 PTT、CAT 串口、I2C 传感器扩展。
工程文件、原理图、BOM 与接线表见 `hardware/ai_radio_hat/`。

### 验证状态与已知问题

如实说明，不把没验证的说成已完成：

1. 应用侧代码（约 5,000 行）功能闭环，VAD、ASR、MMDVM 均为真实实现而非桩；
   但完整的干净环境编译还依赖配套的 ai_agent / velaclaw 组件，从 `repo sync` 到
   出固件的全链路尚需在 Linux 环境复跑确认。
2. VAD 的 RMS 阈值（当前 500）是经验值，不同麦克风增益下要实机标定。
3. 电阻屏坐标转换需要实机校准。
4. AI Radio HAT 已完成原理图与 BOM，PCB 布局布线和打样是下一步。
5. 获奖后按要求把作品 PR 到 openvela 上游对应仓库，走标准 PR + CI。

### AI Coding 日志

开发期间与 AI 工具的对话记录在 `logs/HaohanHe/`，含会话清单 `manifest.json`。
本次主要在 Trae（MiMo-V2.5）中开发，不在官方日志归集工具支持列表内，
无法导出 JSONL，故提交 Markdown 原始全文，说明见该目录。

---

## 日本語

DayNote は、声を出すだけでメモが取れる AI 無線機端末です。

- チーム：hehaohanBI4MIB（何浩瀚、コールサイン BI4MIB）
- 部門：AI ハードウェア製品イノベーション
- 実行プラットフォーム：openvela（NuttX）on Gemini-S1、SoC は Allwinner R528

### これは何をするものか

畑仕事、フィールドでのアンテナ設営、無線機前での運用中は、手が離せません。
鍬を持ち、ツマミを回し、マイクを握っている状態で、何かをメモしたくても
頭で覚えておくしかなく、後で忘れてしまいます。DayNote はそのための端末です。
向かって話しかけると、録音し、文字起こしし、種別を分け、要約を書き、あとで
見返せて検索できるノートとして保存します。

同時にアマチュア無線のネットワーク端末でもあります。自作の AI Radio HAT を
載せると MMDVM/DMR デジタルモードが動き、通話記録も同じようにローカルへ
落ちます。音声経路と 320×240 の画面を両者で共有します。

### 設計上の割り切り

- **端末側を優先、ネットワークはオプション。** 録音、VAD、ローカル分類、
  検索はオフラインでも動く必要があります。ASR と LLM はクラウド側ですが、
  OpenAI 互換エンドポイント経由で叩くので、サービスを変えてもコードではなく
  設定だけ差し替えます。
- **公式フレームワークの上に書き、車輪の再発明をしない。** アプリは openvela
  の ai_agent / mini_memo をベースに重构しました。LLM ルーティング、音声
  チャネル、メッセージバスはフレームワークのものを使い、業務層だけを書き
  ます。nuttx、packages といった本番リポジトリは触りません。
- **小画面に合わせた操作。** キーボードはなく、操作はスワイプ可能な 5 枚の
  ページと 1 つの PTT ボタンに集約し、情報はカード表示で一画面 1 件を
  基本とします。

### 実際に実装したもの

**音声メモ**
- トリガは 2 通り：PTT を押している間だけ録音、またはバックグラウンドの
  VAD 自動セグメンテーション（RMS エネルギーしきい値、約 60ms で録音開始、
  1.5s 無音で停止、1 セグメント 0.5–30s）。両者は排他でマイクを共有します。
- 16kHz / 16bit / モノラル、WAV ヘッダを自動付与します。
- ASR：デフォルトは SiliconFlow の `FunAudioLLM/SenseVoiceSmall` に
  multipart で直接 POST します。OpenAI 互換の `/v1/audio/transcriptions`
  エンドポイントであれば差し替え可能です。
- 理解：書き起こしテキストを LLM に渡し、種別（memo/todo/schedule）、短い
  タイトル、要約、キーワードを出力させ、スケジュール系にはリマインド時刻を
  持たせます。LLM が不通のときはローカルのキーワードルールで兜底し、記録を
  失いません。

**UI と検索**
- LVGL の 5 枚タイル：Home（タイムライン + 統計）、Voice（録音）、
  Review（検索・見返し）、Settings、DMR（ホットスポット状態と通話履歴）。
- FreeType が TTF 中国語フォントを実行時に読み込みます。ノートはタイムライン
  とカードで表示します。
- キーワード / 時刻 / 種別による検索はすべて端末側で完結し、データは端末外へ
  出ません。

**無線部分**
- MMDVM ネットワーククライアント：master への接続、ハートビート、DMR フレーム
  解析、通話中ステータスを扱い、通話履歴は JSON で永続化し、再起動しても
  失われません。
- BLE によるネットワーク設定とコマンドチャネル：Wi-Fi 設定、LLM/ASR/MMDVM
  のパラメータはすべて Bluetooth 経由で配信し、キーボードを繋ぐ必要をなくし
  ます。

### コード構成

```text
app/ai_radio_console/
├── daynote_main.c      エントリ、メインループ、BLE コマンド処理
├── daynote_core.c      ノート保存、ローカル分類、LLM 呼出、VAD スレッド
├── daynote_ui.c        5 ページ LVGL UI
├── daynote_asr.c       SiliconFlow ASR バックエンド（TLS + multipart）
├── daynote_mmdvm.c     MMDVM クライアント、DMR 解析、通話履歴
├── daynote_*.h
├── agent_skills/       3 つのスキル定義（後述）
├── Kconfig / Makefile / CMakeLists.txt
└── subset_font.py      フォントサブセットツール
quickapp/ai_radio/      対クイックアプリ（スマホ / ウォッチ連携）
board/contest_board/    Gemini-S1 ボード設定と defconfig
hardware/ai_radio_hat/  拡張ボード CAD（KiCad / 嘉立創 EDA）
logs/HaohanHe/          AI Coding ログ
```

### Agent Skills

3 つのスキルは Markdown 仕様として `app/ai_radio_console/agent_skills/` に
置かれ、ai_agent フレームワークが意図に応じて読み込みます。内容は発動条件、
呼び出し手順、縮退戦略、例を含みます。

| スキル | 発動条件 | やること |
|---|---|---|
| daynote-record |「記録して」、PTT 押下、VAD セグメント | 録音 → ASR → LLM 分類・要約 → memos.json へ書き込み；オフライン時はローカルルール |
| daynote-search |「X についての記録を探して」、Review ページでの検索 | キーワード / 時刻 / 種別で端末内検索、カード表示 |
| daynote-summary |「今日をまとめて」、定期レビュー（既定 4 時間） | 当日ノートを集約し、ハイライト、ToDo、キーワード、日報を出力 |

### ビルドと書き込み

リポジトリ一式を取得します。このリポジトリの manifest が各ディレクトリを
ビルドツリーへシンボリックリンクします。

```bash
repo init -u https://github.com/open-vela/contest2026_095_hehaohanBI4MIB \
  -b dev-ai-contest-2026 -m contest2026_095_hehaohanBI4MIB.xml
repo sync -c -j8
```

openvela ワークスペースのルート（このリポジトリのひとつ上）でビルドします。

```bash
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay -j8
cd vendor/allwinnertech/lichee && source envsetup.sh && lunch_nuttx && pack
```

アプリは Kconfig スイッチ `LVX_USE_DEMO_CONTEST2026_095_AI_RADIO_CONSOLE` で
制御され、`EXAMPLES_AI_AGENT_VELA` または `VELACLAW_DAEMON` に依存し、
cJSON を自動選択します。ボード側の機能（BLE、オーディオスタック、FreeType、
mbedTLS、libuv、ネットワーク等）は
`board/contest_board/configs/nsh/defconfig` で有効化済みです。

### 初回起動：BLE 経由で設定を流し込む

端末の BLE に接続したら JSON コマンドを送ります。パラメータはローカルの
config store に保存され、再起動後も保持されます。

LLM / ASR：

```json
{
  "cmd": "llm_config",
  "api_key": "sk-あなたの大規模モデルキー",
  "asr_api_key": "sk-あなたのASRキー",
  "host": "api.siliconflow.cn",
  "path": "/v1/chat/completions",
  "model": "Qwen/Qwen2.5-7B-Instruct"
}
```

MMDVM：

```json
{
  "cmd": "mmdvm_config",
  "dmr_id": 460xxxxxx,
  "callsign": "BI4MIB",
  "master": "master アドレス",
  "password": "パスワード"
}
```

フォント：TTT（5MB 以内、Source Han Sans など推奨）を
`/data/daynote/font.ttf` に置くと、起動時に自動で読み込みます。

### データ保存

- ノート：`/data/daynote/memos.json`。1 件あたり
  id / type / content / transcript / summary / keywords / timestamp /
  duration_ms / is_read を持ちます。上限 100 件で、既読の古いエントリから
  自動的に淘汰します。
- 通話履歴：MMDVM モジュールが独立した JSON で保存します。
- データディレクトリは Kconfig の `DAYNOTE_DATA_DIR` で変更できます。

### AI Radio HAT

80mm × 50mm の 2 層拡張ボード（KiCad 7 と嘉立創 EDA のデュアルプロジェクト）
です。無線機 LINEIN を R528 へ入れて ASR に使い、R528 HPOUT を無線機 MIC
へ出してデジタル音声を送り、PTT はフォトカプラで絶縁し、CAT シリアルと
I2C センサ拡張を備えます。プロジェクトファイル、回路図、BOM、結線表は
`hardware/ai_radio_hat/` にあります。

### 検証状況と既知の問題

検証していないものを済んだと言わないように、正直に記載します。

1. アプリ側コード（約 5,000 行）は機能的に閉じており、VAD、ASR、MMDVM は
   スタブではなく実装です。ただしクリーン環境での完全ビルドは ai_agent /
   velaclaw コンポーネントに依存するため、`repo sync` からファームウェア
   出力までの全リンを Linux 環境で再確認する必要が残っています。
2. VAD の RMS しきい値（現在 500）は経験値で、マイクゲインが変わると
   実機での再調整が必要です。
3. 抵抗膜タッチパネルの座標変換は実機校正が必要です。
4. AI Radio HAT は回路図と BOM が完了しており、PCB パターン設計と試作が
   次のステップです。
5. 入賞後は規定に従い、作品を openvela 上流の該当リポジトリへ PR し、
   標準の PR + CI フローに乗せます。

### AI Coding ログ

開発中の AI ツールとの対話ログは `logs/HaohanHe/` にあり、会話一覧の
`manifest.json` を含みます。今回は主に Trae（MiMo-V2.5）で開発しました。
公式のログ収集ツールの対応リストに入っていないため JSONL を出力できず、
Markdown の原文全文をそのまま提出しています。詳細は当該ディレクトリを
参照してください。

---

## English

DayNote is a voice-first AI radio terminal: you talk to it, it turns the talk
into notes.

- Team: hehaohanBI4MIB (He Haohan, callsign BI4MIB)
- Track: AI hardware product innovation
- Platform: openvela (NuttX) on the Gemini-S1 board, SoC Allwinner R528

### What it is for

When you are working in a field, setting up an antenna outdoors, or sitting at
the radio desk, your hands are busy: holding a hoe, turning knobs, holding the
microphone. If you want to remember something, you have to keep it in your head
and you will likely forget later. DayNote is built for that moment. You speak
to it; it records, transcribes, labels the type, writes a summary, and stores
the result as a note you can later browse and search.

It is also an amateur radio network endpoint. With a custom AI Radio HAT
attached, it runs MMDVM/DMR digital modes, and the call log is written to the
same local store. The two jobs share one audio path and one 320x240 screen.

### Design choices

- **On-device first, network as a bonus.** Recording, VAD, local
  classification, and search must work offline. ASR and the LLM live in the
  cloud, but they are called over an OpenAI-compatible endpoint, so switching
  providers is a config change, not a code change.
- **Stand on the official framework, do not reinvent it.** The app is built on
  top of openvela's ai_agent / mini_memo. LLM routing, the audio channel, and
  the message bus come from the framework; only the business layer is written
  here. Production trees like nuttx and packages are not touched.
- **Interaction sized for a small screen.** There is no keyboard. Everything
  lives on five swipeable pages and one PTT button. Information is rendered as
  cards, one note per screen.

### What is actually implemented

**Voice notes**
- Two triggers: hold the PTT button to record, or let background VAD segment
  audio automatically (RMS energy threshold, roughly 60ms to start recording
  on speech, 1.5s of silence to stop, 0.5-30s per segment). The two paths are
  mutually exclusive and share the microphone.
- 16kHz / 16-bit / mono, with a WAV header written automatically.
- ASR: defaults to SiliconFlow's `FunAudioLLM/SenseVoiceSmall`, uploaded as
  multipart. Any OpenAI-compatible `/v1/audio/transcriptions` endpoint can
  be swapped in.
- Understanding: the transcript goes to an LLM, which returns a type
  (memo/todo/schedule), a short title, a summary, and keywords; schedule-type
  notes carry a reminder time. If the LLM is unreachable, a local keyword
  fallback runs so a note is never lost.

**UI and search**
- Five LVGL pages: Home (timeline + stats), Voice (recording), Review
  (search and browse), Settings, DMR (hotspot status and call log).
- FreeType loads a Chinese TTF at runtime. Notes are shown as a timeline and
  as cards.
- Keyword / time / type search all runs on the device; data never leaves the
  box.

**Radio side**
- MMDVM network client: connects to a master, sends heartbeats, parses DMR
  frames, tracks the current call state, and persists the call log as JSON so
  it survives a reboot.
- BLE provisioning and command channel: Wi-Fi credentials and LLM/ASR/MMDVM
  parameters are pushed over Bluetooth, so no keyboard has to be attached.

### Code layout

```text
app/ai_radio_console/
├── daynote_main.c      entry, main loop, BLE command handling
├── daynote_core.c      note storage, local classification, LLM calls, VAD thread
├── daynote_ui.c        five-page LVGL UI
├── daynote_asr.c       SiliconFlow ASR backend (TLS + multipart)
├── daynote_mmdvm.c     MMDVM client, DMR parsing, call log
├── daynote_*.h
├── agent_skills/       three skill spec files (see below)
├── Kconfig / Makefile / CMakeLists.txt
└── subset_font.py      font subsetting tool
quickapp/ai_radio/      companion quick app (phone / watch side)
board/contest_board/    Gemini-S1 board config and defconfig
hardware/ai_radio_hat/  carrier board CAD (KiCad / JLC EDA)
logs/HaohanHe/          AI coding logs
```

### Agent skills

Three skills live as Markdown specs under
`app/ai_radio_console/agent_skills/` and are loaded by the ai_agent
framework based on intent. Each spec lists trigger conditions, call steps,
degradation strategy, and examples.

| Skill | Trigger | What it does |
|---|---|---|
| daynote-record | "note that", PTT press, VAD segment | record -> ASR -> LLM classify/summarize -> append to memos.json; offline path uses local rules |
| daynote-search | "find the note about X", Review page search | on-device search by keyword/time/type, shown as cards |
| daynote-summary | "summarize today", periodic review (default 4h) | aggregate the day's notes into highlights, todos, keywords, and a daily report |

### Build and flash

Pull the whole workspace. The manifest in this repo symlinks the per-directory
pieces into the build tree:

```bash
repo init -u https://github.com/open-vela/contest2026_095_hehaohanBI4MIB \
  -b dev-ai-contest-2026 -m contest2026_095_hehaohanBI4MIB.xml
repo sync -c -j8
```

Build at the openvela workspace root (one level above this repo):

```bash
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay -j8
cd vendor/allwinnertech/lichee && source envsetup.sh && lunch_nuttx && pack
```

The app is gated by the Kconfig switch
`LVX_USE_DEMO_CONTEST2026_095_AI_RADIO_CONSOLE`, depends on
`EXAMPLES_AI_AGENT_VELA` or `VELACLAW_DAEMON`, and auto-selects cJSON. Board
features (BLE, audio stack, FreeType, mbedTLS, libuv, networking, etc.) are
already enabled in `board/contest_board/configs/nsh/defconfig`.

### First boot: push config over BLE

Once you are connected to the device over BLE, send JSON commands. Parameters
are written to a local config store and survive a reboot.

LLM / ASR:

```json
{
  "cmd": "llm_config",
  "api_key": "sk-your-llm-key",
  "asr_api_key": "sk-your-asr-key",
  "host": "api.siliconflow.cn",
  "path": "/v1/chat/completions",
  "model": "Qwen/Qwen2.5-7B-Instruct"
}
```

MMDVM:

```json
{
  "cmd": "mmdvm_config",
  "dmr_id": 460xxxxxx,
  "callsign": "BI4MIB",
  "master": "master address",
  "password": "password"
}
```

Font: drop a TTF (recommended under 5MB, e.g. Source Han Sans) at
`/data/daynote/font.ttf`; it is loaded at boot.

### Data storage

- Notes: `/data/daynote/memos.json`. Each entry has
  id / type / content / transcript / summary / keywords / timestamp /
  duration_ms / is_read. The cap is 100 entries; the oldest already-read
  entries are evicted automatically.
- Call log: kept by the MMDVM module as a separate JSON file.
- The data directory can be changed via the Kconfig `DAYNOTE_DATA_DIR`.

### AI Radio HAT

An 80mm x 50mm two-layer carrier board (dual CAD projects in KiCad 7 and JLC
EDA). Radio LINEIN feeds the R528 for ASR; R528 HPOUT feeds the radio MIC for
digital voice; PTT is opto-isolated; there is a CAT serial line and I2C sensor
expansion. Project files, schematic, BOM, and wiring table are under
`hardware/ai_radio_hat/`.

### Verification status and known issues

Stated honestly rather than overclaimed:

1. The app-side code (~5,000 lines) is a closed loop. VAD, ASR, and MMDVM are
   real implementations, not stubs. However, a fully clean-environment build
   still depends on the companion ai_agent / velaclaw components, and the
   end-to-end path from `repo sync` to a flashed image needs to be re-run on a
   Linux host to confirm.
2. The VAD RMS threshold (currently 500) is empirical and must be recalibrated
   on hardware for different microphone gains.
3. Resistive touch coordinates need on-device calibration.
4. The AI Radio HAT schematic and BOM are done; PCB layout and fabrication are
   the next step.
5. If the project wins, it will be PR'd back to the matching openvela upstream
   repository through the standard PR + CI flow.

### AI coding logs

Conversation logs with AI tools during development live under
`logs/HaohanHe/`, with a `manifest.json` index. Development was done mostly
in Trae (MiMo-V2.5), which is not on the official log-collection tool list,
so JSONL export was not available. The raw Markdown transcripts are committed
as-is; see that directory for notes.

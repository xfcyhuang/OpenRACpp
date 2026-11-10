# PORTING_PLAN —— 分阶段移植计划

**当前状态：M2 达成（2026-11-10）—— Phase 0-7 完成；M3（Phase 8 AI）研究先行（#46）**

## 目标与原则

- **范围（终态：全移植）**：上游引擎七程序集（Game / Mods.Common / Mods.Cnc / Mods.D2k / Platforms.Default / Server / Utility）+ ra/cnc/d2k/ts 四 mod + Launcher（GUI，Dear ImGui）。
- **原则**：语义等价第一原则，不照抄（见 [ADR-0001](docs/adr/0001-semantic-equivalence-first.md)）。
- **路线**：垂直切片优先，先通沙盒对局再横向补全（见 [ADR-0002](docs/adr/0002-vertical-slice-first.md)）。
- **编码**：`../cpp26.md` 为准；裸 ctest 偏离见 [ADR-0003](docs/adr/0003-bare-ctest-no-framework.md)。
- **平台**：Windows 优先（llvm-mingw Clang 23 / UCRT / WinSock2）；POSIX 留接口列 CI 待办。
- **上游**：`../openra`，基线钉定见 [UPSTREAM.baseline](UPSTREAM.baseline)，同步手动发起。
- **验证**：ra 为主线 mod；三层验证 = 字节恒等 + oracle 回放逐帧 SyncHash + ctest 三编排（base/asan/hardened）。

## 里程碑 M1：CLI 直进沙盒对局可玩（2026-11-07 达成，Phase 0-5）

加载 ra 固定地图 → 直进对局 → 可框选/移动/建造/生产/交战的基础 HUD 侧栏；静音桩、无 AI、无菜单 UI。oracle 真回放 580 帧逐帧 SyncHash 全对收口。

## 里程碑 M2：菜单→大堂→对局全流程可走（2026-11-10 达成，Phase 6-7）

音频去静音（OpenAL/Dummy 一等公民回落）、热键注册表与设置持久化生效面（windowed_size/vsync）；Widget 框架与 chrome 装载、主菜单三页设置、遭遇战大堂（预览/槽位/势力）；FlowState 状态机串联菜单→大堂→对局全流程（abort 重连大堂、disconnect 回菜单）。

## 里程碑 M3：遭遇战完整形态（进行中，Phase 8，研究 #46 / 决策 #47）

bot = ModularBot trait 体系移植（上游无 NullBot）；lua = vendored Lua 5.1.5 + 绑定最小通路（上游 Eluant 实证，ADR-0005）——两正交面。验收双口径：①无头 bot vs bot 逐帧 SyncHash 黄金全等（回放重演先行、live 定种子收口）②人类 vs bot 经大堂 bot 槽开局可玩。切票 T30-T40（#47 认可）。

## 阶段表

每阶段退出条件：本阶段验收项通过 + ctest 三编排全绿。

| 阶段 | 内容 | 验收 |
|------|------|------|
| **Phase 0** 骨架 | CMake 根 + `base/asan/hardened` 三 preset + `std.cppm` PCM 共享 + `ora::<assembly>` 命名空间骨架 + `docs/COVERAGE.md`/`upstream.md` 空表 + `tools/golden_gen`（dotnet 10 无头宿主）脚手架 | 空测试三编排全绿；golden_gen 可产/校验一份最小产物 |
| **Phase 1** 数据层最小集 | MiniYaml 解析、ra 规则加载、`.oramap` 地图读取、素材索引最小面、确定性随机数 | 规则/地图加载单测 + yaml dump 与 oracle 字节恒等 |
| **Phase 2** 引擎核心最小集 | World/Tick 循环、Actor/Trait 装配最小面、订单管道、Actor 创建/销毁 | 无头跑固定种子 N tick，状态摘要自洽 |
| **Phase 3** 渲染与输入最小集 | SDL2+OpenGL 窗口、地形/精灵绘制、框选与指令输入、ra 侧栏 HUD 最小面 | 固定地图可视可交互（手动验收 + 渲染截图对拍） |
| **Phase 4** gameplay 最小闭环 | 建造/生产/移动/攻击/资源/电力最小闭环、CLI 直进参数 | 沙盒对局可玩（M1 功能面齐） |
| **Phase 5** 确定性关卡 | golden_gen 产真 .orarep（固定地图固定种子 ~150 帧）→ 逐帧 SyncHash 全对 + desync 篡改负向样本；回放/地图读写字节恒等 | **M1 达成** |
| **Phase 6** 平台层补全 | 音频（去静音桩）、输入全量、显示设置与持久化 | 音效/音乐生效；设置往返一致 |
| **Phase 7** UI 体系 | Widget 框架、主菜单、遭遇战大堂（地图预览/槽位/设置面板） | 菜单→大堂→对局全流程可走 |
| **Phase 8** AI | bot = ModularBot trait 体系移植（上游无 NullBot：12 BotModule、ra 4 类型）+ 三欠账兑现（Move Activity 化/AutoTarget rng/HPF）+ 大堂 bot 槽；lua = vendored Lua 5.1.5 + 绑定最小通路（正交面，ADR-0005） | 遭遇战完整形态：bot vs bot 逐帧 SyncHash 黄金全等 + 人类 vs bot 大堂开局可玩 |
| **Phase 9** 网络/服务器 | Connection/Server/LobbyCommands 族、回环验收 | 127.0.0.1 回环全链验收 |
| **Phase 10** utility | OpenRA.Utility 语义（格式转换/地图工具） | 命令行工具集对拍 |
| **Phase 11** mods 增量 | cnc → d2k → ts | 各 mod 沙盒对局可玩 |
| **Phase 12** Launcher | Dear ImGui GUI（见 [ADR-0004](docs/adr/0004-launcher-dear-imgui.md)）、内容发现/拉起语义 | GUI 启动器全流程 |
| **Phase 13** 全量审计收尾 | 上游文件覆盖审计、COVERAGE 登记清零或完毕、三编排常态化 | 覆盖审计无未登记缺口 |

## 同步

上游更新的**语义合并**流程见 [UPSTREAM.baseline](UPSTREAM.baseline)；同步与阶段推进可交错，每次同步须对拍回归全绿。

# UPSTREAM 溯源登记

代码文件不承载注记(编码规范:代码文件零注释);本文件是 `// UPSTREAM:` 标注的统一承载。
增量迁移:注记随各批改写对应文件时迁入本表并从代码删除;基线 commit 见 `UPSTREAM.baseline`;校验器 `tools/upstream_check.py`。

## src/game/game.hpp

- 上游:`OpenRA.Game/Game.cs @1f594e320` + `OpenRA.Game/Support/Program.cs L14-19`
- 批注:主循环骨架逐语义(RunTime 时钟/延迟动作队列/JoinInner·JoinLocal/InnerLogicTick 的双 TickTime 节拍/LogicTick 的连接态事件/RenderTick 的 prepare→world→UI→flip 全序/Loop 的逻辑-渲染双时间表与追帧截断/Run 的 finally 清理)。Phase 5-8 依赖面走 Deps/钩子注入:Renderer/Sound/CursorManager/ChromeProvider/WorldRenderer+Viewport/Ui、Settings.Graphics 值面、NetworkConnection 的连接态观测(Phase 7)、world.OrderGenerator(Phase 5)、PerfHistory/PerfSample/Benchmark/截图(Phase 5+)、TextNotificationsManager/UnitOrders.Clear(第十六批客户端接线:JoinInner 双 Clear + LobbyInfoChanged 事件转发)—— 不在场的编排点全部 no-op。第二十六批扩面:OpenWindow×2/LoadWidget 三转发(L148-176)+ TakeScreenshot 置旗(L691/696,RenderTick L756-759 消费)+ WidgetLoader 的 Game 持有(ModData.WidgetLoader 的 D105 族分层偏离)。

## src/game/game.cpp

- 上游:`OpenRA.Game/Game.cs @1f594e320`(game.hpp 的实现)

## src/ui/ui.hpp

- 上游:`OpenRA.Game/Widgets/Widget.cs @1f594e320 L24-194`(Ui 静态门面逐语义:窗口栈/焦点三指针/输入分发/ResetAll/Tick/Draw/Mediator 转发)
- 批注:C# static 走进程级单例;窗口栈条目在 widget 被 HideChild 摘出 Root 时接管所有权(可见栈顶由 Root 持有);WidgetLoader/Renderer.Resolution/Game.RunTime/ModData 走 Deps 注入;LastMousePos/LastMoveRunTime 写 gfx::Viewport 同名静态(Viewport.cs L134-135)。第二十六批扩面:WireModData(Game.InitializeMod 的重建接线,Ui.modData 静态直赋的注入等价)+ Deps 的 fn_game_load_widget(Game.LoadWidget 分层注入,缺省回落 Ui.LoadWidget)/fn_take_screenshot。

## src/ui/ui.cpp

- 上游:`OpenRA.Game/Widgets/Widget.cs @1f594e320 L24-194`(ui.hpp 的实现)

## src/ui/widget.hpp

- 上游:`OpenRA.Game/Widgets/Widget.cs @1f594e320`(Widget 树逐语义:Ui 门面/ChromeLogic/WidgetBounds/Widget/ContainerWidget/InputWidget/WidgetArgs/Mediator)
- 批注:C# static Ui 走进程级单例(Widget 的 Outer 族经其取焦点态);子 widget 所有权归父 unique_ptr,RemoveChild/HideChild 转移出树;Mediator 的 TypeDictionary 反射接口索引换 type_index 桶 + NotificationHandlerSet 混入;PostInit 的 ObjectCreator.CreateObject 换名字分派注册表(未知名 `Cannot locate type: {name}` 逐字);FieldLoader.LoadFieldOrProperty 换 LoadFieldOrProperty 虚方法链式回退;Initialize 的 Renderer.Resolution 与 ChromeMetrics 经 WidgetArgs(windowSize/chromeMetrics)注入。第二十六批扩面:WidgetArgValue 增 `game::PlayerProfile*`(profile)与 `std::function<int(int)>`(negotiateWidth)两槽。

## src/mods/create_map_players.cpp

- 上游:`OpenRA.Mods.Common/Traits/World/CreateMapPlayers.cs @1f594e320`(实现部分)+ 各批 mods trait 的注册表装配

## src/mods/load_widget_at_game_start.hpp / .cpp

- 上游:`OpenRA.Mods.Common/Traits/World/LoadWidgetAtGameStart.cs @1f594e320`(全文)
- 批注:TraitLocation World|EditorWorld;IWorldLoaded/INotifyGameLoading/INotifyGameLoaded 三接口;Game.LoadWidget 走 Ui::Deps.fn_game_load_widget 分层注入(缺省回落 Ui.LoadWidget);GameLoaded 的 optionsButton.OnClick 经快照后入 RunUnsynced(第二十二批 UAF 教训)。

## src/mods/hotkey_logics.hpp / .cpp

- 上游:`OpenRA.Mods.Common/Widgets/Logic/SingleHotkeyBaseLogic.cs L17-36` + `MusicHotkeyLogic.cs L19-83` + `ScreenshotHotkeyLogic.cs L18-31` + `MuteHotkeyLogic.cs L18-49`(全文)
- 批注:SingleHotkeyBaseLogic 的 namedKey/logicArgs/GLOBAL_KEYHANDLER 惯例;MusicHotkey 的四键分派与 StopMusic/PauseOrResumeMusic;Screenshot 的 Game.TakeScreenshot 走 Ui::Deps.fn_take_screenshot;Mute 的 Settings.Sound.Mute 翻转 + Fluent 双反馈行。

## src/mods/local_profile_logics.hpp / .cpp

- 上游:`OpenRA.Mods.Common/Widgets/Logic/LoadLocalPlayerProfileLogic.cs L16-30` + `LocalProfileLogic.cs L18-93` + `PlayerProfileBadgesLogic.cs L17-65`(全文)
- 批注:LoadLocalPlayerProfile 的 minimalProfile = CurrentWindow 非空;LocalProfile 的 LinkState 可见域/LINK_BUTTON 转发 SETTINGS_BUTTON/RefreshBadges 的 RunAfterTick + 订阅退订(C++ 侧加 alive 令牌防 RunAfterTick 悬垂);Badges 的模板摘除保活 + negotiateWidth 宽度方程 + TruncateText;game::PlayerBadge 与 ui::PlayerBadge 的字段桥(str_icon 三元 → opt_icon 三元)。

## tests/render_test.cpp

- 上游:`OpenRA.Mods.Common/LoadScreens/BlankLoadScreen.cs`(第二十六批节的 InitializeMod→LoadShellMap→MAINMENU 装配链;chrome 树 = mods/common/chrome/mainmenu.yaml + mods/common/chrome/playerprofile.yaml)

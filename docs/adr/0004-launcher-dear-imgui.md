# Launcher GUI 采用 Dear ImGui

上游 OpenRA.Launcher 为 Avalonia GUI；C++ 侧以 **Dear ImGui** 照搬其行为形态（内容目录发现、参数装配、拉起游戏/工具/服务器）。GUI 属第三方豁免域，不受 std→Boost→自研选型顺序约束。否决：自有 DirectUI 库（成熟度待验，不为关键路径引入不确定面）、Qt（体量与许可负担）、Win32 原生（开发效率）。Launcher 在路线图中居末位，先游戏后启动器。

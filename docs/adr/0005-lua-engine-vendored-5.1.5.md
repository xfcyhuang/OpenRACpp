# lua 引擎采用 vendored Lua 5.1.5

上游 lua 引擎实测为 NuGet `OpenRA-Eluant 1.0.23` = **原生 Lua 5.1.5（lua.org C 参考）** + C# P/Invoke 绑定（非 MoonSharp 纯 C# 解释器，认知修正见 #46 §1）。C++ 侧以 **vendored Lua 5.1.5 源码入库**（静态库编入，构建期/链接期零外部依赖，vendored 入库已在 #46 裁定）为决策：与 oracle 语言语义同源（同一参考实现谱系），等价面收敛为绑定层窄点（double→int 截断/错误消息串/表遍历序），点估 1.5 人月。沙盒内 `math.random` 上游已禁（脚本随机统一并入同步随机面），libm 差异是仅存外部风险面（现语料不触发）。否决：①' vendored Lua 5.4+垫片（5.1→5.4 语言 diff 面未封闭，每处差异是潜在 SyncHash 分叉点）；② 自写子集解释器（语料外语义静默偏离不可穷举，4.5 人月）；③ 逐语义完整移植（ltable 遍历序逐 bit 复刻单点高风险，6 人月）。证据矩阵与决策树见 `.ora-explore/m3-ai/`（#46）与 #47。

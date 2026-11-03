# tools/golden_gen

dotnet 10 无头 oracle 宿主（PORTING_PLAN Phase 0 验收项），`ProjectReference` 指向上游
`../openra/OpenRA.Game/OpenRA.Game.csproj`（上游产物落上游 git 忽略目录 `openra/bin`，
不改上游 tracked 文件）。当前模式：

- `prng`：驱动上游 `OpenRA.Support.MersenneTwister` 真源，产 PRNG 黄金序列到
  `tests/golden/prng/seed_<n>.txt`（7 种子；格式见黄金文件首部注释）。

```
# 仓库根执行；<outdir> 缺省 tests/golden/prng
dotnet run --project tools/golden_gen -c Release -- <outdir>
```

后续模式（yaml-roundtrip、resolved-rules、真回放）随对应工单在此宿主上扩展。

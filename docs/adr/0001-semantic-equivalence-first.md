# 语义等价第一原则与三层验证

本项目是 OpenRA 的 C++26 重写，以**语义等价**为第一原则：一切可观测行为（数据产物、逐帧 SyncHash、规则效果）与上游一致，代码形貌不要求一致（不照抄）。等价性由三层验证背书：①数据/序列化**字节恒等**（地图、回放等与上游产物逐字节相同）；②**oracle 回放对拍**（dotnet 无头宿主产真 .orarep，C++ 引擎回放逐帧 SyncHash 全对）；③ctest 三编排（base / asan / hardened）全绿。有意优化须证据登记于 `docs/UPSTREAM_CPP_REVIEW.md`；确实无法等价之处登记 `docs/COVERAGE.md`，**未登记的偏离视为 bug**。

## Considered Options

- 逐层横向铺满再整体验证：推迟了端到端可玩性，行为漂移发现太晚。
- 纯单元测试等价：无法证明确定性序列化与逐帧状态一致。

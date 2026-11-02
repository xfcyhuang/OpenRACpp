# 裸 ctest，不引入测试框架

`cpp26.md` §七推荐 Catch2 或 Google Test，本项目有意偏离：全部测试为自写 `main` 的 ctest 可执行文件。理由：主力测试形态是黄金对拍、字节恒等、回放驱动这类自写驱动的测试，断言框架收益低，而引入框架带来 vendoring / 编译态负担，且三编排（base/asan/hardened）并行下零第三方依赖更稳。此偏离为对 `cpp26.md` 的登记例外，非规范本身修改。

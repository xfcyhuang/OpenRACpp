# 域文档（Domain Docs）

工程技能在探索本代码库时应如何消费域文档。

## 探索之前，先读这些

- 仓库根目录的 **`GLOSSARY.md`**；或
- 根目录的 **`GLOSSARY-MAP.md`**（若存在）：它为每个上下文指向一个 `GLOSSARY.md`，读取与当前主题相关的每一个。
- **`docs/adr/`**：读取涉及你即将工作区域的 ADR。多上下文仓库中，还要检查 `src/<context>/docs/adr/` 中的上下文级决策。

若这些文件不存在，**静默继续**。不要指出缺失；也不要一上来就建议创建。`/domain-modeling` 技能（经 `/grill-with-docs` 与 `/improve-codebase-architecture` 触达）会在术语或决策真正落定时惰性创建它们。

## 文件结构

单上下文仓库（绝大多数仓库）：

```
/
├── GLOSSARY.md
├── docs/adr/
│   ├── 0001-event-sourced-orders.md
│   └── 0002-postgres-for-write-model.md
└── src/
```

多上下文仓库（根目录存在 `GLOSSARY-MAP.md` 时）：

```
/
├── GLOSSARY-MAP.md
├── docs/adr/                          ← 系统级决策
└── src/
    ├── ordering/
    │   ├── GLOSSARY.md
    │   └── docs/adr/                  ← 上下文专属决策
    └── billing/
        ├── GLOSSARY.md
        └── docs/adr/
```

## 使用词汇表的词汇

当你的输出命名一个领域概念时（issue 标题、重构提案、假设、测试名），使用 `GLOSSARY.md` 中定义的术语。不要漂移到词汇表明确回避的同义词。

如果你需要的概念还不在词汇表里，这是一个信号：要么你在发明项目不使用的语言（重新考虑），要么存在真实缺口（记下来交给 `/domain-modeling`）。

## 标注 ADR 冲突

如果你的输出与既有 ADR 矛盾，显式指出而不是静默覆盖：

> _与 ADR-0007（事件溯源订单）矛盾，但值得重开，因为……_

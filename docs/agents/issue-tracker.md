# Issue 跟踪器：GitHub

本仓库的 issue 与规格（spec）以 GitHub issue 形式存在。所有操作使用 `gh` CLI。

## 约定

- **创建 issue**：`gh issue create --title "..." --body "..."`。多行正文用 heredoc。
- **读取 issue**：`gh issue view <number> --json number,title,body,labels,comments`。
- **列出 issue**：`gh issue list --state open --json number,title,body,labels,comments --jq '[.[] | {number, title, body, labels: [.labels[].name], comments: [.comments[].body]}]'`，配合适当的 `--label` 与 `--state` 过滤。
- **将 issue 设为父 issue 的子 issue**：`gh issue create --parent <parent> ...`，或事后 `gh issue edit <parent> --add-sub-issue <child>`（`gh` 2.94+）。旧版 `gh`：`gh api --method POST repos/<owner>/<repo>/issues/<parent>/sub_issues -F sub_issue_id=<child-db-id>`（数据库 id，见下方 **Blocking**）。无法使用子 issue 时，在子 issue 正文顶部写 `Part of #<parent>`。
- **Blocking（阻塞关系）**：GitHub **原生 issue 依赖**是最权威、UI 可见的表达方式。添加一条边：`gh issue create --blocked-by <n>,<n> ...` 或 `gh issue edit <child> --add-blocked-by <blocker>`（`gh` 2.94+）。旧版 `gh`：`gh api --method POST repos/<owner>/<repo>/issues/<child>/dependencies/blocked_by -F issue_id=<blocker-db-id>`，其中 `<blocker-db-id>` 是阻塞方的数字 **数据库 id**（`gh api repos/<owner>/<repo>/issues/<n> --jq .id`，_不是_ `#number` 也不是 `node_id`）。依赖不可用时，回退为在子 issue 正文顶部写 `Blocked by: #<n>, #<n>`。所有阻塞方都关闭后，工单才算解除阻塞。
- **统计未关闭的阻塞方**：`gh api repos/<owner>/<repo>/issues/<n> --jq .issue_dependencies_summary.blocked_by`（仅统计未关闭的阻塞方；0 表示可开工）。`gh issue view <n> --json blockedBy` 会连已关闭的阻塞方一起列出，需过滤：`--jq '[.blockedBy.nodes[] | select(.state == "OPEN")] | length'`。
- **评论**：`gh issue comment <number> --body "..."`
- **添加 / 移除标签**：`gh issue edit <number> --add-label "..."` / `--remove-label "..."`
- **关闭**：先用 `gh issue comment <number> --body "..."` 说明原因，再 `gh issue close <number>`。对已关闭的 issue，`gh issue close --comment` 会丢弃评论但仍以退出码 0 结束。

仓库从 `git remote -v` 推断；在克隆目录内运行时 `gh` 会自动完成。

只向用户自己的仓库提 issue。绝不向 `mattpocock/skills`（这些技能的来源仓库）提 issue，除非用户拥有它。在 fork 中，`gh` 可能默认指向上游父仓库，因此会话内首次 `gh issue create` 之前，先用 `gh repo view --json nameWithOwner` 检查目标；若不是用户自己的仓库，先与用户确认。

## Pull request 作为分诊面

**PRs as a request surface: no.** _（若本仓库将外部 PR 视为功能请求，改为 `yes`；`/triage` 会读取此标志。）_

设为 `yes` 时，PR 与 issue 使用相同的标签与状态流转，用 `gh pr` 等价命令：

- **读取 PR**：`gh pr view <number>`，再 `gh pr view <number> --comments`（非 TTY 下 `--comments` 只输出评论、不输出正文），diff 用 `gh pr diff <number>`。
- **列出待分诊的外部 PR**：`gh api --paginate 'repos/{owner}/{repo}/pulls?state=open' --jq '.[] | select(.author_association | IN("OWNER","MEMBER","COLLABORATOR") | not) | {number, title, author: .user.login, author_association, labels: [.labels[].name]}'`。
- **评论 / 打标签 / 关闭**：`gh pr comment`、`gh pr edit --add-label`/`--remove-label`、`gh pr close`。

GitHub 的 issue 与 PR 共用同一编号空间，裸 `#42` 可能是两者之一：先用 `gh pr view 42` 解析，失败再退回 `gh issue view 42`。

## 当技能说“发布到 issue 跟踪器”

创建一个 GitHub issue。

## 当技能说“获取相关工单”

按上方 **读取 issue** 的方式读取。

## Wayfinding 操作

供 `/wayfinder` 使用。**地图（map）**是单个 issue，工单是它的**子** issue。

- **地图**：一个标签为 `wayfinder:map` 的 issue，正文承载 Notes / Decisions-so-far / Fog。`gh issue create --label wayfinder:map`。
- **子工单**：作为地图的 GitHub 子 issue 关联（见 **将 issue 设为父 issue 的子 issue**）。子 issue 不可用时，把子项加入地图正文的任务清单，并在子 issue 正文顶部写 `Part of #<map>`。标签：`wayfinder:<type>`（`research`/`prototype`/`grilling`/`task`）。一旦被认领，工单即指派给驱动的开发者。
- **Blocking**：见 **约定** 下的 **Blocking（阻塞关系）**。
- **边界查询（Frontier query）**：列出地图的未关闭子项（`gh issue list --state open`，限定到地图的子 issue / 任务清单），剔除存在未关闭阻塞方（见 **统计未关闭的阻塞方**，或 `Blocked by` 行中有未关闭 issue）或已有指派人的项；按地图顺序取第一个。
- **认领**：`gh issue edit <n> --add-assignee @me`，这是会话的第一个写操作。
- **解决**：`gh issue comment <n> --body "<answer>"`，然后 `gh issue close <n>`，最后把上下文指针（gist + 链接）追加到地图的 Decisions-so-far。

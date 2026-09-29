# gfsim tile 代数错位：根因定性与修复要求

> 对象：`kernel_fa_fa_gmma_kchains_*` PV K-chain 上观测到的「编译器代数 vs gfsim 代数」错位
> 结论依据：PTO-SPEC normative ASL（`/tmp/pto-spec-current`，`main@01445483`）
> 日期：2026-09-22

---

## 一、结论（TL;DR）

1. **gfsim 当前的 SKIP 行为是规范违规。** 它让「值驻留在 internal accumulator」改变了架构上的**出版（publication）**与**源生命周期**，而 ASL 明文禁止这一点。
2. **编译器侧 +1 是正确的**，且应当是**无条件**的：每次成功的 CUBE D 写都必须 allocate + publish 一代，与 CCTRL 无关。
3. **提案「gfsim 只把 tile 指针 +1、不分配 tile reg」方向正确，但表述必须修正**——它建立在「这个指针 = tile reg 源选择器列表」这个前提上，而 ASL 的定义不是这样。`#N` 索引的是**已出版的代（published generation）**，不是物理 tile 寄存器。所以不存在"internal acc 不是 tile reg 所以不该进列表"的固有歧义。
4. **落地时必须区分两层**（这是唯一的真实风险来源）：
   - **L1 架构层**：每个 hand 的**已出版代**列表 → `#N` 只索引这一层，每次成功出版都 append；
   - **L2 实现层**：物理 tile 寄存器池占用（T#0..T#15）、ACC 缓存驻留/替换 → 架构不可观察，只影响性能。
   - 提案 = 「append 到 L1 + residency 标记为透明缓存 + 不占 L2 池槽」。若实现成「只 bump 计数器不建有效条目」或「让消费者用 L2 解析 `#N`」，就会出问题。
5. **先确认一件事再动手**：编译器那个 +1 是**每次 D 写无条件发生**，还是只在「容量不足回退到 tile reg」的分支里发生？**如果是后者，要修的是编译器而不是 gfsim**（见第四节）。

---

## 二、规范依据（三条原文）

### 2.1 `#N` 索引的是"出版代"，不是物理寄存器

文件：`asl/block/operands/B.IOT.asl`

> Bind ordered relative Local Tile sources and renamed destinations; each T/U/M/N #1 source names the **newest published generation** of that hand.
>
> T#1, U#1, M#1, and N#1 name the **newest published generation** in their hand; increasing indices select progressively older **live generations**. Direct model TileIndex values are **resolved physical identities** and are **not** encoded relative selectors.
>
> Code zero selects the T destination hand; **successful publication pushes the new generation to T#1**.

要点：
- 推动选择器的是 **publication**，不是"占用了一个物理寄存器"；
- 「物理身份（resolved physical identities）」被 ASL 明确划给**直接 TileIndex 值**，与 `#N` 相对选择器**分属两套东西**。

### 2.2 InternalAcc 是透明缓存，D 必须出版

文件：`asl/tile/model/execution/internal-accumulator.asl`

> InternalAcc MUST be a **transparent implementation cache**. Explicit TileReg C MUST remain the architectural accumulator input and explicit TileReg D MUST be **allocated and published on every successful operation**. CCTRL MAY provide non-binding input-prefetch and output-replacement hints, but cache hit, miss, **capacity, residency, replacement**, and timing **MUST NOT change architectural results, faults, allocation, publication, source lifetime, or ordering**.

要点：**residency（驻留在哪）不得改变 allocation / publication / source lifetime / ordering** —— gfsim 的 SKIP 正好违反这条。

### 2.3 raw acc 也必须出版；CCTRL 只是非绑定提示

文件：`asl/tile/matrix-and-matrix-vector/matrix-matrix/TMATMUL_ACC.asl`

> CCTRL[0]=1 MUST **publish** raw accumulator-type D and MAY provide a **non-binding** transparent-cache replacement hint; **D allocation and publication remain mandatory**.
>
> CCTRL[1] MAY hint transparent-cache use or prefetch of explicit C; **explicit C remains the architectural accumulator input**.
>
> **Cache behavior MUST NOT alter results, faults, source lifetime, or ordering.**

要点：`RawAccumulator` / `InternalAccHint` 都不免除 D 的 allocate + publish 义务。

### 2.4 三条合起来

```text
每次成功的 CUBE D 写 → 必须 allocate + publish 一代
CCTRL[0] / CCTRL[1]  → 只是非绑定提示，不改变出版、分配、源生命周期、顺序
值驻留在哪（tile reg / ACC cache） → 架构不可观察
```

---

## 三、澄清："会不会引入歧义"这个担心

### 3.1 前提需要修正

提问里的前提是：

> 这个指针对应的是【tile reg】源选择器列表，【internal acc】实际上不是【tile reg】

按 ASL，这个前提要拆成两句话看：

- ASL 确实把 D 写成 `TileReg`，但那指**架构上的 tile 标识**（被命名的目的地），**不是**物理的 T#0..T#15 池；
- `#N` 索引 **published generation**，与「payload 驻留在哪」**正交**。

所以：**不会有固有歧义。** 会出问题的是把两层混在一起。

### 3.2 正确的两层模型

| 层 | 内容 | 可见性 |
|---|---|---|
| **L1 架构层** | 每个 hand 的**已出版代**列表（每条含描述符/形状） | `#N` **只**索引这一层；每次成功出版都 append，与 CCTRL 无关 |
| **L2 实现层** | 物理 tile 寄存器池占用（T#0..T#15）、ACC 缓存驻留/替换 | **架构不可观察**；只影响性能，不得影响 L1 |

提案的正确等价形式：

```text
append 到 L1（vld=true，带形状）
+ residency 标记为 TransparentAcc
+ 不消耗 L2 的 T#0..T#15 池槽
```

---

## 四、动手前必须先确认的一件事（决定谁改）

提问里提到：

> 编译器侧现在即使是写入 internal acc 的指令，**因为容量不足存在 fall back 到 tile reg 的逻辑**，会把指针 +1

需要区分两种情况：

### 情况 A：+1 是无条件的（每次 D 写都出版）

→ 编译器正确，**符合 ASL**。此时修 gfsim（即本提案）。

### 情况 B：+1 只在"容量不足回退到 tile reg"的分支里发生

→ **编译器违反 ASL**：`#N` 编号会随缓存容量/寄存器压力变化，而 ASL 明文要求

> capacity, residency, replacement ... MUST NOT change ... publication ... ordering

此时**该修的是编译器**（把"出版"与"容量回退"解耦：出版无条件，回退只决定 payload 落在哪）；让 gfsim 去模仿一个容量依赖的编号，等于把编译器 bug 固化进模型，并且会在寄存器压力不同的编译下继续错位。

**判断方法**：同一源程序分别用「寄存器充足 / 不足」两种条件编译，比较同一处 `#N` 的编号。若编号随压力变化 → 情况 B。

---

## 五、gfsim 侧实现要求

### 5.1 把 `mapQEntry` 明确定义为「per-hand published-generation ring」

每条记录：

```text
{
  descriptor(shape, dtype, size_code),          // 出版时确定，供消费者做几何/尺寸校验
  payload_residency ∈ {TilePool(k), TransparentAcc},
  vld, generation
}
```

### 5.2 解析规则

- `#N` **只**按 ring 序解析（`(lastAllocPtr - N) mod ring`），**不查 residency**；
- 每次成功 CUBE D 写（**含 raw acc / InternalAccHint**）都 append 一条 `vld=true`；
- residency = TransparentAcc 时**不消耗** T#0..T#15 池槽；
- 取 payload 时才看 residency；TransparentAcc 从**条目**取权威 payload（ACC 只当缓存，或直接忽略 hint，与 portable model 一致）。

### 5.3 关键禁忌

- ❌ 只 bump 计数器、不建有效条目（会让 `#N` 撞未分配格）；
- ❌ 任何消费者路径用 L2（池占用/ACC 驻留）解析 `#N`；
- ❌ 让 L2 的占用反过来影响 L1 的编号。

---

## 六、风险清单（按严重度）

| # | 风险 | 说明 | 处理 |
|---|---|---|---|
| R1 | **只 +1 不建条目** | `#N` 解析到未分配格 → 就是那个 `mapQEntry[H][255].vld == false → BIssue.cpp:3422` 断言；且 K-chain 自己的 mid 要读 `n#1` 作 ACC 源，条目无效直接断链 | 必须出版**有效条目**（含描述符/形状） |
| R2 | **编译器的 +1 是否无条件** | 若只在回退分支发生 → 编号依赖容量，违反 ASL transparency | 见第四节；情况 B 则改编译器 |
| R3 | **形状/尺寸必须记在条目上** | 附件里 16KB vs 4KB 的"错版本"就是形状没跟着代走 | 消费者读**条目记录的**形状，不读"当前绑定的寄存器" |
| R4 | **ACC 驻留的别名/活跃性** | 多个 hand 同时 ACC 驻留是否互相覆盖 | 按 ASL 当**透明缓存**：权威 payload 放出版条目，hint 可忽略 |
| R5 | **fault/rollback 原子性** | 指针推进若在提交组外，失败会留幽灵代 | 推进落在**提交组内**（对齐 spec 的 CUBE rollback 条款） |
| R6 | **source lifetime** | ASL 明确列为不得被 residency 改变的量；SKIP 会让源被提前回收 | 出版即修复（这正是当前观测到的现象） |
| R7 | **不得引入新的误拒** | ACC 驻留的值被普通 tile 源消费时，模型必须能服务 | 透明缓存语义；不能新增"ACC 驻留不可作 tile 源"的失败路径 |

---

## 七、验证方案

1. **强不变式（最划算，建议先加）**
   对每个 hand：`gfsim 已出版代数 == .diss 里该 hand 的 D 写次数`。
   把附件表格里的"编译器第几代"当 oracle，加一条自检断言即可一次性抓出这类错位。

2. **逐条 `#N` 比对**
   按附件表格形式，比对 (generation, shape) 两侧解析结果，确认 DIFF 归零。

3. **压力依赖性测试**
   同一源程序在「寄存器充足 / 不足」两种编译下比较 `#N` 编号；若不同 → 情况 B，先修编译器。

4. **rollback 用例**
   注入 fault，确认没有多出幽灵代。

5. **无新增误拒**
   跑原本全绿的套件。

---

## 八、一句话回复版

> ASL 里 `#N` 索引的是「已出版的代（published generation）」而不是物理 tile 寄存器——`asl/block/operands/B.IOT.asl` 明确写 "successful publication pushes the new generation to T#1"，且物理身份 "are not encoded relative selectors"。同时 `internal-accumulator.asl` 规定 InternalAcc 只是透明缓存、D 必须 allocate+published，且 "capacity, residency, replacement ... MUST NOT change ... allocation, publication, source lifetime, or ordering"；`TMATMUL_ACC.asl` 也写明 CCTRL[0]=1 时 "D allocation and publication remain mandatory"、CCTRL[1] 只是非绑定提示。
>
> 所以 gfsim 现在因为"只写 internal acc"就不出版，是规范违规；编译器 +1 是对的。提案方向正确，但两点必须注意：(1) 不能只把指针 +1，必须**出版一个带形状/描述符的有效条目**，否则 `#N` 会撞到未分配格（就是那个 `mapQEntry[...].vld == false` 断言）；(2) 要区分两层——`#N` 只索引"出版代列表"，物理寄存器池/ACC 驻留是架构不可观察的实现层，不要把池占用混进选择器解析。
>
> 另外请先确认：编译器那个 +1 是**每次 D 写无条件发生**，还是只在"容量不足回退到 tile reg"的分支里发生？如果是后者，编号就依赖缓存容量，违反 ASL 的 transparency 条款，那要修的是编译器而不是 gfsim。

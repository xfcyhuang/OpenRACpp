# PROTOTYPE（wayfinder 工单 #23）—— 一次性原型，证明 x64 汇编与 oracle 黄金对拍三通路，勿并入主分支。
#
# 上游语义来源：OpenRA.Game/Sync.cs @ 基线 1f594e320
#   HashInt2 / HashCVec : ((i2.X * 5) ^ (i2.Y * 3)) / 4
#     —— C# int32 语义：乘法回绕（本就与 imul 低位一致）、除法向零截断（与 sar 的向下取整在负数上分叉！）
#   HashPlayer          : (int)(p.PlayerActor.ActorID << 16) * 0x567
#     —— ActorID 为 uint，左移 16 后按位转 int，再 int32 回绕乘。
#
# 工具链铁律（research/llvm-mingw-asm-toolchain 报告，均为崩溃实证）：
#   * 本文件全部为叶子函数（无 prologue、无 call）→ 无需 .seh_* unwind 元数据；
#     一旦加 call 或建立栈帧，必须补 .seh_* 且 call 前 sub rsp,0x28。
#   * Win64 ABI 陷阱：传 32 位参数时 RCX/RDX 的高 32 位未定义 → 全程只用 32 位操作
#     （32 位指令写目标寄存器自动零扩展到 64 位）。

    .intel_syntax noprefix
    .text

    .globl ora_proto_hash_int2
ora_proto_hash_int2:                       # int ora_proto_hash_int2(int x /*ecx*/, int y /*edx*/)
    imul    eax, ecx, 5                    # x*5，32 位回绕
    imul    ecx, edx, 3                    # y*3
    xor     eax, ecx
    # C# '/' 向零截断，sar 向下取整：负数差 1。调整后再移位即可逐位一致。
    cdq                                     # edx = eax 符号扩展（0x00000000 / 0xFFFFFFFF）
    and     edx, 3                          # 负数 → 3，非负 → 0
    add     eax, edx
    sar     eax, 2
    ret

    .globl ora_proto_hash_int2_naive_sar
ora_proto_hash_int2_naive_sar:             # 反面教材：天真 sar。负数输入与 C# 语义分叉，供测试断言陷阱确实存在。
    imul    eax, ecx, 5
    imul    ecx, edx, 3
    xor     eax, ecx
    sar     eax, 2                          # = floor(v/4)，负数时 ≠ C# 的 trunc(v/4)
    ret

    .globl ora_proto_hash_player
ora_proto_hash_player:                     # int ora_proto_hash_player(unsigned int actor_id /*ecx*/)
    mov     eax, ecx
    shl     eax, 16                         # uint<<16，位级即 (int)(id<<16)
    imul    eax, eax, 0x567                 # int32 回绕乘
    ret

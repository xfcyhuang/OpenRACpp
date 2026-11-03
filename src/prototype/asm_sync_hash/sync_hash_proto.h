// PROTOTYPE（wayfinder 工单 #23）—— 一次性原型，勿并入主分支。
// 上游来源：OpenRA.Game/Sync.cs @ 1f594e320 的 HashInt2/HashCVec/HashPlayer。
// 汇编实现经 C ABI 暴露（extern "C"，工具链核查结论：C++ 侧引用汇编符号必须 extern "C"）。
#pragma once

extern "C" {

// ((x*5) ^ (y*3)) / 4，C# int32 语义（乘法回绕、除法向零截断）
int ora_proto_hash_int2(int x, int y);

// 反面教材：天真 sar 实现（负数输入与 C# 分叉），仅用于测试断言陷阱存在
int ora_proto_hash_int2_naive_sar(int x, int y);

// (int)(actor_id << 16) * 0x567，uint 左移 + int32 回绕乘
int ora_proto_hash_player(unsigned int actor_id);

}  // extern "C"

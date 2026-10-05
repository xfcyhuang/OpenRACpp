# OpenRACpp

**中文** | [English](#english)

## 简介

[OpenRA](https://github.com/OpenRA/OpenRA) 游戏引擎的 **C++26 语义重写**。

按 OpenRA 上游源码（基线 commit `7d57605bca`）的代码语义完全重写，数据、地图与回放格式兼容上游。

## 状态

**Phase 3 完成**(仿真核心 + Order/锁步;2026-10-03):

- `src/sim/` 10 文件:TraitDictionary(平行数组二分,载荷 = gen 导出的 TypeId 上行转换表)、Actor(条件系统/Initialize 观察者链/Dispose 帧末幂等)、World(Tick 序/SyncHash n 连续公式)、Activity 状态机逐行、Sync 哈希协议(含 bool 字段 IL 可达语义)、Target/Player/Effects/TypeDictionary/ActorInitializer
- `src/net/` 8 文件:Order 逐字节序列化(构造期望字节逐位断言 + 往返恒等)、OrderPacket/OrderIO、EchoConnection、OrderManager 锁步全文(TryTick 三段/IsNetFrame 节流/帧号校验)、UnitOrders 可运行子集
- 验收:EchoConnection 单机 **10⁶ tick 双构建(ASan+UBSan/Release)通过,无泄漏无 desync**;全套 ctest 11/11 双构建;UPSTREAM 标注 85 条校验通过;偏离 D25~D34 登记于 docs/COVERAGE.md

**Phase 2 完成**(元数据框架 + 数据加载链;2026-10-03):

- `tools/schema_dumper --gen` 权威导出:680 类型(601 可加载)+ 37 枚举 + 9 BitSet 标签 + TypeId 键空间(6048 类型)+ [VerifySync] 成员表(84 类型);`src/meta/` GenericValue/GeneratedRecord 值袋 + 双向 typed 内存变换
- 加载链全通:Manifest → ModData → Ruleset → ActorInfo(`@` 实例名/接口依赖拓扑序)→ WeaponInfo;24 个 LoadUsing loader 逐语义移植
- 验收:三 mod 全量深度解析 dump **逐字节快照回归**(ra 80,274 / cnc 49,416 / d2k 35,833 行);上游 `--check-yaml` exit=0 佐证;偏离 D10~D24 登记

**Phase 1 完成**(MiniYaml + 文件系统;2026-10-03):

- `src/yaml/`:上游 MiniYaml.cs(791 行)逐语义重写——行状态机(4 空格/1 tab 层级、`\#` 转义、`\ ` 空白守护)、字符串池 interning、`Merge`/继承解析/`-Key` 弱删除、规范化序列化、可变 Builder;`src/core/text.hpp` 复刻 .NET `char.IsWhiteSpace`/`Trim` 的 UTF-8 语义
- `src/fs/`:FileSystem(挂载顺序 = 覆盖优先级、`'|'` 显式挂载、大小写规整解析)、Folder(读写)、ZipFile(只读,SharpZipLib→**miniz**,含子目录视图与嵌套 zip)、`MiniYaml::Load`
- **第一关卡验收:上游 mods/ 全部 759 个 yaml × 双模式(丢弃/保留注释),`FromStream→WriteToString` 输出与 C# oracle 逐字节 100% 一致**(13.5MB 黄金数据,`tests/golden_yaml.txt`);上游 MiniYamlTest.cs 28 用例断言文本逐字移植全绿;fs_test 24 断言全绿
- 工程门禁:`import std;` 严控(禁传统 std 头引入,`tools/std_import_check.py` 强制)、函数级裁剪(`-ffunction-sections -fdata-sections` + `--gc-sections`)、UPSTREAM 溯源标注(`tools/upstream_check.py`)、全代码中英双语注释
- 双构建(ASan+UBSan 与 Release)ctest 全绿;偏离 9 项登记于 docs/COVERAGE.md

**Phase 0 完成**(C++26 工程骨架 + 定点原语):

- 工程基线:clang `-std=c++26` + `import std;`(std.cppm 预编译 PCM)+ CMake/Ninja + ctest;确定性选项 `-fwrapv -fno-strict-aliasing`
- `src/core/` 12 个头文件:定点原语(WPos/WVec/WAngle/WRot/WDist/CPos/CVec/MPos/int2/Rectangle/Int32Matrix4x4/ISqrt)与 MersenneTwister,全部按上游源码逐语义重写,`constexpr` 全面化,文件头带 `// UPSTREAM:` 溯源标注
- 验收:与 C# 版黄金数据 **60,883 行逐行对拍 100% 一致**(覆盖全角度三角学、ArcSin/ArcCos 全量、MT19937 全序列、LerpQuadratic 的 decimal 截断语义等),ASan+UBSan 与 Release 双构建全绿
- 上游同步机制就位:`UPSTREAM.baseline`(基线 commit `7d57605bca`)+ `docs/COVERAGE.md` 覆盖登记 + `tools/upstream_check.py` 校验器

**core 基础设施增补**(2026-10-04,随上游审阅落地):

- `src/core/percent_modifiers.hpp`:上游 `ApplyPercentageModifiers` 的 C# decimal(128 位软十进制,22 个 sim 调用文件)以 `__int128` 精确复刻——分子累积 × 分母 `100^k`,整除 = `(int)decimal` 向零截断
- `src/core/arena.hpp`:PORTING_PLAN §4.5 内存分区落地——FrameArena(帧临时,仅平凡可析构)/ WorldArena(整局,析构逆序登记、Destroy 幂等、Reset 复用)
- `tests/core_test.cpp`:decimal 语义断言(含早截断分歧底线用例)+ arena 生命周期断言;双构建 ctest 12/12

**Phase 4 第一批完成**(平台层骨架 + 渲染命令缓冲;2026-10-04):

- `third_party/SDL2`:官方 2.32.10 MinGW x64 开发包接入
- `src/platform/`:gl_types(103 个 GL 常量逐值对照)+ gl_loader(78 入口表驱动直连,KHR_debug 回调替代 glGetError 轮询)+ sdl2_window(窗口三模式 + **几何打包 atomic 快照**,getter 无锁)
- `src/gfx/`:gfx_command(**值类型命令 + 内联载荷 + SPSC 无锁字节环**,替代上游装箱消息队列)+ render_thread(**统一线程模型**:渲染线程永远存在并独占 GL 上下文,消费端绑定状态 diff)
- 验收(platform_test):SPSC 双线程 **200,000 条**序号完整性压测 + 桌面 GL 集成 —— **NPOT FBO**(333×257)直接 FRAMEBUFFER_COMPLETE、清屏与着色器三角形的 glReadPixels 像素断言;无桌面环境自动 SKIP;双构建 ctest 13/13;偏离 D35~D40 登记

**Phase 4 第三批完成**(Sheet/SheetBuilder/Sprite + Palette 家族 + HardwarePalette;2026-10-05):

- `src/gfx/` 六件:sprite(四枚举 + Sprite 预计算 1/128 inset 归一化坐标 + SpriteWithSecondaryData)、vertex(48B 逐字段 + combined 属性契约)、gfx_util(**FastCreateQuad 位域打包逐位 = combined.vert 注释契约**、FastCopyIntoChannel 全路径、uint32 快速整数预乘、旋转/包围盒/NextPowerOf2 + Vector2/3 渲染运算)、sheet(**shelf 打包 + Indexed 四通道轮换 + dirty 全量/子区域自动切换** + 缓冲转移复用)、palette(Immutable/Mutable/Remap/PaletteReference,字节流构造的 <<2|>>6 语义)、hardware_palette(**OPT-A7:调色板 dirty 行** —— 单行变化逐行 SetSubData 增量,过半退全量;OPT-C5 逐字节读回断言)
- 验收(gfx_test):纯逻辑(shelf 几何/通道轮换/dirtyRegion 并集/palette 字节流边界/预乘边界/位域全位/拷贝全路径)+ GL 集成(Sheet 全量与子区域上传读回、缓冲转移 GL 路径、调色板**增量 == 全量参考逐字节**);双构建 ctest 14/14;修出两个真 bug(palette 扩容清零丢已写行、GetData 未绑定自身读错纹理);偏离 D46~D50 登记

**Phase 4 第二批完成**(输入层 + GL 资源封装;2026-10-05):

- `src/platform/`:keycode.hpp(238 条枚举逐值照搬,SDL 头对照断言)+ sdl2_input(事件泵逐事件:修饰符采样、**motion 合并**、X1X2 转伪键盘、滚轮/UTF-8 文本、退出上报)+ MultiTapDetection/TapHistory(三槽 250ms/位移 4 多击检测,时钟注入可测);sdl2_window 增焦点/挂起原子状态
- `src/gfx/`:shader(**{VERSION} 替换 + 链接后 active-uniform 枚举入整数表(OPT-A6:无字符串热路径)+ sampler 单元分配**,Set*/Location 整型化 API)+ texture(**BGRA 上传/UNPACK 行打包/RGBA16F/读回/ScaleFilter,RAII 异步删除**);gfx_command 增 13 命令
- 验收(platform_test):纯逻辑(多击时序/距离边界、修饰符位组合、坐标舍入边界、Keycode×SDL 对照)+ 合成事件泵(双击 1→2、三条 motion 合一、滚轮/文本/退出)+ GL 集成(**NPOT BGRA 逐字节往返、子区域行距位图、白纹理 × 红 uniform 采样链像素断言、RAII 后管线照常**);双构建 ctest 13/13;修出第一批三个真 bug(SPSC 失配对齐/上下文泄漏/payload 越界);偏离 D41~D45 登记

**Phase 4 第四批完成**(SpriteRenderer + 单级合成 Renderer;2026-10-05):

- `src/gfx/` 五件:vertex_buffer(**OPT-A5:glBufferStorage 持久映射三槽 VB** + 槽级 fence(渲染线程内闭环,主线程零等待)+ 槽基址经 glDrawElementsBaseVertex;**OPT-A6:per-(顶点格式, program) VAO 缓存**,VB/IB/属性一次固化)、frame_buffer(NPOT 直建,Bind 的 viewport 保存/恢复与清屏)、sprite_renderer(8 纹理槽映射含双 sheet 与满槽 flush 重试逐行、BlendSpan 交错段、DrawSprite 全形态、RgbaSprite/ColorRenderer 家族;BlendSpanTracker/IRgbaQuadSink 提出为纯逻辑可测件)、renderer(**OPT-B1 单级合成**:NPOT world FBO + screen FBO 整级删除,BeginUI 把 worldSprite 直接画进默认帧缓冲 —— 每帧 3 次全屏 clear + 2 次全屏拷贝 → 1 clear + 1 blit;NDC 净映射等价论证在文件头)、palette(**OPT-A7:色移 epoch** —— PaletteReference 每精灵的字符串字典查找归零)
- `glsl/combined.vert|frag` 自上游原样复制;gfx_command +15 命令(blend 状态机 diff/深度/FBO depth/持久 VB/VAO/Present);渲染线程启动即建常驻全局 VAO(上游同语义)
- 验收(gfx_test):纯逻辑(BlendSpan 段合并/ResolveTextureIndex 三态与 epoch 失效链/worldSprite 几何的整数倍 renderScale 舍入/ColorRenderer 几何捕获)+ GL 集成(**调色板采样链 × 五轮 Flush 覆盖三槽回绕与 fence 等待**、三段 blend 交错单次 Flush、双 shader VAO 往返、NPOT FBO 130×70、**单级合成全流程**:BeginWorld → world 精灵 → BeginUI 2× 合成 → UI 精灵 → 默认帧缓冲三区像素断言);双构建 ctest 14/14;修出四个真 bug(fence 常量误值致 INVALID_ENUM、纹理绑定缓存 8 单元把 Palette/ColorShifts 挡在门外、GL 名字复用撞绑定 diff 缓存、持久 VB 槽写入与索引寻址错位);偏离 D51~D57 登记

**Phase 4 第五批完成**(Westwood 文件格式第一编:编解码器 + SHP/TMP 图像链;2026-10-05):

- `src/formats/` 十二件:fast_byte_reader(逐语义 + 越界等价抛点)、span_reader(UPSTREAM: NONE 自有 Stream 形态适配)、lcw(Format80 五 case 解码 + "quick and dirty v2" Encode)、xor_delta(Format40 六 case)、rle_zeros(Format2)、lzo(minilzo 2.06 C# 移植逐控制流照抄,goto/标签保真,未对齐读写按小端位拼)、crc32(256 表逐值 + 链式 Update/Finish)、shp_td(头表/引用解压链/TrimmedFrame 收边/无限递归防线)、shp_d2(帧偏移 2/4 字节两型 + 查表)、shp_ts(三扫描线格式 + 伪帧判定循环)、tmp_td/tmp_ra(单字节索引 tile 集)、tmp_ts(菱形展开 + 悬崖 extra + 深度第二帧组);`src/gfx/sprite_frame.hpp`(ISpriteFrame 契约)+ SheetBuilder::Add(ISpriteFrame) 接线(D47 遗留)
- oracle:`tools/golden_gen -- fmt`(上游 DLL extern-alias 引用,按各 mod mod.yaml 的 SpriteFormats 链序)生成 `tests/golden_formats.txt`:**mods 全部 213 个 .shp(186 ShpTD/27 ShpTS)逐帧 Type/Size/FrameSize/Offset(.NET float 格式化)/Data-CRC32 + 前 32 字节 hex、6 个 .pal 的 ImmutablePalette 全 256 项、合成 tmpTD/RA/TS 夹具(两语言同构造算法)、LZO 四向量(含 M2 大匹配与 MatchNext 双重读路径)、LCW 编码向量** —— 6096 行逐行对拍一致,oracle 双跑确定性验证
- 验收(formats_test):纯逻辑(FastByteReader/RLE0 两分支/XOR 六 case/LCW 五 case + Encode→Decode 往返 + 越界抛点/CRC32 标准向量 + 分段链式、合成 shpTD 的 TrimmedFrame 裁剪与全零帧、判定负例)+ 黄金对拍 + SheetBuilder 帧接线(Indexed sheet Red 通道字节 2);双构建 ctest 15/15;修出两个真 bug(CRC32 表 idx194 一位抄错 —— 标准向量只查 9 项表未触及,zlib 交叉验证定位;RLE0 字面量分支缺界检查 —— ASan 实证);偏离 D58~D62 登记

**Phase 4 第六批完成**(Westwood 文件格式第二编:Png 自研 + Tga/Dds 的 Pfim 移植 + ShpRemastered + EmbeddedSpritePalette;2026-10-05):

- `src/formats/` 五件:png(**OPT-B3:上游自研 592 行逐语义照抄**——块循环/IHDR 校验/PLTE/tRNS 部分 alpha/tEXt ASCIIZ + 重复键更新保位/IDAT 链缝合/zlib 解压 + 五滤波反解 + 位深 1/2/4 解包/原始像素构造的 BGR↔RGB 大端交换/Save 块序 + CRC32 链式写 + 行滤波 0;zlib 走 miniz,D63/D64)、targa(**Pfim v0.11.3 逐语义移植**——18 字节头/四向原点/非压缩 + RLE(含 TopLeft-RLE 行紧排怪癖照抄)/4 字节对齐 stride/色图应用含上游 newLen 怪癖)、dds(Pfim 子集——非压 8/16/24/32bpp + 位掩码 R/B 交换 + Rgba16 半字节交换 + mip 链、DXT1/3/5 块解码(RGB565 float 插值逐字 + (byte)(x+0.5f) 截断)、mip 尺寸 double 截断;BC4/5/DX10 系显式拒绝 = D66)、shp_remastered(zip 容器:惰性前缀正则最左匹配手写复刻、缺号空白帧、meta JSON 手写全串匹配、前缀不一致逐字抛)、embedded_sprite_palette(帧级/文件级协商)
- oracle:`tools/golden_gen -- fmt` 增四段合成夹具(两语言同构造;**stored-deflate zlib 与手工 zip 保证两语言字节恒等**):7 PNG(五滤波/位深 1/2/4/8/PLTE+tRNS 部分/双 IDAT 切分/重复 tEXt/未知块 + **Save 结构对拍 = 块类型序 CRC + IDAT 解压 CRC**,D64)+ 5 TGA(三向 × 24/32bpp + RLE 两型)+ 7 DDS(非压含掩码交换/三 mip/DXT1 插值+透穿/DXT3/DXT5 双梯度)+ 1 ShpRemastered 合成 zip(meta 裁剪帧/缺号空白帧/无 meta 帧/无关条目)—— golden 6096 → **6183 行逐行对拍一致**,oracle 双跑确定性验证
- 验收(formats_test):纯逻辑新增(PNG 九负例 + Save 往返/块序/CRC 逐块/滤波 0 行流、Tga/DDS/ShpRemastered/EmbeddedSpritePalette 负例与四态);双构建 ctest 15/15;偏离 D63~D67 登记

**Phase 4 第七批完成**(mix 包 + Blowfish;2026-10-05):

- `src/formats/` 三件:blowfish(P/S 盒 18+4×256 逐值照搬 + 16 轮 Feistel 逐行 + RunCipher 大端装卸与奇数尾落 0)、blowfish_key_provider("direct C port" 全文逐语义 —— DER 公钥解包/小端大数 16 位 limb/倒数与 Knuth-D 商估计(**全部中间量 64 位累积,对齐 C# int*uint 的 long 提升**)/平方乘/(55/a+1) 块分组)、xcc_database(local 的 48 字节头解析 + Data() 写出;global 的块解析)
- `src/fs/` 三件:package_entry(Classic rotate-1 与 CRC32 填充字节两哈希)、mix_file(三格式检测 + 加密头 80B 密钥块 RSA→Blowfish 整块解密 + local/global 库双哈希名解析择优;MixLoader 后缀嗅探)、d2k_sound_resources(.rs 目录解析,绝对偏移取内容)
- 接线:Manifest.PackageFormats 解析 + ModData 按名装载 Mix/D2kSoundResources loader → FileSystem 构造(ModData.cs L63-65 等价)
- oracle:golden_gen 增 Q/KB/BE/M/X/G 五段(加密夹具**构造侧走内嵌 Blowfish/KeyProvider 逐字副本、解析侧走上游 MixLoader.MixFile = 跨语言闭环**),黄金 6183 → **6227 行逐行对拍一致**,oracle 双跑确定性验证
- 验收(formats_test):Blowfish 公开标准向量/往返/奇数尾、Xcc 往返与截断负例、HashFilename 填充边界、mix 最小夹具 + 垃圾密钥块负例 + 嗅探、.rs 绝对偏移 + 重键负例;双构建 ctest 15/15;偏离 D68~D73 登记

**Phase 4 第八批完成**(aud/wav 声音格式;2026-10-05):

- `src/formats/` 四件:ima_adpcm(ImaAdpcmReader.cs 全文:IndexAdjust 8 项/StepTable 89 项逐值、双整除向零截断与双饱和)、westwood_compressed(五分支 = 2 位差分/4 位差分/字面/单样点跳变/填充,跳变 byte 回环非饱和照抄)、aud_reader(12 字节头 + 0xdeaf 块头;IMA 流 index/currentSample 跨块持久与奇 outputSize 半字节截断;WS 流的增长清零/不增长残留语义)、wav_reader(RIFF 块循环 + PCM 直通/IMA 交错/MS ADPCM 三解码路径,含 outputSize = uncompressedSize×channels×2 与无 fact 的 -1 首样点即截怪癖)
- oracle:mods 全部 46 个 .aud + 4 个 .wav 实资产 + 13 个合成夹具 + IMA/Westwood 解码向量,黄金 6227 → **6547 行逐行对拍一致**;修出一个真 bug(交错缓冲定长 32 致 mono 输出 ~2×);双构建 ctest 15/15;偏离 D74~D77 登记

**Phase 4 第九批完成**(vqa/wsa 视频格式;2026-10-05):

- `src/formats/` 五件:video(IVideo/IVideoLoader/GetVideo 链:属性 → 方法、byte[]/null → span)、vqa_video(VqaVideo.cs 全文逐语义:FINF 偏移表 0x40000000 标志、SND0/SND2 收集(SND2 走 IMA、立体声交错、奇长 +=2 怪癖)、DecodeVQFR 子块循环(CBFZ 反向模式 + HQ RGB555 展开、CBP0/CBPZ 分块码本"下一帧"应用、VPTZ/VPRZ/VPTR 三指令路径、VQFL 提前返回)、非 HQ 8 位/HQ 五 case 两条 DecodeFrameData)、wsa_video(调色板 <<2 + 高位复制、偏移 +768、LCW + XOR delta 作用于上一帧、行距重算;IsWsa 嗅探含 `width <= 0` 恒假的 ushort 怪癖)、SpanReader 增 Peek(EOF -1 语义)
- oracle:六个合成夹具两语言同构造(非 HQ 全路径/奇长立体声/CBP 双轮/HQ 三指令路径/WSA 双填充/嗅探负例),黄金 6547 → **6606 行逐行对拍一致**;修出一个真 bug(AdvanceFrame 漏置 has_previous 致第二帧对全零基线异或);双构建 ctest 15/15;偏离 D78~D79 登记

下一批次(Phase 4 续):vxl/hva/idx、OpenAL/FreeType、硬件光标、WorldRenderer/渲染收集(OPT-A7 SOA + 帧 arena)。

## 与上游的差异(优化点与偏离登记)

本项目以**语义等价**为第一原则(黄金对拍验证),在此基础上于 C++ 侧做有意优化;凡无法或有意不逐语义等价处,在 [docs/COVERAGE.md](docs/COVERAGE.md) 登记偏离(**未登记的偏离视为 bug**)。

### 有意优化(C++ 侧)

| 层面 | 上游 C# | C++ 侧 |
|---|---|---|
| 数据反射链 | `FieldLoader`/`ObjectCreator` 运行时反射 + `TypeDescriptor` 转换器兜底 | `gen/` 编译期描述表(680 可加载类型,由 schema_dumper 从 C# 反射权威导出)+ 注册表工厂,无运行时反射 |
| 类型/接口查询 | `Dictionary<Type,…>` 哈希 + `GetInterfaces()` 反射枚举 | TypeId 键空间(6048 类型全名 → `uint16`,gen 导出)+ 平行数组二分 |
| SyncHash | `Reflection.Emit` 运行时生成 IL 哈希 + `ConcurrentCache` 委托链 | `gen/sync_gen.cpp` 编译期生成(84 类型),零反射零委托间接 |
| 数值修正链 | `ApplyPercentageModifiers` 的 C# `decimal`(软十进制,比 int64 慢一个数量级) | `__int128` 精确复刻(整除 = `(int)decimal` 向零截断;等价域论证见 `src/core/percent_modifiers.hpp` 文件头) |
| 内存管理 | GC(每帧 LINQ/闭包/装箱分配) | 分区 arena:`FrameArena`(帧末重置)+ `WorldArena`(整局 bump + 析构登记);同步路径禁 `shared_ptr` |
| 集合参数 | `IEnumerable<int>`(LINQ 链 + 枚举器分配) | `std::span` 直传(首批落地点即数值修正链) |
| 枚举/异常 | 字符串 switch / 异常类型分散 | `enum class` + 集中 `YamlException`(消息文本仍逐字对齐) |

后续大项(渲染命令缓冲替代装箱消息队列、寻路世代标记免清零、条件系统 intern 化、空间索引 ActorID 数组化等,共 40+ 项)按 Phase 随移植同批落地;完整审阅证据(上游 file:line 锚点)见 [docs/UPSTREAM_CPP_REVIEW.md](docs/UPSTREAM_CPP_REVIEW.md)。

### 与上游不同步处(偏离登记摘要)

当前登记 **D1~D79**(全文见 [docs/COVERAGE.md](docs/COVERAGE.md)),按模块:

- **yaml/fs(D1~D9)**:异常类型统一为 YamlException(消息文本逐字一致)、惰性枚举物化为 vector、null/"" 键合流等——合法输入下行为等价或不可观测;
- **meta/加载链(D10~D24)**:TypeConverter 兜底未实现(实际字段类型已全覆盖,不可达)、字典字段为插入序 vector(dump 协议按键排序)、三 mod 解析快照以 C++ 侧固化(D24:C# `--dump` 工具受 ALC 程序集副本环境制约,恢复后可再对拍校准)等;
- **sim/net(D25~D34)**:Initialize 观察者去重形态差异(受端幂等)、**trait 工厂与所有权待 Phase 5 随 World arena 接线(D26/D27)**、`Target.FromCell` 以 square 网格公式桩换算(D28,Phase 5 接 Map)、UI/大厅命令族静默吞并(D29,Phase 6/7 接线)、SyncReport 未接(D30,上游默认关闭)等;
- **平台/渲染(第一批 D35~D40、第二批 D41~D45、第三批 D46~D50、第四批 D51~D57)**:命令缓冲替代装箱消息队列、统一线程模型、NPOT、KHR_debug、纹理生命周期契约(替代 glIsTexture 驱逐)、持久映射 VB/VAO 缓存/blend diff/单级合成等;输入层形态适配(退出上报/时钟注入/X1X2 IsRepeat);**格式族第一编(D58~D62)**:Stream → SpanReader、loader 链独立起读、Data null/空统一、越界异常 runtime_error 等价抛点、帧数据 shared_ptr 共享;**格式族第二编(D63~D67)**:zlib 层 miniz 形态(D63)、Save 的 deflate 字节依实现(D64)、Pfim 双路径统一(D65)、BC4/5/DX10 拒绝(D66)、正则手写复刻(D67);**mix 包族第三编(D68~D73)**:HashFilename 哈希域 ASCII(D68)、mix 字节驻留/顺序非契约/global 库不重试(D69)、未解析日志丢弃 + ReadBlocks 宽松界照抄(D70)、Xcc ASCII 域(D71)、.rs 重键等价抛(D72)、loader 名字分派表(D73);**声音族第四编(D74~D77)**:LoadSound 惰性工厂物化为整段 PCM vector(D74)、wav 的 NotSupportedException 前缀逐字与 Skip 越端时机(D75)、westwood_compressed 越界等价抛与 byte 回环保留(D76)、ima_adpcm 尺寸契约消息逐字(D77);**视频族第五编(D78~D79)**:vqa 的 cbf 重指以 heap + span 别名、参数-less 异常 .NET 全名 + 默认消息、IVideo 属性 → 方法/span 形态(D78)、wsa 每帧数组以成员 vector 重置零等价(D79);**主循环、mods 运行时 trait、UI、服务器、Lua 脚本(Phase 5-8 范围)尚未移植**——见上方状态节。

## 许可证与归属

本项目是 OpenRA 的衍生作品，按上游许可证以 **GPL-3.0** 发布，见 [LICENSE](LICENSE)。

- 上游版权所有：Copyright (c) OpenRA Developers and Contributors
- 本项目的重写代码：Copyright (c) 2026 xfcyhuang

OpenRA、Command & Conquer、Red Alert 及 Dune 2000 相关商标归各自权利人所有，本项目与这些权利人无从属关系。

---

<a name="english"></a>

**[中文](#opencpp-1)** | English

## Overview

A **C++26 semantic rewrite** of the [OpenRA](https://github.com/OpenRA/OpenRA) game engine.

The engine is rewritten from scratch to match the exact semantics of the upstream OpenRA source code (baseline commit `7d57605bca`), while keeping data, map, and replay formats compatible with upstream.

## Status

**Phase 3 complete** (simulation core + orders/lockstep; 2026-10-03):

- `src/sim/`, 10 files: TraitDictionary (parallel arrays with binary search, payloads keyed by the gen-exported TypeId upcast tables), Actor (condition system / Initialize observer chains / idempotent frame-end Dispose), World (tick ordering / the n-consecutive SyncHash formula), a line-by-line Activity state machine, the Sync hash protocol (including the IL-reachable bool semantics), Target/Player/Effects/TypeDictionary/ActorInitializer
- `src/net/`, 8 files: byte-exact Order serialization (expected-byte bit assertions + round-trip identity), OrderPacket/OrderIO, EchoConnection, the full OrderManager lockstep (three-stage TryTick / IsNetFrame throttling / frame validation), and a runnable UnitOrders subset
- Acceptance: EchoConnection **10⁶ ticks on both builds (ASan+UBSan / Release), leak-free and desync-free**; the full ctest suite 11/11 on both builds; 85 UPSTREAM tags validated; deviations D25–D34 registered

**Phase 2 complete** (metadata framework + the data-loading chain; 2026-10-03):

- `tools/schema_dumper --gen` authoritative export: 680 types (601 loadable) + 37 enums + 9 BitSet tagsets + the TypeId key space (6048 full type names) + the [VerifySync] member tables (84 types); `src/meta/` GenericValue/GeneratedRecord value bags with two-way typed memory transforms
- The full loading chain: Manifest → ModData → Ruleset → ActorInfo (`@` instance names / interface-dependency topological order) → WeaponInfo; all 24 LoadUsing loaders ported semantically
- Acceptance: three-mod deep-parse dumps **compared byte-for-byte as frozen snapshots** (ra 80,274 / cnc 49,416 / d2k 35,833 lines); upstream `--check-yaml` exit=0 as corroboration; deviations D10–D24 registered

**Phase 1 complete** (MiniYaml + file system; 2026-10-03):

- `src/yaml/`: a statement-by-statement rewrite of upstream MiniYaml.cs (791 lines) — the line state machine (4-space/1-tab levels, `\#` escaping, `\ ` whitespace guards), string-pool interning, merge/inheritance resolution/`-Key` weak removals, normalized serialization, and the mutable builders; `src/core/text.hpp` replicates the .NET `char.IsWhiteSpace`/`Trim` semantics over UTF-8
- `src/fs/`: FileSystem (mount order as override priority, `'|'` explicit mounts, case-insensitive path resolution), Folder (read/write), ZipFile (read-only, SharpZipLib→**miniz**, with the subfolder view and nested zips), and `MiniYaml::Load`
- **First-gate acceptance: all 759 upstream mods yaml files × both modes (discard/keep comments) match the C# oracle byte-for-byte on `FromStream→WriteToString` output** (13.5 MB golden data in `tests/golden_yaml.txt`); all 28 upstream MiniYamlTest.cs cases ported with verbatim assertion texts; fs_test's 24 assertions all green
- Project gates: strict `import std;` enforcement (no classic std-header includes, forced by `tools/std_import_check.py`), function-level dead-code elimination (`-ffunction-sections -fdata-sections` + `--gc-sections`), UPSTREAM provenance tags (`tools/upstream_check.py`), bilingual (Chinese/English) comments throughout
- Both build flavors (ASan+UBSan and Release) pass ctest; 9 registered deviations in docs/COVERAGE.md

**Phase 0 complete** (C++26 skeleton + fixed-point primitives):

- Toolchain baseline: clang `-std=c++26` + `import std;` (precompiled std.cppm PCM) + CMake/Ninja + ctest; determinism flags `-fwrapv -fno-strict-aliasing`
- 12 headers in `src/core/`: fixed-point primitives (WPos/WVec/WAngle/WRot/WDist/CPos/CVec/MPos/int2/Rectangle/Int32Matrix4x4/ISqrt) and MersenneTwister, rewritten statement-by-statement from upstream sources, fully `constexpr`, each file carrying a `// UPSTREAM:` provenance tag
- Acceptance: **60,883 lines of golden differential testing match the C# output 100%** (covering full-circle trigonometry, exhaustive ArcSin/ArcCos, full MT19937 sequences, and the decimal truncation semantics of LerpQuadratic); clean under both ASan+UBSan and Release builds
- Upstream sync mechanism in place: `UPSTREAM.baseline` (commit `7d57605bca`), coverage registry in `docs/COVERAGE.md`, and the `tools/upstream_check.py` validator

**Core infrastructure additions** (2026-10-04, landed alongside the upstream review):

- `src/core/percent_modifiers.hpp`: the upstream `ApplyPercentageModifiers` C# `decimal` chain (128-bit soft decimal, 22 sim call files) reproduced exactly with `__int128` — accumulated numerator over denominator `100^k`, integer division = `(int)decimal` truncation towards zero
- `src/core/arena.hpp`: the PORTING_PLAN §4.5 memory regions — FrameArena (frame-transient, trivially destructible only) / WorldArena (per-world, reverse-order destructor records, idempotent Destroy, Reset reuse)
- `tests/core_test.cpp`: decimal-semantics assertions (including the early-truncation divergence floor case) + arena lifecycle assertions; ctest 12/12 on both builds

**Phase 4 first batch complete** (platform-layer skeleton + the render command buffer; 2026-10-04):

- `third_party/SDL2`: the official 2.32.10 MinGW x64 dev package
- `src/platform/`: gl_types (103 GL constants value-checked one by one) + gl_loader (78 entry points loaded table-driven; the KHR_debug callback replaces glGetError polling) + sdl2_window (three window modes + a **packed atomic geometry snapshot**, lock-free getters)
- `src/gfx/`: gfx_command (**value-type commands + inline payloads over an SPSC lock-free byte ring**, replacing the upstream boxed message queue) + render_thread (a **unified threading model**: the render thread always exists and solely owns the GL context; the consumer state-diffs bindings)
- Acceptance (platform_test): a two-thread **200,000-record** sequence-integrity stress test plus desktop-GL integration — a **NPOT FBO** (333×257) passing FRAMEBUFFER_COMPLETE directly, and glReadPixels pixel assertions for the clear and a shader triangle; auto-SKIP on headless hosts; ctest 13/13 on both builds; deviations D35–D40 registered

**Phase 4 third batch complete** (Sheet/SheetBuilder/Sprite + the Palette family + HardwarePalette; 2026-10-05):

- Six files in `src/gfx/`: sprite (four enums + Sprite with precomputed 1/128-inset normalized coordinates + SpriteWithSecondaryData), vertex (the 48-byte layout field by field + the combined attribute contract), gfx_util (**FastCreateQuad's bitfield packing bit-for-bit per the combined.vert contract**, every FastCopyIntoChannel path, the fast integer uint32 premultiply, rotation/bounds/NextPowerOf2, plus the Vector2/3 rendering arithmetic), sheet (**shelf packing + the Indexed four-channel rotation + the dirty full/sub-rectangle switch** + buffer-transfer reuse), palette (Immutable/Mutable/Remap/PaletteReference with the byte-stream <<2|>>6 semantics), hardware_palette (**OPT-A7: palette dirty rows** — single-row changes upload incrementally via per-row SetSubData, falling back to a full upload past half-dirty; byte-exact OPT-C5 readback assertions)
- Acceptance (gfx_test): pure logic (shelf geometry/channel rotation/dirtyRegion unions/palette byte-stream boundaries/premultiply boundaries/every bitfield bit/every copy path) + GL integration (full and sub-rectangle sheet uploads with readback, the buffer-transfer GL path, the palette **incremental == full-reference byte-for-byte** check); ctest 14/14 on both builds; two real bugs fixed (the palette growth zeroing written rows; GetData reading the wrong texture unbound); deviations D46–D50 registered

**Phase 4 second batch complete** (the input layer + GL resource wrappers; 2026-10-05):

- `src/platform/`: keycode.hpp (the 238-entry enum reproduced value-for-value, asserted against the SDL headers) + sdl2_input (the event pump event by event: modifier sampling, **motion coalescing**, X1X2 as pseudo-keyboard, wheel/UTF-8 text, exit reporting) + MultiTapDetection/TapHistory (the three-slot 250ms/displacement-4 multi-tap detection with an injectable clock); focus/suspend atomics added to sdl2_window
- `src/gfx/`: shader (the **{VERSION} substitution plus post-link active-uniform enumeration into an integer table (OPT-A6: no string hot path) plus sampler-unit assignment**, integer Set*/Location APIs) + texture (**BGRA uploads/UNPACK row packing/RGBA16F/readback/ScaleFilter with RAII asynchronous deletes**); 13 commands added to gfx_command
- Acceptance (platform_test): pure logic (multi-tap timing/distance boundaries, modifier bit combinations, coordinate-rounding boundaries, Keycode×SDL cross-checks) + the synthetic event pump (double-click 1→2, three motions coalesced to one, wheel/text/exit) + GL integration (**NPOT BGRA byte-exact round-trips, sub-rectangle pitched bitmaps, a white-texture × red-uniform sampling chain with pixel assertions, a healthy pipeline after RAII teardown**); ctest 13/13 on both builds; three first-batch bugs fixed on the way (the misaligned SPSC record / the leaked GL context / the payload over-read); deviations D41–D45 registered

**Phase 4 fifth batch complete** (the first Westwood formats installment: the codecs + the SHP/TMP image chain; 2026-10-05):

- Twelve files in `src/formats/`: fast_byte_reader (semantics kept + equivalent out-of-bounds throw points), span_reader (UPSTREAM: NONE, the in-house Stream-shape adapter), lcw (the Format80 five-case decoder + the "quick and dirty v2" encoder), xor_delta (the Format40 six cases), rle_zeros (Format2), lzo (the minilzo 2.06 C# port copied control flow for control flow, gotos and labels faithful, unaligned accesses little-endian byte-assembled), crc32 (the 256-entry table value for value + the chaining Update/Finish), shp_td (the header table / reference decompression chain / TrimmedFrame bounds-trimming / the infinite-recursion guard), shp_d2 (the 2/4-byte frame-offset flavors + the lookup table), shp_ts (three scanline formats + the bogus-frame probe loop), tmp_td/tmp_ra (the one-byte-index tile sets), tmp_ts (the diamond unpack + cliff extras + the depth second frame set); `src/gfx/sprite_frame.hpp` (the ISpriteFrame contract) + the SheetBuilder::Add(ISpriteFrame) wiring (the D47 leftover)
- Oracle: `tools/golden_gen -- fmt` (upstream DLLs via extern alias, chained per each mod's mod.yaml SpriteFormats order) generates `tests/golden_formats.txt`: **every one of the 213 mods .shp files (186 ShpTD / 27 ShpTS) with per-frame Type/Size/FrameSize/Offset (.NET float formatting)/Data-CRC32 + the first 32 bytes hex, all 256 entries of the 6 .pal files' ImmutablePalette, the synthetic tmpTD/RA/TS fixtures (the same construction algorithm in both languages), four LZO vectors (including the M2 long match and the MatchNext double-read paths), and an LCW encode vector** — 6096 lines matching line for line, with the oracle's cross-run determinism verified
- Acceptance (formats_test): pure logic (FastByteReader / both RLE0 branches / the XOR six cases / the LCW five cases + an Encode→Decode round trip + overflow throw points / CRC32 standard vector + segmented chaining, the synthetic shpTD's TrimmedFrame trimming and all-zero frame, negative probes) + the golden differential + the SheetBuilder frame wiring (Indexed-sheet Red channel = byte 2); ctest 15/15 on both builds; two real bugs fixed (a one-digit CRC32 table typo at idx194 — the standard vector touches only 9 table entries, located via zlib cross-validation; the missing RLE0 literal-branch bounds check — proven by ASan); deviations D58–D62 registered

**Phase 4 sixth batch complete** (the second Westwood formats installment: the in-house Png + the Pfim port of Tga/Dds + ShpRemastered + EmbeddedSpritePalette; 2026-10-05):

- Five files in `src/formats/`: png (**OPT-B3: upstream's own 592 lines kept verbatim** — the chunk loop / IHDR checks / PLTE / partial-alpha tRNS / tEXt ASCIIZ + duplicate-key update-in-place / IDAT-chain stitching / zlib inflate + the five unfilters + 1/2/4-bit unpacking / the raw-pixel ctor's BGR↔RGB big-endian swap / Save's chunk order + chained CRC32 writes + filter-0 rows; zlib via miniz, D63/D64), targa (**a verbatim-semantics port of Pfim v0.11.3** — the 18-byte header / four orientations / uncompressed + RLE (the TopLeft-RLE tight-packing quirk included) / 4-byte-aligned strides / the color-map application with its upstream newLen quirk), dds (a Pfim subset — the uncompressed 8/16/24/32bpp paths + the bitmask R/B swap + the Rgba16 nibble swap + mip chains, the DXT1/3/5 block decoders (the RGB565 float interpolation verbatim + the (byte)(x+0.5f) truncation), double-truncated mip dimensions; the BC4/5/DX10 family explicitly rejected = D66), shp_remastered (zip container: the lazy-prefix regex reproduced as a leftmost hand match, blank gap frames, the meta JSON hand-parsed full-string, the prefix-mismatch throw verbatim), embedded_sprite_palette (the frame/file negotiation)
- Oracle: `tools/golden_gen -- fmt` gains four synthetic-fixture sections (the same construction in both languages; **stored-deflate zlib and the hand-rolled zip keep the two languages byte-identical**): 7 PNGs (all five filters / bit depths 1/2/4/8 / PLTE + partial tRNS / the two-chunk IDAT split / duplicate tEXt / an unknown chunk + **the Save structural differential = the chunk-type-order CRC + the IDAT-decompressed CRC**, D64) + 5 TGAs (three orientations × 24/32bpp + two RLE flavors) + 7 DDSs (uncompressed with the mask swap / three mips / DXT1 interpolated + punch-through / DXT3 / DXT5 both gradients) + 1 ShpRemastered synthetic zip (a meta-cropped frame / a blank gap frame / a plain frame / an unrelated entry) — the golden grows 6096 → **6183 lines matching line for line**, with the oracle's cross-run determinism verified
- Acceptance (formats_test): new pure logic (nine PNG negatives + the Save round trip / chunk order / per-chunk CRCs / filter-0 row stream, Tga/Dds/ShpRemastered/EmbeddedSpritePalette negatives and four states); ctest 15/15 on both builds; deviations D63–D67 registered

**Phase 4 seventh batch complete** (the mix package + Blowfish; 2026-10-05):

- Three files in `src/formats/`: blowfish (the 18 + 4×256 P/S boxes copied value for value + the 16-round Feistel line by line + RunCipher's big-endian load/store with the odd trailing element zeroed), blowfish_key_provider (the "direct C port" kept verbatim — the DER public-key unwrap / little-endian bignums over 16-bit limbs / the reciprocal and Knuth-D quotient estimation (**all intermediates accumulate in 64 bits, matching C#'s int*uint promotion to long**) / square-and-multiply / the (55/a+1) block grouping), xcc_database (the local 48-byte-header parse + the Data() writer; the global block parse)
- Three files in `src/fs/`: package_entry (the Classic rotate-1 and the CRC32 pad-byte hashes), mix_file (the three-format detection + the encrypted header's 80-byte keyblock RSA→Blowfish whole-block decrypt + the local/global database dual-hash name resolution; the MixLoader suffix sniff), d2k_sound_resources (the .rs directory parse with absolute-offset content)
- Wiring: Manifest.PackageFormats parsing + ModData constructing the Mix/D2kSoundResources loaders by name → the FileSystem construction (the ModData.cs L63-65 equivalent)
- Oracle: golden_gen gains the five Q/KB/BE/M/X/G sections (the encrypted fixture's **construction side runs the embedded verbatim Blowfish/KeyProvider copies while parsing runs the upstream MixLoader.MixFile — a cross-language closed loop**), the golden growing 6183 → **6227 lines matching line for line**, with the oracle's cross-run determinism verified
- Acceptance (formats_test): the Blowfish published standard vector / round trip / odd-tail, the Xcc round trips and truncation negatives, HashFilename padding boundaries, the minimal mix fixture + the garbage-keyblock negative + the sniff, the .rs absolute offsets + the duplicate-key negative; ctest 15/15 on both builds; deviations D68–D73 registered

**Phase 4 eighth batch complete** (the aud/wav sound formats; 2026-10-05):

- Four files in `src/formats/`: ima_adpcm (the whole of ImaAdpcmReader.cs: the 8-entry IndexAdjust / 89-entry StepTable value for value, both int divisions truncating towards zero, both saturations), westwood_compressed (the five branches = the 2-bit / 4-bit deltas / literals / the single-sample jump / fill, the jump's non-saturating byte wrap kept), aud_reader (the 12-byte header + the 0xdeaf chunk header; the IMA stream's index/currentSample persisting across chunks and the odd-outputSize half-nibble truncation; the WS stream's grow-means-fresh-zero / no-grow-residue semantics), wav_reader (the RIFF chunk loop plus the three decode paths PCM passthrough / IMA interleave / MS ADPCM, including outputSize = uncompressedSize×channels×2 and the missing-fact -1 cut-right-after-the-first-sample quirk)
- Oracle: every one of the 46 mods .aud + 4 .wav real assets + 13 synthetic fixtures + the IMA/Westwood codec vectors, the golden growing 6227 → **6547 lines matching line for line**; one real bug fixed (the fixed-32 interleave buffer doubling mono output); ctest 15/15 on both builds; deviations D74–D77 registered

**Phase 4 ninth batch complete** (the vqa/wsa video formats; 2026-10-05):

- Five files in `src/formats/`: video (IVideo/IVideoLoader/the GetVideo chain: properties → methods, byte[]/null → spans), vqa_video (the whole of VqaVideo.cs: the FINF offset table's 0x40000000 flag, the SND0/SND2 collection (SND2 through IMA, stereo interleaving, the odd-length +=2 quirk), the DecodeVQFR subchunk loop (the CBFZ reverse mode + the HQ RGB555 expansion, the CBP0/CBPZ partial codebooks applied "the frame after", the VPTZ/VPRZ/VPTR instruction paths, VQFL's early return), the two DecodeFrameData paths — non-HQ 8-bit and HQ five-case), wsa_video (the palette's <<2 + high-bit replication, the +768 offsets, LCW + the XOR delta over the previous frame, the row-stride recomputation; the IsWsa sniff with the always-false `width <= 0` ushort quirk), plus SpanReader gaining Peek (the EOF -1 semantics)
- Oracle: six synthetic fixtures built identically in both languages (the non-HQ full path / odd-length stereo / the two CBP rounds / the HQ three instruction paths / WSA in both padding modes / a sniff negative), the golden growing 6547 → **6606 lines matching line for line**; one real bug fixed (AdvanceFrame leaving has_previous unset, XORing frame two against a zero baseline); ctest 15/15 on both builds; deviations D78–D79 registered

Next batch (Phase 4 continued): vxl/hva/idx, OpenAL/FreeType, hardware cursors, and WorldRenderer/render collection (OPT-A7 SOA + the frame arena).

## Differences from upstream (optimizations & registered deviations)

Semantic equivalence is this project's first principle (verified by golden differentials); on top of that, deliberate C++-side optimizations are made. Wherever exact semantic equivalence is impossible or intentionally forgone, a deviation is registered in [docs/COVERAGE.md](docs/COVERAGE.md) (**an unregistered deviation is treated as a bug**).

### Deliberate optimizations (C++ side)

| Area | Upstream C# | C++ side |
|---|---|---|
| Data reflection chain | `FieldLoader`/`ObjectCreator` runtime reflection + `TypeDescriptor` converter fallback | `gen/` compile-time descriptor tables (680 loadable types, exported authoritatively from C# reflection by schema_dumper) + registry factories; no runtime reflection |
| Type/interface queries | `Dictionary<Type,…>` hashing + `GetInterfaces()` reflection | the TypeId key space (6048 full names → `uint16`, gen-exported) + parallel-array binary search |
| SyncHash | `Reflection.Emit` runtime IL generation + a `ConcurrentCache` delegate chain | `gen/sync_gen.cpp` generated at compile time (84 types), zero reflection, zero delegate indirection |
| Percentage modifiers | the C# `decimal` chain of `ApplyPercentageModifiers` (soft decimal, an order of magnitude slower than int64) | exact `__int128` reproduction (integer division = `(int)decimal` truncation towards zero; the equivalence-domain argument is in the `src/core/percent_modifiers.hpp` header) |
| Memory management | GC (per-frame LINQ/closure/boxing allocations) | partitioned arenas: `FrameArena` (frame-end reset) + `WorldArena` (per-world bump + destructor records); no `shared_ptr` on the synced path |
| Collection parameters | `IEnumerable<int>` (LINQ chains + enumerator allocations) | direct `std::span` (the numeric-modifier chain is the first landing point) |
| Enums / exceptions | string switches / scattered exception types | `enum class` + a unified `YamlException` (message texts still match verbatim) |

The bigger items ahead (a render command buffer replacing the boxing message queue, generation-stamped pathfinding state, condition-name interning, ActorID-array spatial indexes, and 40+ more) land per phase together with their ports; the full review evidence (upstream file:line anchors) is in [docs/UPSTREAM_CPP_REVIEW.md](docs/UPSTREAM_CPP_REVIEW.md).

### Not-yet-synced with upstream (registered-deviation summary)

Currently **D1–D79** (full texts in [docs/COVERAGE.md](docs/COVERAGE.md)), by module:

- **yaml/fs (D1–D9)**: exception types unified into YamlException (message texts verbatim), lazy enumerations materialized into vectors, null/"" key coalescing, etc. — behavior-equivalent or unobservable for valid inputs;
- **meta/loading chain (D10–D24)**: the TypeConverter fallback not implemented (actual field types are fully covered, unreachable), dictionary fields as insertion-ordered vectors (dump protocol sorts by key), the three-mod parse snapshots frozen on the C++ side (D24: the C# `--dump` tool is constrained by the ALC assembly-copy environment; re-differential once restored), etc.;
- **sim/net (D25–D34)**: the Initialize observer-dedup shape differs (receiving ends are idempotent), **the trait factory and ownership await Phase 5 wiring into the World arena (D26/D27)**, `Target.FromCell` stubbed with the square-grid formula (D28, Map lands in Phase 5), UI/lobby command families silently swallowed (D29, wired in Phase 6/7), SyncReport not wired (D30, off by default upstream), etc.;
- **Platform/render (first batch D35–D40, second D41–D45, third D46–D50, fourth D51–D57)**: the command buffer replacing the boxing message queue, the unified threading model, NPOT, KHR_debug, the texture lifetime contract (replacing the glIsTexture eviction), the persistent-mapped VBs/VAO cache/blend diff/single-pass compositing, etc., plus input-layer shape adaptations (exit reporting / clock injection / X1X2 IsRepeat); **the first formats installment (D58–D62)**: Stream → SpanReader, loaders each starting fresh at offset 0, Data null/empty unified, out-of-bounds exceptions as equivalent runtime_error throws, frame data shared via shared_ptr; **the second formats installment (D63–D67)**: the zlib layer's miniz shape (D63), Save's implementation-defined deflate bytes (D64), the Pfim dual-path unification (D65), the BC4/5/DX10 rejection (D66), the hand-written regex reproductions (D67); **the mix-package family, third installment (D68–D73)**: HashFilename's ASCII hash domain (D68), the mix byte residency / non-contractual ordering / no global-database retry (D69), the dropped unresolved log + ReadBlocks' loose bounds check kept verbatim (D70), the Xcc ASCII domain (D71), the .rs duplicate-key equivalent throw (D72), the loader name-dispatch table (D73); **the sound family, fourth installment (D74–D77)**: LoadSound's lazy factory materialized into a whole-PCM vector (D74), wav's NotSupportedException prefixes verbatim and the Skip-past-end timing (D75), westwood_compressed's equivalent throws with the byte wrap kept (D76), ima_adpcm's size-contract message verbatim (D77); **the video family, fifth installment (D78–D79)**: vqa's cbf re-pointing as heap + span aliases, the parameterless exceptions as .NET full names + default messages, the IVideo property → method/span shape (D78), wsa's per-frame arrays as member vectors reset to zero (D79); **the main loop, runtime mods traits, UI, server, and Lua scripting (Phases 5–8) are not ported yet** — see the status section above.

## License & Attribution

This project is a derivative work of OpenRA and is released under the **GPL-3.0**, the same license as upstream. See [LICENSE](LICENSE).

- Upstream copyright: Copyright (c) OpenRA Developers and Contributors
- Rewrite code in this project: Copyright (c) 2026 xfcyhuang

OpenRA, Command & Conquer, Red Alert, and Dune 2000 are trademarks of their respective owners; this project is not affiliated with them.

# OP3 自动对焦执行计划（交给 5.6luna）

编制日期：2026-09-13。范围：后摄 IMX298 + BU63165GWL。本文是执行交接，尚未执行新的手机测试。

目标顺序：**有效上电实验 → 控制芯片通信 → 镜头实际移动 → 单次自动对焦 → recovery 集成**。
每个阶段独立记录结果、独立提交。前一阶段没有证据，不能把后一阶段标为完成。

## 1. 先纠正旧结论

| 旧说法/现象 | 当前可成立的结论 |
| --- | --- |
| “保持 IMX298 subdev 打开，已经排除传感器供电问题” | **这个实验无效。** 当前 `imx298_internal_ops` 只有 `.init_state`，没有 `.open` 上电回调；`imx298_s_stream(1)` 才调用 `pm_runtime_resume_and_get()`。旧日志在 `sensor-opened` 之后也没有新的 IMX298 `power_on`。供电假设仍为 INCONCLUSIVE。 |
| `BU63165GWL ready`、出现 lens subdev | `bu63165gwl_probe()` 没有访问芯片识别寄存器，只能证明软件注册完成，不能证明芯片 ACK。 |
| `VAF enabled voltage=2800000`、GPIO39 logical=1 | 证明驱动调用及框架读值；不能代替模块引脚电压、波形的物理测量。 |
| `VIDIOC_G_CTRL` 返回请求位置 | 当前驱动没有硬件位置读回，读到的是 V4L2 缓存；不能作为镜头移动证据。 |
| 已有 autofocus 驱动 | 只有 `FOCUS_ABSOLUTE` 位置控制和扫位置小程序；闭环搜索算法尚未实现。 |
| 1476×834 RAW 连续采集成功 | 2026-09-09 已记录同一次 STREAMON 的两帧成功；不能外推为当前 AF/CCI100 镜像长期稳定。 |
| `0x1c` 改成 `0x0e` 就确认地址正确 | 当前候选使用 Linux 7 位地址 `0x0e`；原厂运行时 SID 来自用户态 `actuator_params.i2c_addr >> 1`，还应解出本机 actuator 库的字段，不能只看 DT 的 `reg`。 |

历史失败：VCM 首次位置 128 写入 `f0 90 00 00 80` 返回 `-110`，CCI 日志有
`addr=0x0e len=5 cur=4`，随后 `irq=0 cur=1 exec=4 cmd=0x0090f059`。
这能定位一次超时，不能单凭此判定错误地址、缺初始化或 CCI 硬件故障。
以上设备数据来自前次会话，不是 9 月 13 日复测结果。

## 2. 固定工作位置与证据

所有相对路径以 `/home/kai/src/oneplus3-mainline` 为根。

- 项目分支：`agent/implementation/recovery-browser-001`；本次审查起点 `3530e969122f02e4e8baaad612adc91d3f471b8a`。
- 独立相机内核：`source/linux-pmos-msm8996-6.12-camera-imx298`，分支 `agent/implementation/op3-camera-imx298-001`。
- 相机起点：`004cda8e061387d07ab4dd08c474910eedd3eb24`；tree `fe0ab3370df353df031c8a91b15801634b80dba1`。
- 正式上游基线仍为 `BASELINE.env` 中的 Linux 6.12.1 / `67b0bbc3cbf46bae712a2606a43361756fcbd829`。
- 相关 [GitHub Issue #12](https://github.com/iamgreatwk/op3/issues/12) 的正文仍是初始相机范围；用户后续已明确扩展到自动对焦。各阶段使用本文的本地任务名，不能虚构新 Issue 编号或套用旧的 probe 验收条件。
- 正式 recovery 内核和 `manifests/op3-recovery-audio-full.env` 保持为已验证产品记录；AF 实验单独记录，不提前提升为正式基线。

开始时执行以下只读检查；分支/HEAD 不符时先解释差异，不能自动 reset：

```bash
cd /home/kai/src/oneplus3-mainline
./scripts/agent-start.sh
git status --short --branch
git branch -vv
git worktree list
git -C source/linux-pmos-msm8996-6.12-camera-imx298 status --short --branch
git -C source/linux-pmos-msm8996-6.12-camera-imx298 branch -vv
git -C source/linux-pmos-msm8996-6.12-camera-imx298 worktree list
git -C source/linux-pmos-msm8996-6.12-camera-imx298 rev-parse HEAD 'HEAD^{tree}'
gh issue view 12 --repo iamgreatwk/op3
```

审查时项目已有未跟踪的 `-o`、`host-tools/`、`patches/pmos612-op3-camera-imx298/0020-media-i2c-use-OP3-BU63165GWL-sequential-write.patch`，保留原状。
GitHub 的相机补丁档案尚未覆盖本地全部 AF 实验提交；迁往另一台电脑前，先核对并导出**缺失的相机提交**，在临时重建目录验证最终 tree 相同。不能声称只下载当前 GitHub 即可复现 AF 候选。

本机已重新校验的文件（用于辨认历史候选，不代表 AF 成功）：

| 文件 | SHA256 |
| --- | --- |
| `artifacts/boot-oneplus3-pmos612-recovery-imx298-afpwdm-20260909.img` | `64ccf213644f517f8c8b8c512869f396cb75c90920f89163fb8385a244dce4e2` |
| `artifacts/boot-oneplus3-pmos612-recovery-imx298-vio-smd-lvs1.img`（旧采集对照） | `8809a46d5be7b5f983a0d3acfb27bd33c34b12bcb8951d486d123d8252933e84` |
| `out/pmos-msm8996-6.12-camera-imx298-cci100/vcm-af-pwdm-module/bu63165gwl.ko` | `cd638af89815c4967ba5e718dc4c4f20130a972da9ba3736bfe16b114fa0b063` |
| `out/pmos-msm8996-6.12-camera-imx298-cci100/imx298-af-pwdm-module/imx298.ko` | `103fe40cad22972ed3408b76f997dc8a06e1c09a4b2dba03e8dc71874144e8f9` |
| `out/pmos-msm8996-6.12-camera-imx298-cci100/.config` | `b716c8c4bf04ab0a1f002bfa7314f29a112c07e901b9efa9029b15f3f28387c5` |
| `artifacts/op3-imx298-probe-modules.tar.gz` | `33918d7cb399894a719f1567091054eaceb2eec8c6d96586ad61f26cdd6739ef` |
| `out/pmos-msm8996-6.12-recovery-audio-full-s1302-poll-drm100/Module.symvers` | `4a1d8355871c5f98cdc09d9d3cb5db7c7c9b9f4ed22b996ac8be377250096cfd` |

注意：旧 module bundle 不包含后来的 AF 修正；其同名 IMX298/CCI 文件不能覆盖本轮选定版本。
手机 `/newroot/tmp/*.ko` 的名字也不是版本证据。每次传输都用版本目录，比较主机和手机 SHA256，并记录明确的 `insmod` 路径。
上次使用的 CCI 调试模块来源为 `cfd8372d7f74`，手机暂存文件是否仍在尚未复核。
重新获得精确文件或从该提交重建后才可声称重现上次组合；不能替换为 `c36c728e704a` 的失败实验模块。
**已发现的旧产物陷阱：** 本机 `out/pmos-msm8996-6.12-camera-imx298-cci100/drivers/i2c/busses/i2c-qcom-cci.ko`
的SHA是 `dc723a10678d848dfd09364c350b81e9d124012d12ff5633a23baef77d3984e4`，仍是已revert的EXEC计数实验产物，不能直接上传。
当前HEAD的CCI源文件与 `cfd8372d7f74` 完全相同（已用git diff核对）；需要时从**当前干净HEAD**单独重建CCI外置模块，记录新的哈希，无需切换整个工作树到旧commit。

## 3. 每阶段执行卡

### G0：补齐原厂控制芯片协议证据（主机只读审查）

任务名 `OP3-AF-G0`。假设：现有 VCM 实现遗漏原厂必要的供电/初始化前置条件。
变量：只补充证据，不改运行代码。交付 `docs/handoff/op3-af-protocol-audit.md`。

原厂同机型参考固定在 LineageOS commit `b2a6b3b561067a4de54a0d60524659cb99bdcd3b`：

- [15801 相机 DTS](https://github.com/LineageOS/android_kernel_oneplus_msm8996/blob/b2a6b3b561067a4de54a0d60524659cb99bdcd3b/arch/arm/boot/dts/qcom/15801/msm8996-camera-sensor-mtp_15801.dtsi)：actuator 和 OIS 均指向 CCI0 / `reg=0x1c`，OIS 节点声明 L23 2.8 V。两者可能共享控制芯片资源，不能把 OIS 相关初始化预先排除。
- [msm_actuator.c](https://github.com/LineageOS/android_kernel_oneplus_msm8996/blob/b2a6b3b561067a4de54a0d60524659cb99bdcd3b/drivers/media/platform/msm/camera_v2/sensor/actuator/msm_actuator.c)：追踪 `msm_actuator_set_param`、`msm_actuator_init_focus`、`msm_actuator_write_sequence`，核对用户态地址、寄存器地址宽度、初始化表和位置值。
- [msm_ois.c](https://github.com/LineageOS/android_kernel_oneplus_msm8996/blob/b2a6b3b561067a4de54a0d60524659cb99bdcd3b/drivers/media/platform/msm/camera_v2/sensor/ois/msm_ois.c)：存在配置/初始化写表、固件下载、状态读回路径。需要进一步确认本机库实际走哪条路径，不能把通用下载函数视为本机必需固件的证明。
- 同一提交下 `sensor/io/msm_camera_cci_i2c.c`、`sensor/cci/msm_cci.c`：核对 SID 转换、顺序写的字节布局及事务边界。

本机离线备份：
`/home/kai/下载/WorkBuddy-20260826/WorkBuddy/2026-08-04-10-03-12/agent-os/backups/partitions_20260818/android/vendor.img`。
使用 `debugfs` 只读提取，禁止 `-w`。已核对内部路径/大小/文件内容 SHA256：

| 内部文件 | 大小 | SHA256 |
| --- | ---: | --- |
| `/lib/libactuator_rohm_bu63165gwl.so` | 23044 | `1ef67290abb3b017a8ab688dcf15c0a39f64bb836f7c72a574452f4d5fe0adf9` |
| `/lib/libois_rohm_bu63165gwl.so` | 315408 | `7afafc91554b3ffeb676740eabbcae00b79d7ea4f3cc29fdeb4760d84f5dc52f` |

另有 `/lib/libmmcamera_imx298.so` 和 `/lib/libmmcamera_sony_imx298_eeprom.so` 可交叉核对。
提取物放在忽略的 `artifacts/op3-af-evidence/` 中，只归档相关相机数据，不提交整个 vendor/persist 镜像。
先用 `file/readelf` 确认 ELF 位宽、ABI、导出入口、重定位，再解配置结构；不能用猜测的 C 结构偏移，不能在主机直接加载 Android `.so`。

必须给出的协议表：7 位实际地址及依据、寄存器宽度/端序、VAF/逻辑电源/使能脚/MCLK要求、初始化步骤与延迟、是否需下载固件、可安全读的状态寄存器、有效位置范围及 park 位置。每项写“来源+位置”或 `UNKNOWN`。
PASS 是必要参数有可追溯依据；资料不足则 INCONCLUSIVE。未知的地址/写表不得交给 Luna 自行猜测。
本阶段允许研究 OIS 与 AF 的依赖，实际防抖功能仍不在实现范围。

### G1：做真正处于采集状态的对焦对照（仅改测试用户态）

任务名 `OP3-AF-G1`。假设：当前 VCM 写超时与没有活动相机电源会话有关。
唯一变量：**发送同一条 VCM 命令时，IMX298 是否正在输出帧**；内核、DTB、模块、CCI频率保持同一组。
允许修改 `scripts/op3-v4l2-stream-test.c`，也可基于它新增单独的 AF session helper；本阶段不改驱动。

先做无对焦的对照：在 AF_PWDM 候选镜像上，实际完成至少 4 次连续 DQBUF，确认电源日志、正常帧序号和格式；否则停止 AF，回到旧采集对照镜像定位采集回归。
曝光/增益先用已有参考 `893/240`。目标是光线充足的固定黑白文字或边缘图案；若明显欠曝或饱和，先单独固定可用曝光条件，再做 AF 比较。

为 helper 实现以下**待新增功能**（现有程序还没有这些开关，不可直接假定存在）：

1. 按 sysfs `name` 和 media graph 发现 IMX298、lens、VFE RDI 节点；不要硬编码 subdev17/18、video0、I²C总线5。
2. 设置 media links/RAW10格式、申请并排队至少4个buffer；成功 STREAMON 后等待4帧，才发布“采集已就绪”。
3. 独立采集线程持续 DQBUF/QBUF；对焦线程在就绪后打开 lens 并写一个有原厂范围依据的位置。首次先只发一次命令；原先128仅在G0确认合法时复用。
4. 两个线程不共用会被慢I²C操作占住的队列锁。不能在 DQBUF/QBUF 线程 sleep 等镜头，也不能在镜头超时时停止回收buffer，让CAMSS溢出掩盖原问题。
5. 在lens打开前、位置命令前后分别记录传感器 runtime_status、时钟框架状态、GPIO/电源日志、帧序号/时间戳、ioctl返回值与errno。出现位置写错误立即结束试验，不继续扫位置。
6. lens fd 始终保持至本次采集结束；退出时先停止对焦线程并等待在途ioctl返回，正常 STREAMOFF，再关lens和视频fd、释放buffer。SIGINT/超时也走同一有界清理路径。
7. GPIO39被两个驱动非独占使用，**没有共同引用计数**。初始化完成后才开始试验，期间不要重载驱动/关闭lens。清理前后记录GPIO39，不能宣称此共享方案已适合长期集成。

首个测试位置必须不同于control当前缓存值，并确认驱动实际进入write_position及I²C事务。
V4L2可能略过“设置为相同值”；当前VCM驱动也没有在每次resume自动恢复缓存位置，因此同值ioctl返回0不能算通信成功。

每轮保存：完整dmesg（不清空）、boot_id、uname、cmdline、模块哈希、media graph、runtime状态、逐帧sequence/timestamp/bytesused/flags、单次focus返回码。日志及时复制到主机版本目录。
正常 `DQBUF` 后也检查 `V4L2_BUF_FLAG_ERROR`、buffer索引、plane长度/data_offset；错误帧不参加图像验收。

判断：

- 采集前提成立且命令仍 `-110`：仅可排除“本次活动传感器会话本身足以解决超时”，转G2。
- 命令成功：通过通信小门槛，仍需G3证明镜头移动。与同镜像的非采集失败对照比较；若缺对照，在另一次干净启动补一次。
- 采集未建立、runtime状态不明、GPIO被另一方拉低：本次 INCONCLUSIVE，不能排除供电。
- 发生VFE溢出/SMMU fault/重启：停止、保存证据；下一次干净启动后继续，不能反复启动/停止流来碰运气。

### G2：按证据修正通信/初始化（每次只改一个原因）

任务名 `OP3-AF-G2-原因名`。只能选择下表一行作为当前实验；写清预期差异再提交。

| G0/G1发现 | 单变量动作 | 通过条件 / 不通过后的处理 |
| --- | --- | --- |
| 原厂库地址与当前0x0e不符 | 只改DTS地址、编DTB并临时boot；其余文件固定 | 原厂定义的安全事务收到预期响应；否则记录FAIL，不扫描全部地址 |
| 活动采集中仍有原厂要求的电源/使能缺失 | 单独修正一个电源/使能的资源管理（必要binding+DTS属于同一资源变更） | 状态日志与原厂时序相符且事务响应改善；与初始化表改动分开 |
| 有安全的芯片ID/状态读协议 | 先在VCM驱动新增可开关诊断读，保持供电和位置协议 | 非固定垃圾值且符合原厂语义；读失败不能用probe成功覆盖；不对未知寄存器随意写读指针 |
| 可通信，但原厂在位置写前执行初始化/固件/servo启动 | 仅移植可证明为AF前置条件的最小序列至BU63165GWL驱动 | 有原厂定义的就绪/校验依据，位置写成功；每次重新上电重建状态，失败反向清理 |
| 原厂位置协议与当前payload宽度/端序不符 | 仅修正该事务编码 | 返回完整传输长度且后续图像支持移动；短写必须失败 |
| 原厂协议/电源已核对，仍超时 | 先对照同次采集中已成功的IMX298寄存器写与VCM事务；新增必要CCI错误状态日志 | 有具体控制器差异才改CCI；不能从“VCM 5字节超时”推断“所有长写都坏了” |

原厂AF/OIS共享芯片的必要初始化可以属于AF前置工作，但是否需要尚未证实；不得一开始整包移植OIS或向非易失存储写校准。
若需固件，记录提取来源/哈希/加载与校验协议；CCI最大写长度要从当前adapter quirks核对，分块边界遵从原厂协议，不随意拆长包。
若仍无法分辨地址/ACK/clock-stretch，下一项是有针对性的SCL/SDA/电源测量，列出需测信号与判别标准，报告硬件证据缺口。不要靠重复重启替代测量。
之前queue1和逐字更新EXEC计数两项已失败并revert；无新证据不重复执行。

### G3：证明镜头移动且能改变焦点（用户态固定位置实验）

前提：G1或G2已实现有效通信；G0给出合法位置范围。任务名 `OP3-AF-G3`，变量仅为镜头位置。

- 固定手机、光线、目标距离、曝光和增益；保持一次STREAMON和同一个lens fd。
- 先选原厂合法范围内至少3个不同位置，再扩展为覆盖有效行程的稀疏采样；不把0/1023直接等同无穷远/微距。
- 每次移动后持续回收buffer；依据原厂settle时间丢弃过渡帧，再保存3帧有效帧。需根据驱动timestamp标志/时钟、frame sequence区分排队旧帧；初始保守等待150ms并排除既有队列帧只是测试参数，后续以测得稳定性调整。
- 记录位置请求/缓存值/时间/图像清晰度；当前位置没有硬件读回就明确写“无硬件位置反馈”。
- 输出相同ROI的并排图、RAW、位置—清晰度数据。回到同一位置再拍一次，检查重复性/回差。

PASS：位置变化造成可重复的边缘清晰度变化，超过同位置多帧噪声波动，且无总线/采集故障。
只看到I²C成功、噪声变化、亮度变化或control读回不算PASS。无变化则先核对初始化/servo/范围/目标条件，不能进入自动搜索。

### G4：实现轻量单次自动对焦（用户态）

任务名 `OP3-AF-G4`。假设：在已证实的有效位置区间，图像对比度搜索能稳定找到较清晰位置。
唯一变量：位置选择从手动改成算法；固定驱动/模式/曝光/增益。

1. 使用G3同一采集会话实现粗扫→峰值邻域细扫→回到最佳位置→再拍验证；每位置取3帧分数的中位数。
2. 首版可取有效区间8–12个粗点、峰值附近5个细点；把预算/settle/最大运行时间作为可记录参数，不能无限扫描。先做single-shot，暂不做PDAF/连续追焦。
3. 按协商格式解包RAW10并跳过行padding。已知模式参考为1476×834、bytesperline=1848、每帧1541232字节；运行时以G_FMT与DQBUF数据为准，不能按紧密连续1476×834×10/8读取整帧。
4. 从固定中央ROI提取同一Bayer绿色子格或固定方式生成亮度，避免直接在RGGB交错原始字节上算梯度。首版用归一化梯度能量，记录ROI均值、噪声与饱和比例；不用逐帧自动拉伸/白平衡后的图像评分。
5. 只有显著、可重复的峰值才返回FOCUSED；峰值与其它点的差距须大于G3测得的同位置噪声（例如至少3倍波动尺度）。低纹理/低信噪比返回LOW_CONFIDENCE，I²C/丢帧/超时返回ERROR，不能默认成功。
6. 稳定方向接近最终位置，验证帧分数接近扫描最佳分数；容差由G3重复测量给出。低置信度时回到有依据的安全/先前有效位置，而不是机械端点。

解包的最小单元测试：对于每组5字节 `b0 b1 b2 b3 b4`，
`p0=(b0<<2)|(b4&3)`，`p1=(b1<<2)|((b4>>2)&3)`，
`p2=(b2<<2)|((b4>>4)&3)`，`p3=(b3<<2)|((b4>>6)&3)`。
每一行独立从bytesperline定位，检查尾部padding不参与评分；仅在协商格式确为SRGGB10P时使用这套解包。

验收：近处（例如40–60cm文字）和远处（例如2m以上高对比目标）分别3次，使用一致图像处理生成前后对照；结果与该场景手动扫描的最佳区间一致。
再用低纹理目标确认LOW_CONFIDENCE、主动取消确认清理。保存算法分数、耗时、帧序号与选择位置。
初版手机只运行C helper和必要评分，可把图像展示放主机；无需引入浏览器、Mesa或整套Android camera HAL。

### G5：接入recovery与最终构建（通过后另一个阶段）

任务名 `OP3-AF-G5`。将single-shot启动/取消/状态/拍照接到recovery；相机打开后保持会话、对焦完成后拍照、关闭后释放资源。
此前先解决GPIO39共享所有权、运行时电源引用和驱动清理问题并复测；不能把两个nonexclusive GPIO使用者当成引用计数。
用户确认近/远照片效果后，再整理已通过补丁、模块闭包、必要固件来源和构建脚本，最后构建Buildroot并验收冷启动/重复拍摄/退出省电。
主项目push不包含嵌套内核提交；须验证导出的补丁能重建同一tree。正式接受由用户与Integration决定。

## 4. 编译、上传和故障恢复约定

- 当前用户要求是计划；本次没有编译、操作手机或创建新任务。后续执行沿用所有者此前对相机编译/测试的明确授权；若转入不含历史的新任务，在交接提示中带上这段授权范围。大型构建规则以现行AGENTS.md和所有者授权为准，旧模板的“永远禁止agent编译”已过时。
- 首选小C helper/单模块上传测试，DT变更才编DTB；不把modpost失败自动升级成全内核/Buildroot构建。
- 用户态helper使用 `aarch64-linux-gnu-gcc-11 -static -O2 -Wall -Wextra -Werror`；使用双线程时加 `-pthread`。源代码先提交，输出到新的实验目录，保留编译日志与SHA256。
- 新helper至少校验参数、RAW10行解包/ROI、固定位置评分重复性、错误和取消清理；G4前用可控清晰/模糊测试数据确认评分方向，不能仅对真实噪声帧自证。
- 内核模块必须使用同源、相容配置与完整符号表。当前CCI100输出没有顶层Module.symvers，完整参考符号表在上表路径且包含media/V4L2导出；先确认与运行内核/依赖模块的来源相符。`uname -r`/vermagic相同不单独证明ABI相容。
- 使用绝对 `M=` 外置模块目录和现有 `scripts/op3-camera-vcm-module.mk`，通过 `KBUILD_EXTRA_SYMBOLS`传完整表；不能用相对 `M=drivers/media/i2c`，不能给输出目录链接别的构建的 `vmlinux.o`，不能用 `KBUILD_MODPOST_WARN=1`/force-load掩盖未解析符号。

单模块命令模板（G2确定并提交候选后使用；不是要求现在执行）：

```bash
(
set -euo pipefail
project=/home/kai/src/oneplus3-mainline
kernel="$project/source/linux-pmos-msm8996-6.12-camera-imx298"
kout="$project/out/pmos-msm8996-6.12-camera-imx298-cci100"
symvers="$project/out/pmos-msm8996-6.12-recovery-audio-full-s1302-poll-drm100/Module.symvers"
test "$(git -C "$kernel" branch --show-current)" = agent/implementation/op3-camera-imx298-001
test -z "$(git -C "$kernel" status --porcelain)"
test -s "$kout/.config"
test -s "$symvers"
module_build=$(mktemp -d "$project/out/op3-af-vcm-module.XXXXXX")
git -C "$kernel" rev-parse HEAD | tee "$module_build/source-commit.txt"
sha256sum "$kout/.config" "$symvers" | tee "$module_build/inputs.sha256"
make -C "$kernel" O="$kout" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- \
  CC=aarch64-linux-gnu-gcc-11 modules_prepare 2>&1 | tee "$module_build/prepare.log"
ln -s "$project/scripts/op3-camera-vcm-module.mk" "$module_build/Makefile"
ln -s "$kernel/drivers/media/i2c/bu63165gwl.c" "$module_build/bu63165gwl.c"
make -C "$kernel" O="$kout" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- \
  CC=aarch64-linux-gnu-gcc-11 KBUILD_EXTRA_SYMBOLS="$symvers" \
  M="$module_build" modules 2>&1 | tee "$module_build/build.log"
sha256sum "$module_build/bu63165gwl.ko" | tee "$module_build/artifact.sha256"
)
```

缺顶层Module.symvers的warning只有在完整extra符号表确实被modpost读取、没有未解析/重复符号且ABI来源已核对时才可解释；其余情况停止并记录，不修改内核文件掩盖。
上面用子shell包住`set -e`，避免失败直接退出用户交互终端。任一构建失败后不要继续sha256/上传旧文件。

手机每轮按以下顺序执行，具体命令必须使用本轮确认的节点与版本目录：

1. 确认fastboot序列号与镜像SHA，再用 `fastboot -s 9634f4ac boot 完整镜像路径` 临时启动；不要把“fastboot已发送成功”当成系统成功启动。
2. 登录后记录boot_id、uname、cmdline、挂载、电池/温度；本机USB SSH地址通常为172.16.42.1，变化时重新确认。认证交互处理，文档/脚本不新增明文口令。
3. 干净启动按依赖加载：mc → videodev → v4l2-async → v4l2-fwnode → videobuf2-common/memops/dma-sg/v4l2 → 选定CCI → CAMSS → 选定IMX298 → 选定VCM。已加载版本不明则重启，不以“文件已上传”代替“模块已生效”。
4. `scp -O`上传到 `/newroot/tmp/op3-af-实验名-提交号/`，手机比对SHA；旧bundle仅按清单取依赖，不能覆盖新模块。动态工具缺loader时优先上传静态helper，不重建rootfs。
5. 按G1前提验证采集后运行一次当前实验；收集完整日志、返回码、RAW元数据并复制到主机，写结论。
6. 正常关闭会话。CCI故障后不继续同boot的后续实验；不运行 `rmmod qcom_camss`/卸载CCI，不扫总线，不flash/erase，不清空sda15。需要新启动时沿已授权临时boot路径恢复。

## 5. 每阶段必须留下的交接

按 `docs/templates/agent-handoff.md` 全部字段记录：任务/Issue、角色、正式baseline、工作分支、commit、变更文件、当前层、唯一假设/变量、构建执行者与命令/输出目录、构建结果、产物SHA、设备执行者/结果、完整证据路径、SUPPORTED/REJECTED/INCONCLUSIVE、不确定项、下一实验。
分开写 `BUILD_PASS`、`I2C_PASS`、`LENS_MOTION_PASS`、`AF_PASS`，不能互相替代。一次失败之后，下一步必须指向新增证据或一项可解释的修正。
在阶段结束或暂停时更新handoff并本地提交；远端push与正式基线提升遵循用户授权。

## 6. 可直接复制给 5.6luna 的任务提示

以下按 `docs/templates/codex-subtask-prompt.md` 填充，过时的编译限制按现行AGENTS.md修正；使用当前两个已分配分支，无需自动新建分支或任务。

```text
你是 OnePlus 3 项目的 Implementation Agent，接手后摄自动对焦。
主目录 /home/kai/src/oneplus3-mainline；先运行 ./scripts/agent-start.sh，
阅读 AGENTS.md、BASELINE.env、docs/handoff/latest.md、docs/bringup-status.md、
docs/test-matrix.md、docs/decisions.md、docs/build-environment.md、
docs/collaboration-framework.md，以及 docs/op3-autofocus-luna-plan.md。
检查本地Git身份；为空时仅按仓库模板设置local身份，不能改global。

正式baseline：pmOS 6.12.1 / 67b0bbc3cbf46bae712a2606a43361756fcbd829。
项目分支 agent/implementation/recovery-browser-001。
相机内核工作树 source/linux-pmos-msm8996-6.12-camera-imx298，
分支 agent/implementation/op3-camera-imx298-001，审查起点004cda8e0613。
Issue #12为原相机记录，用户已明确追加AF任务；按计划逐阶段记录。
Previous PASS：IMX298=0x0298及旧CCI400镜像单次STREAMON连续两帧RAW采集；
当前AF候选必须重新验证采集前提。镜头移动和闭环AF均未PASS。

先执行G0，只读核对原厂实际地址、初始化与共享电源依赖；当前唯一假设：
VCM简化实现可能遗漏原厂前置条件。唯一变量：补齐协议证据。
PASS：必要字段有可追溯依据；UNKNOWN则记录INCONCLUSIVE，不猜寄存器。
随后按G1先改用户态session helper，建立真实STREAMON并持续DQBUF/QBUF，
再测试同一位置命令；只有采集、电源状态前提成立才能判断供电假设。
每次进入下一阶段，先写明该阶段层、唯一假设、变量、PASS/FAIL再执行。

硬边界：
1. 按已有相机编译/测试授权执行小helper、单模块、必要DTB与临时fastboot boot；
   大型编译遵守现行AGENTS.md，记录commit、命令、输出目录、SHA。
2. 不flash/erase、不换rootfs；最终Buildroot放在成功验证后的G5。
3. 不改其它分支/正式recovery内核/main/bringup；保留既有未跟踪文件。
4. 不混改CCI、DTS、VCM和用户态；每个实验只有一项原因。
5. 6.3.1只作历史证据、7.x不在范围；不盲扫I²C，不强制加载模块，
   不卸载CAMSS/CCI，不无依据重复已revert的queue/EXEC实验。
6. 未证实芯片ACK不算probe PASS，缓存位置不算镜头移动；
   仅open IMX298 subdev不会上电，必须以源码与本轮runtime/帧证据判断。
7. 先提交源代码再编译；每阶段更新并提交handoff；push须有用户授权。

允许交付：协议审查、分阶段最小代码/脚本、命令、日志、照片、局部Git提交。
按docs/templates/agent-handoff.md记录所有字段及NOT_RUN/PASS/FAIL、
SUPPORTED/REJECTED/INCONCLUSIVE。只使用真实执行记录，不把计划写成结果。
一阶段前提不成立就停止依赖它的测试，完成相关诊断并报告具体缺口。
最终成功要求：可重复镜头运动+近远目标单次AF+图像证据+无采集故障；
Integration与用户决定接受。先做G0/G1，绝不要开局重编整个Buildroot。
```

## 审查依据与本次完成记录

本地关键位置：`imx298.c` 的 `imx298_s_stream` / `imx298_internal_ops`；
`bu63165gwl.c` 的 `probe` / `power_on` / `write_position`；
`scripts/op3-v4l2-focus-test.c` 的缓存读回；`scripts/op3-v4l2-stream-test.c` 的STREAMON/DQBUF循环。
Linux文档也要求按寄存器访问和采集管理[传感器运行时电源](https://docs.kernel.org/driver-api/media/camera-sensor.html)，并区分[绝对位置与自动对焦控制](https://docs.kernel.org/userspace-api/media/v4l/ext-ctrls-camera.html)。实现API以本项目6.12源码为准。

- Task / Issue：OP3-AF-PLAN，关联#12；Role：Research/Review。
- Baseline/branch：见第2节；Commit：包含本文的本地文档提交（用 `git log -1 -- docs/op3-autofocus-luna-plan.md` 定位）。
- Changed files：本文、`docs/handoff/latest.md`、`docs/handoff/op3-camera-imx298-001.md`。
- Layer：执行计划/静态审查；Hypothesis：旧“open即保持上电”的测试前提是否成立；Only variable：交接说明。
- Build executor：无；Build result：NOT_RUN。Device executor：无；Device result：NOT_RUN。
- Artifacts：仅重算第2节已有文件和G0离线库的SHA，没有新构建产物。
- Evidence：本文源码定位、旧会话日志摘录、固定版本原厂源码、离线vendor文件校验。
- Conclusion：旧open实验前提REJECTED；AF故障根因INCONCLUSIVE。
- Uncertainties：本机运行时地址、芯片初始化依赖、真实供电波形、实际位置范围、当前候选流稳定性。
- Next experiment：G0核对协议后，G1真实采集会话下对焦对照。

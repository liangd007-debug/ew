# EWhere - 内网穿透 / 端口转发工具

基于 EarthWorm (EW) 第三方重写版 `anhilo/xiaogongju` 的审查、修复与优化版。
原版只发布二进制不开放源码，这个重写版实现了 EW 的全部 6 种工作模式。

> 仅用于合法授权的渗透测试与教学演示。

## 工作模式

| 模式 | 作用 | 位置 |
|------|------|------|
| `ssocksd` | 正向 SOCKS5 代理 | 能直接出网的机器 |
| `rcsocks` | 反向 SOCKS5 代理（控制端） | 公网机器 |
| `rssocks` | 反向 SOCKS5 代理（工作端） | 内网机器 |
| `lcx_listen` | 反向隧道（控制端） | 公网机器 |
| `lcx_tran` | 端口转发 | 本机 |
| `lcx_slave` | 反向隧道（工作端） | 内网机器 |

常见组合：

```bash
# 正向 SOCKS5
./EWhere -s ssocksd -l 1080

# 端口转发：本机1080 -> 目标:80
./EWhere -s lcx_tran -l 1080 -f 192.168.1.10 -g 80

# 反向 SOCKS5：公网机
./EWhere -s rcsocks -l 1080 -e 8888
# 内网机
./EWhere -s rssocks -d <公网IP> -e 8888

# 反向隧道（把内网 22 端口拉到公网 1080）：公网机
./EWhere -s lcx_listen -l 1080 -e 8888
# 内网机
./EWhere -s lcx_slave -d <公网IP> -e 8888 -f 127.0.0.1 -g 22
```

选项：`-l` 监听端口 `-d/-e` 控制端地址/端口 `-f/-g` 转发目标 `-t` 隧道空闲超时（毫秒，默认 10000）`-h` 帮助 `-a` 关于 `-v` 版本。

## 构建

在 Kali（或任何带 gcc + mingw-w64 的 Linux）上：

```bash
make            # 四个目标全编
make linux64    # EWhere_linux_x64
make linux32    # EWhere_linux_x86   需要 gcc-multilib
make win64      # EWhere_win_x64.exe 需要 mingw-w64
make win32      # EWhere_win_x86.exe 需要 mingw-w64
```

产物在 `bin/` 下（本仓库已附带在 Kali 上编好的四份）。

## 中文编码（Win7 控制台不乱码的关键）

同一份 UTF-8 源码，两个平台编译时用不同字符集：

- **Linux 目标**：默认 UTF-8，Kali 终端直接正常显示。
- **Windows 目标**：`-finput-charset=UTF-8 -fexec-charset=GBK`，字符串在二进制里是 GBK 字节；
  程序启动时再 `SetConsoleOutputCP(936)` 强制控制台代码页。

这样 Win7/中文 cmd（cp936）显示中文完全正常，不依赖终端 chcp 设置。
另外 Windows 下 stdout 用 `_IONBF` 无缓冲——MinGW 会把 `_IOLBF` 当全缓冲处理，
否则 `EWhere ... > log.txt` 时日志会卡在缓冲里看不到（已实测验证）。

## 相对原重写版的修复

- `MIC_THREAD_CREATE` 失败判断 `!= 0`（原来 `pthread_create` 判 `< 0` 永远不触发）
- 线程参数原来传栈变量地址（`&sockbuf`/`&poolnum`），多连接竞态 → 改为堆分配 + 线程内释放
- 隧道线程的 poolnum 参数泄漏（每连接 4 字节）→ 线程内 `free`
- `tunn_set_second_pool_and_run_it` 的 `num` 来自网络，未校验范围 → 越界写（可远程触发）→ 加边界 + 状态校验
- SOCKS5 协商原来一次 `recv(262)` 会吞掉后续请求字节 → 按 RFC 分步精确读取
- 所有 `recv` 改为 `API_socket_recv_full` 精确读满，杜绝 TCP 短读导致的握手失败
- 控制协议确认判断 `&&` 写错为 `||` 语义（上线确认分支）→ 修正
- `rcsocktul.poolnum` 原为 `char`（>127 溢出）→ 改 `int`
- 反向端重连用递归（每断一次多一层栈/线程）→ 改循环重连
- DNS 解析失败原直接解引用空指针崩溃 → 加错误检查
- 服务端 socket 缺 `SO_REUSEADDR`，重启绑定失败 → 加上（已实测重启秒绑）
- 连接目标改为 5 秒超时非阻塞 connect，目标不可达不再卡死工作线程
- 控制通道 `cmd_socket` 加互斥锁；隧道池加锁，取代原来的忙等假锁
- 池满时的处理：丢弃连接而不是死等；`tunn_close` 的 `> 0` 判断改为 `>= 0`
- `-t` 空闲超时原来按"轮询次数"估算（标称 2000 实际 4.7 秒）→ 按真实时间计算（实测 2.00 秒）
- 删除死代码：`docs/` 重构笔记、`-q` 彩蛋、注释掉的旧实现、未用变量

## 验证

Kali 上 `bash test/ew_test.sh`，9 项全通过：

```
ssockd 正向 SOCKS5 / lcx_tran 转发 / lcx_listen+rssocks 反向 SOCKS5 /
lcx_listen+lcx_slave 反向转发 / 三级级联 / SOCKS5 域名解析 /
重启后端口立即重绑 / -t 2000 空闲 2.00 秒准确断开
```

Windows 侧在本机实测：中文帮助 GBK 解码正常、重定向日志实时落盘、
`EWhere_win_x64.exe` 与 `EWhere_win_x86.exe` 走 SOCKS5 访问均返回 200。

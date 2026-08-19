# PWTool

基于 ProjectWise (Bentley) SDK 的工程文档协同工具，用于打开 / 链接 / 上传 PW 上的工程模型文件。
使用 C++ / MFC，Visual Studio 2017 (v141 工具集)。

## 功能

- **登录 / 退出**：惰性登录（用到时再弹 PW 登录框），窗口标题实时显示当前账号
- **打开**：从 PW 目录下载模型到本地 model 目录并打开，可选历史版本
- **链接**：多选下载到本地目录，登记链接目录，后续可一键更新到所选版本
- **链接管理**：查看每个链接模型的最新版本、选择历史版本并更新
- **上传**：更新已有文档（自动生成新版本）或首次上传新建文档；支持多人上传同名文件（自动续传新版本）

## 工程结构

```
PWTool/                    # 仓库根
├── PWTool.sln             # 解决方案
├── PWTool/                # 工程源码
│   ├── PWTool.vcxproj     # 工程文件
│   ├── PWTool.cpp/h       # 应用入口（CWinApp）
│   ├── PWToolDlg.cpp/h    # 主对话框：登录/打开/链接/链接管理/上传入口
│   ├── PWHelper.cpp/h     # PW 协同公共封装（登录/下载/上传/版本/INI/日志）
│   ├── DocListDlg.cpp/h   # 文档列表对话框（打开/链接共用）
│   ├── LinkMgrDlg.cpp/h   # 链接管理对话框
│   ├── VersionListDlg.cpp/h # 版本选择对话框
│   ├── PWTool.rc          # 资源脚本
│   ├── resource.h / pch.h
│   └── res/               # 图标等资源
├── dist/                  # 打包输出（x86/x64 重命名 exe + 说明）
└── 参考资料/              # 本地归档（SDK 参考，不入库）
```

## 编译环境

- Visual Studio 2017（或 VS2022 安装 v141 工具集），MFC
- Windows SDK 10.0.17763.0
- ProjectWise SDK：`D:\SDK\ProjectWise100003262en`（include / lib 路径已在 vcxproj 中配置）
- PW 相关 DLL（`dmawin.dll` 等）为延迟加载，运行前程序会自动定位 PW 客户端 bin 目录并加入搜索路径

详细编译运行说明见 `参考资料/20260728/PWExampleFroTSZ-VS2017-编译运行说明.docx`（该目录仅本地保留，不入库）。

## 编译打包

```bat
# Release x64 / x86（dist 同时发布两个平台）
MSBuild PWTool.sln /p:Configuration=Release /p:Platform=x64
MSBuild PWTool.sln /p:Configuration=Release /p:Platform=x86
```

- 产物：`x64\Release\PWTool.exe`、`Release\PWTool.exe`
- 打包输出到 `dist\`：两个重命名 exe（`-x64` / `-x86`）+ `check_pw_runtime.bat` + `使用说明.txt`
- 程序在**装有 PW 客户端的机器**上运行（开发机无需装 PW 客户端）

## 代码规范

- 编码 UTF-8-BOM，缩进 **4 空格**（禁用 tab），由根目录 `.editorconfig` 统一
- 对话框与核心封装均放在 `PWHelper` 命名空间；对话框只做 UI 与流程，PW API 调用集中在 `PWHelper.cpp`
- 日志统一走 `PWHelper::AppendLog()`，各模块用不同日志文件 / tag 区分：
  - 下载/链接：`exe\pw_link.log`（下载带 `[下载]` tag）
  - 链接更新：`LinkModel\pw_update.log`
- 最近使用的本地目录等配置存 `%APPDATA%\PWTool\Settings.ini`（exe 可能部署在不可写目录）
- PW 地址来源（本地文件 ↔ 服务器文档的对应关系）记录在各目录的 `PWAddress.ini`

## 数据源已知限制（CISDI-PW）

这些限制直接影响功能实现，已在代码注释中标注，整理如下：

- **版本集枚举 API 只返回活动版本**：枚举全部版本改用"按文件名匹配所有文档行"（`EnumSameNameDocuments`）
- **直接 API 检入不自动建版本**：上传必须走官方检入对话框并强制生成新版本
- **检入框建版本后内容可能是旧的**：需再用直接 API 把最新版本内容改写为本地文件
- **建版本时创建人字段轮转错位**：版本列表"上传人"列取修改人字段（`DOC_PROP_UPDATERID`），该字段可靠
- **同一文档两个版本拷到同一目录会报"位置占用"**：下载/上传均使用唯一临时子目录绕开

## 已知限制

- **单线程 UI**：所有 PW 操作（下载重试等待、上传）均为同步调用，操作期间界面会阻塞
- 打开/链接下载时若本地同名文件被 CAD 占用，会提示先关闭文件后重试

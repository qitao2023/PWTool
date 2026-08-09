# PWTool

基于 ProjectWise (Bentley) SDK 的工程文档协同工具示例，使用 C++ / MFC (Visual Studio 2017, v141 工具集)。

## 工程结构

```
PWExampleFroTSZ-VS2017/
├── PWExampleFroTSZ-VS2017.sln   # 解决方案文件
└── PWExampleFroTSZ-VS2017/      # 工程源码
    ├── PWExampleFroTSZ-VS2017Dlg.cpp/h  # 主对话框
    ├── PWHelper.cpp/h                   # ProjectWise 辅助封装
    ├── DocListDlg.cpp/h                 # 文档列表对话框
    ├── LinkMgrDlg.cpp/h                 # 链接管理对话框
    ├── UploadDlg.cpp/h                  # 上传对话框
    └── res/                             # 资源文件
```

## 编译环境

- Visual Studio 2017 (v141 工具集)
- Windows SDK 10.0.17763.0
- ProjectWise SDK：`D:\SDK\ProjectWise100003262en`
- 需要把 ProjectWise 的 bin 目录添加到环境变量 PATH 中

详见 `参考资料/20260728/PWExampleFroTSZ-VS2017-编译运行说明.docx`。

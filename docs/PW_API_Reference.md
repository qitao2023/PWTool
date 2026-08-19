# ProjectWise API 速查参考

> 本文档列出 PWTool 中实际调用的 ProjectWise SDK 函数（`aaApi_*`），供开发者查阅。
> 详细文档参见 PW SDK 头文件（`dmsapi.h` 等）及 SDK samples。

---

## 目录

- [初始化 / 登录](#初始化--登录)
- [用户属性](#用户属性)
- [项目/目录](#项目目录)
- [文档查询](#文档查询)
- [文档操作](#文档操作)
- [用户设置](#用户设置)
- [错误处理](#错误处理)
- [常用属性常量](#常用属性常量)

---

## 初始化 / 登录

| 函数 | 用途 | 关键参数 / 返回值 |
|------|------|-------------------|
| `aaApi_Initialize(module)` | 初始化 PW API 运行环境 | `AAMODULE_EXPLORER`；必须在任何其他 `aaApi_*` 调用前执行 |
| `aaApi_LoginDlgExt(hWnd, title, flags, dbSrc, dbSrcSize, ...)` | 弹出登录对话框 | `AALOGIN_SILENT` 跳过已有连接；返回 `IDOK` = 成功 |
| `aaApi_GetCurrentUserId()` | 返回当前登录用户 ID | 0 = 未登录 |
| `aaApi_GetActiveDatasource()` | 返回活动数据源句柄 | `HDSOURCE`；0 = 未连接 |
| `aaApi_GetActiveDatasourceName(buf, size)` | 把数据源名称写入 buf | — |
| `aaApi_LogoutByHandle(hDs)` | 断开指定数据源连接 | 用于退出登录、切换账号 |

---

## 用户属性

| 函数 | 用途 | 关键参数 / 返回值 |
|------|------|-------------------|
| `aaApi_SelectUser(userId)` | 按 ID 加载用户记录到内部缓冲区 | 返回 1 = 成功；之后用 `GetUser*Property` 取属性 |
| `aaApi_GetUserStringProperty(propId, idx)` | 取用户字符串属性 | `USER_PROP_NAME` = 登录账号；idx 通常为 0 |

---

## 项目/目录

| 函数 | 用途 | 关键参数 / 返回值 |
|------|------|-------------------|
| `aaApi_SelectProject(projectId)` | 按 ID 加载项目记录到缓冲区 | 返回 >0 = 成功；之后用 `GetProject*Property` 取属性 |
| `aaApi_SelectProjectDlg(hWnd, title, flags)` | 弹出项目选择对话框 | 返回用户选定的项目 ID；0 = 取消 |
| `aaApi_GetProjectStringProperty(propId, idx)` | 取项目字符串属性 | `PROJ_PROP_NAME` = 项目名 |
| `aaApi_GetProjectNumericProperty(propId, idx)` | 取项目数值属性 | `PROJ_PROP_STORAGEID` / `PROJ_PROP_WSPACEPROFID` |

---

## 文档查询

| 函数 | 用途 | 关键参数 / 返回值 |
|------|------|-------------------|
| `aaApi_SelectDocument(projectId, docId)` | 加载单条文档记录 | 返回 1 = 成功；之后用 `GetDocument*Property` 取属性 |
| `aaApi_SelectDocumentsByProjectId(projectId)` | 加载项目内**所有**文档记录 | 返回记录数；遍历时 idx = 0..count-1 |
| `aaApi_GetDocumentStringProperty(propId, idx)` | 取文档字符串属性 | 见 [常用属性常量](#常用属性常量) |
| `aaApi_GetDocumentNumericProperty(propId, idx)` | 取文档数值属性 | 见 [常用属性常量](#常用属性常量) |
| `aaApi_GetDocumentAccess(projectId, docId, idx)` | 取文档访问权限 | `AADMS_ACCESS_READ` / `AADMS_ACCESS_WRITE`；idx < 0 时内部自动 select |

---

## 文档操作

### 下载

| 函数 | 用途 | 关键参数 / 返回值 |
|------|------|-------------------|
| `aaApi_FetchDocumentFromServer(flags, prjId, docId, dir, outBuf, bufSize)` | 从服务器强制下载 | flags 组合见下方；成功时 outBuf 回填文件路径 |
| `aaApi_CopyOutDocument(prjId, docId, dir, outBuf, bufSize)` | 拷出文档到本地 | 用于历史版本；需 `AADMS_PAR_DIS_SHOW_VERSION=1` |

**FetchDocumentFromServer 常用 flags：**
```
AADMS_DOCFETCH_COPYOUT                  // 拷出到本地目录
AADMS_DOCFETCH_IGNORE_UPTODATECOPY      // 绕过本地缓存，强制重新下载
AADMS_DOCFETCH_MASTER_AS_SET            // 主文件作为集合
AADMS_DOCFETCH_NESTED_REFERENCES        // 包含嵌套参照
AADMS_DOCFETCH_REDLINED_REFERENCES      // 包含红线参照
```

### 检出 / 检入

| 函数 | 用途 | 关键参数 / 返回值 |
|------|------|-------------------|
| `aaApi_CheckOutDocument(prjId, docId, dir, outBuf, bufSize)` | 检出文档（锁定） | 下载工作副本到 dir；返回 TRUE = 成功 |
| `aaApi_CheckInDocument(prjId, docId)` | 检入文档（解锁） | 提交本地修改到服务器 |
| `aaApi_DisplayDocumentCheckInActionDlg2(hWnd, title, flags, pItems, count, comment, callback, param)` | 弹出官方检入对话框 | 走工作流路径；可生成新版本 |

### 创建

| 函数 | 用途 | 关键参数 / 返回值 |
|------|------|-------------------|
| `aaApi_CreateDocument(pDocId, prjId, storageId, fileType, itemType, appId, deptId, wspProfId, localFile, fileName, name, comment, version, bLeaveOut, flags, workBuf, workBufSize, ...)` | 创建新文档并上传 | 首次上传使用；`version=NULL` 系统自动生成；`bLeaveOut=FALSE` 上传后检入 |

---

## 用户设置

| 函数 | 用途 | 关键参数 / 返回值 |
|------|------|-------------------|
| `aaApi_GetUserNumericSetting(paramId)` | 读取用户级数值设置 | `AADMS_PAR_DIS_SHOW_VERSION`：0=只显示当前版本，1=显示所有版本 |
| `aaApi_SetUserNumericSetting(paramId, value)` | 写入用户级数值设置 | 仅客户端本地生效，无需管理员权限 |

---

## 错误处理

| 函数 | 用途 | 关键参数 / 返回值 |
|------|------|-------------------|
| `aaApi_GetLastErrorMessage()` | 返回最近错误的文本描述 | `LPCWSTR`；空串表示无错误 |
| `aaApi_GetLastErrorId()` | 返回最近错误的数字码 | `LONG`；0 = 无错误 |

---

## 常用属性常量

### 文档字符串属性（`DOC_PROP_*`）

| 常量 | 含义 |
|------|------|
| `DOC_PROP_NAME` | 文档显示名 |
| `DOC_PROP_FILENAME` | 文件名（含扩展名） |
| `DOC_PROP_VERSION` | 版本串（A/B/C/.../Z/AA） |
| `DOC_PROP_FILE_UPDATE_TIME` | 文件内容最后更新时间 |

### 文档数值属性（`DOC_PROP_*`）

| 常量 | 含义 |
|------|------|
| `DOC_PROP_ID` | 文档 ID（每个版本独立 docid） |
| `DOC_PROP_VERSIONNO` | 版本序号（越大越新） |
| `DOC_PROP_SIZE` | 文件大小（字节） |
| `DOC_PROP_ORIGINALNO` | 活动版本=0；历史版本=活动版本的 docid |
| `DOC_PROP_CREATORID` | 创建人用户 ID |
| `DOC_PROP_UPDATERID` | 修改人（上传人）用户 ID |

### 访问权限（`AADMS_ACCESS_*`）

| 常量 | 含义 |
|------|------|
| `AADMS_ACCESS_READ` | 只读 |
| `AADMS_ACCESS_WRITE` | 可修改 / 可检入 |

---

## 版本机制说明

ProjectWise 中**每个版本是独立的 docid**。创建新版本时：
- 新版本获得新 docid
- 旧版本 docid 仍然存在，但其 `DOC_PROP_ORIGINALNO` 会指向新版本的 docid
- 活动（当前）版本的 `ORIGINALNO = 0`

因此"判断某 docid 是否为最新版本"的正确方式是：
```cpp
LONG lOriginalNo = aaApi_GetDocumentNumericProperty(DOC_PROP_ORIGINALNO, 0);
BOOL bIsActive = (lOriginalNo == 0);  // TRUE = 当前版本
```

> ⚠️ **本数据源限制**：`aaApi_SelectDocumentDataBufferVersions`（版本集枚举 API）在 CISDI-PW 数据源只返回活动版本，
> 需改用 `aaApi_SelectDocumentsByProjectId` + 按文件名匹配来枚举全部版本。

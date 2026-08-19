#pragma once

// PWHelper.h: PW 协同公共功能封装
//
// 封装 ProjectWise API 的登录、下载、上传、版本查询等操作，
// 以及 PW 地址来源的本地 INI 存取，供各对话框复用。

namespace PWHelper
{
    // PW 地址来源信息（对应 model\PWAddress.ini / LinkModel\PWAddress.ini 中的一个节）
    struct PWAddrInfo
    {
        CString strDatasource;    // 数据源名
        LONG    lProjectId;       // PW 目录(项目) ID
        CString strProjectName;   // 目录名
        LONG    lDocumentId;      // 文档 ID
        CString strVersion;       // 下载时 PW 版本号（如 A/B/C）
        CString strVersionDate;   // 下载时 PW 版本更新时间
        CString strComment;       // 最近一次上传版本说明
        BOOL    bLink;            // 链接状态 1=已链接 0=未链接
        CString strLocalPath;     // 链接/下载时用户实际选择的本地完整路径

        PWAddrInfo()
            : lProjectId(0), lDocumentId(0), bLink(FALSE)
        {
        }
    };

    // 项目文档列表行数据（DocListDlg 使用）
    struct PWDocItem
    {
        LONG    lProjectId;
        LONG    lDocumentId;
        CString strName;
        CString strFileName;
        CString strVersion;
        CString strUpdateTime;
        LONG    lAccess;          // AADMS_ACCESS_*
        LONG    lChosenDocId;     // 用户经"历史版本"列选定的版本docid；0=未选(打开/链接时用最新)
        LONG    lOriginalNo;      // DOC_PROP_ORIGINALNO：活动版本=0，历史版本=活动版本docid
        LONG    lVersionCount;    // 同名文档(版本)数量，用于"历史版本"列显示"N个版本"

        PWDocItem()
            : lProjectId(0), lDocumentId(0), lAccess(0), lChosenDocId(0), lOriginalNo(0), lVersionCount(0)
        {
        }
    };

    // 文档版本信息（VersionListDlg 使用）
    struct PWDocVersionItem
    {
        LONG    lDocumentId;      // 该版本的 docid
        LONG    lVersionNo;       // 版本序号（越大越新）
        CString strVersion;       // 版本串
        CString strUpdateTime;    // 文件更新时间 FILE_UPDATE_TIME
        LONG    lSize;            // 文件大小（字节）DOC_PROP_SIZE
        LONG    lOriginalNo;      // DOC_PROP_ORIGINALNO：活动版本=0，历史版本=活动版本docid
        LONG    lCreatorId;       // 创建人 DOC_PROP_CREATORID（数据源建版本可能从上一版本克隆继承）
        LONG    lUpdaterId;       // 修改人 DOC_PROP_UPDATERID（检入该版本的用户）
        CString strCreatorName;   // 创建人用户名（解析后的登录账号）
        CString strUpdaterName;   // 修改人(上传人)用户名

        PWDocVersionItem()
            : lDocumentId(0), lVersionNo(0), lSize(0), lOriginalNo(0),
              lCreatorId(0), lUpdaterId(0)
        {
        }
    };

    // ---- 登录 / 数据源 ----
    BOOL    IsLoggedIn();                                 // aaApi_GetCurrentUserId()!=0
    BOOL    EnsureLogin(CWnd* pParent);                   // 未登录则弹登录框，返回是否成功
    BOOL    Logout();                                     // 退出登录（断开活动数据源，便于切换账号）
    CString GetCurrentUserName();                         // 当前登录账号(USER_PROP_NAME)；未登录返回空
    CString GetUserNameById(LONG lUserId);                // 由用户ID取登录账号(USER_PROP_NAME)；失败返回空
    CString GetDatasourceName();                          // aaApi_GetActiveDatasourceName

    // ---- DLL 搜索路径（仅Win32，不得调用任何 aaApi_*，供延迟加载前使用）----
    BOOL    EnsurePwDllSearchPath();                      // 定位PW客户端bin目录并加入进程搜索路径

    // ---- 文件系统 ----
    BOOL    CreateDirRecursive(LPCTSTR pszPath);          // 逐级创建目录
    CString GetAppBaseDir();                              // exe 所在目录
    CString GetModelDir();                                // base\model
    CString GetLinkModelDir();                            // base\LinkModel

    // ---- INI（PW 地址来源）----
    CString GetAddrIniPath(LPCTSTR pszFolder);            // 返回 folder\PWAddress.ini
    BOOL    SavePwAddr(LPCTSTR pszFolder, LPCTSTR pszLocalFile, const PWAddrInfo& info); // 记录 localPath=文件夹+文件名
    // 便捷版：查询数据源名/项目名/版本号/更新时间并写入 INI，省去调用方重复拼装 PWAddrInfo。
    // pszLocalPath 非空时作为记录的本地路径，否则默认 folder\文件名。
    void    SavePwAddrOfDocument(LPCTSTR pszFolder, LPCTSTR pszLocalFile,
                                 LONG lProjectId, LONG lDocumentId, BOOL bLink,
                                 const CString& strComment = _T(""),
                                 LPCTSTR pszLocalPath = NULL);
    BOOL    LoadPwAddr(LPCTSTR pszFolder, LPCTSTR pszLocalFile, PWAddrInfo& info);
    BOOL    SetPwAddrLinkState(LPCTSTR pszFolder, LPCTSTR pszLocalFile, BOOL bLink);
    BOOL    DeletePwAddr(LPCTSTR pszFolder, LPCTSTR pszLocalFile);
    BOOL    EnumPwAddrSections(LPCTSTR pszFolder, CStringArray& arrFiles); // 返回 INI 中的节名(文件名)

    // ---- 链接目录注册表（exe 目录\LinkFolders.ini）----
    // 链接时用户可选择任意本地目录，注册后链接管理才会扫描该目录。
    BOOL    RegisterLinkFolder(LPCTSTR pszFolder);       // 登记一个链接目录（去重）
    void    EnumLinkFolders(CStringArray& arrFolders);   // 返回所有链接目录（始终含默认 LinkModel）

    // ---- 最近使用参数（%APPDATA%\PWTool\Settings.ini）----
    // 记录用户上次修改过的参数（如本地下载目录），下次打开自动带出，无需重复填写。
    // 配置存用户目录而非 exe 目录：exe 可能部署在 Program Files 下不可写。
    enum LocalPathMode { LOCALPATH_OPEN, LOCALPATH_LINK };     // 本地下载目录类型（打开/链接分开记忆）
    CString GetSettingsIniPath();                              // %APPDATA%\PWTool\Settings.ini
    CString GetLastLocalPath(LocalPathMode mode);              // 上次使用的本地下载目录；无记录返回空
    void    SetLastLocalPath(LPCTSTR pszPath, LocalPathMode mode); // 记录上次使用的本地下载目录

    // ---- PW 操作 ----
    // 由文档的任一版本 docid 解析出该文档当前(最新)版本的 docid。
    // [修复] ProjectWise 每个版本是独立的 docid，服务器上检入新版本后旧 docid 仍指向旧版本；
    // 通过 DOC_PROP_ORIGINALNO 关联：当前版本该值=0，历史版本该值=当前版本的 docid。
    LONG    GetLatestDocumentId(LONG lProjectId, LONG lDocumentId);
    // 比较版本串（A<B<...<Z<AA）：返回 a>b ? 1 : (a<b ? -1 : 0)
    int     CompareVersionStrings(LPCTSTR a, LPCTSTR b);
    // 按文件名列出项目内所有同名文档（每个同名文档视为一个"版本"）。
    // [数据源] 版本集枚举 API(SelectDocumentDataBufferVersions) 在本数据源只返回活动版本，
    // 但 SelectDocumentsByProjectId 能返回全部同名文档——用后者枚举版本。
    LONG    EnumSameNameDocuments(LONG lProjectId, LONG lDocumentId,
                                  CArray<PWDocVersionItem, PWDocVersionItem&>& arrVersions);
    // 在项目内按文件名查找已存在文档，返回其最新(活动)版本 docid；不存在返回 0。
    LONG    FindDocumentIdByName(LONG lProjectId, LPCTSTR pszFileName);
    // 诊断工具（排查"上传人/创建人错位"时手动调用，正常流程不调用）：
    // 把指定文档各版本的原始字段（docid/版本串/版本号/originalno/创建人ID/修改人ID）写到日志文件。
    void    DumpVersionItems(const CArray<PWDocVersionItem, PWDocVersionItem&>& arrVersions,
                             LPCTSTR pszLogPath);
    // 诊断工具：枚举指定文档全部同名版本后 DumpVersionItems 到 exe 目录 pw_version_dump.txt。
    void    DumpDocumentVersionsToFile(LONG lProjectId, LONG lDocumentId);
    // 下载指定 docid 指向的版本到工作目录（不改写为最新版本，调用方自行决定用哪个版本）。
    // [修复1] 目标目录已有同名文件时直接 CopyOut 会失败或生成带序号的新文件（重新链接/覆盖
    // 下载被误判失败）。现先下载到临时子目录再覆盖到工作目录，保证同名文件被真正更新。
    // [修复2] 当前版本：FetchDocumentFromServer + AADMS_DOCFETCH_IGNORE_UPTODATECOPY 强制从
    // 服务器重新下载，避免复用本地缓存里旧内容（本数据源新版本会重分配 docid，缓存易判错）。
    // [修复3] 历史版本：SDK 在 AADMS_PAR_DIS_SHOW_VERSION=0 时拒绝拷出非当前版本，临时置 1
    // 再恢复，并用 CopyOutDocument 取数，保证"链接回前一个版本"也能下载。
    // [修复4] 每次用唯一临时子目录（__pwdl_<docid>_<序号>）：同一文档先拷出活动版本后，再拷
    // 其他版本到同一目录会报 AAERR_DMS_ERR_CO_LOCATION_IS_USED(58218，"拷出位置已被占用")。
    // 成功返回 TRUE 且 strOutFile 为最终落盘路径；失败返回 FALSE，pstrErr 非空时给出原因。
    BOOL    DownloadDocument(LONG lProjectId, LONG lDocumentId, LPCTSTR pszWorkDir,
                             CString& strOutFile, CString* pstrErr = NULL); // CopyOutDocument
    // 下载指定版本并覆盖本地链接文件。下载直接落到目标文件所在目录；若 CopyOut 因同名文件
    // 生成了带序号的新文件，则把它覆盖/移动到链接文件上。成功返回 TRUE，失败时 strErr 给出原因；
    // pstrOutFile 非空时回传 CopyOut 实际写入的文件路径（便于诊断）。
    BOOL    DownloadAndReplace(LONG lProjectId, LONG lDocumentId, LPCTSTR pszTargetFile,
                               CString& strErr, CString* pstrOutFile = NULL);
    CString GetVersionDate(LONG lProjectId, LONG lDocumentId);            // 读取指定 docid 对应版本的时间 FILE_UPDATE_TIME（不解析最新）
    CString GetVersion(LONG lProjectId, LONG lDocumentId);                // 读取指定 docid 对应版本号 DOC_PROP_VERSION（不解析最新）
    CString GetLatestVersionDate(LONG lProjectId, LONG lDocumentId);      // 解析最新版本 + FILE_UPDATE_TIME
    CString GetLatestVersion(LONG lProjectId, LONG lDocumentId);          // 解析最新版本 + DOC_PROP_VERSION
    LONG    GetDocumentAccess(LONG lProjectId, LONG lDocumentId, LONG lIndex = -1); // aaApi_GetDocumentAccess（lIndex>=0时用当前buffer，不重新select）
    // 上传新版本：CheckOut->覆盖工作副本->弹出官方检入对话框（走工作流路径，可生成新版本）。
    // [数据源] Rules Engine 控制版本时，直接 API 检入不建版本；官方检入对话框可让用户
    // 确认"生成新版本"，版本号留空系统自动分配。返回是否成功；pOutNewVersion 非空回传是否生成了新版本。
    // [注] 该数据源 API 检入框建版本时创建人固定轮转错位（无法从 App 端规避），
    // "上传人"(DOC_PROP_UPDATERID)每版记录正确，以版本列表"上传人"列为准。
    // [修复] 检入框解析工作文件用"用户配置的工作目录"而非临时目录，故检出目标直接用配置的
    // 工作目录并覆盖为本地最新内容，否则检入框检入工作区里的陈旧副本（版本号增但内容不是最新）。
    BOOL    UploadNewVersion(HWND hWndParent, LONG lProjectId, LONG lDocumentId,
                             LPCTSTR pszLocalFile, LPCTSTR pszWorkDir,
                             const CString& strComment, CString& strErr,
                             BOOL* pOutNewVersion = NULL);
    BOOL    CreateNewDocument(LONG lProjectId, LPCTSTR pszLocalFile, const CString& strComment,
                              LONG& lDocId, CString& strErr);             // aaApi_CreateDocument

    // ---- 日志 ----
    // 追加一行带时间戳的日志（UTF-8）。pszTag 非空时输出 "[时间] [tag] 内容"。
    // 各功能模块（下载/链接/更新）以不同日志文件 + tag 区分。
    void    AppendLog(LPCTSTR pszLogFile, LPCTSTR pszTag, LPCTSTR pszLine);

    // ---- 通用 ----
    CString GetLastErrorText();                           // 包装 aaApi_GetLastErrorMessage()
    BOOL    OpenWithShell(LPCTSTR pszFile);               // ShellExecute 打开文件
    CString GetFileName(LPCTSTR pszPath);                 // 取路径最后一段
    CString GetFileFolder(LPCTSTR pszPath);               // 取路径所在目录
}

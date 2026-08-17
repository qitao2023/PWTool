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

        PWDocItem()
            : lProjectId(0), lDocumentId(0), lAccess(0), lChosenDocId(0), lOriginalNo(0)
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

        PWDocVersionItem()
            : lDocumentId(0), lVersionNo(0), lSize(0), lOriginalNo(0)
        {
        }
    };

    // ---- 登录 / 数据源 ----
    BOOL    IsLoggedIn();                                 // aaApi_GetCurrentUserId()!=0
    BOOL    EnsureLogin(CWnd* pParent);                   // 未登录则弹登录框，返回是否成功
    BOOL    Logout();                                     // 退出登录（断开活动数据源，便于切换账号）
    CString GetCurrentUserName();                         // 当前登录账号(USER_PROP_NAME)；未登录返回空
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
    BOOL    LoadPwAddr(LPCTSTR pszFolder, LPCTSTR pszLocalFile, PWAddrInfo& info);
    BOOL    SetPwAddrLinkState(LPCTSTR pszFolder, LPCTSTR pszLocalFile, BOOL bLink);
    BOOL    DeletePwAddr(LPCTSTR pszFolder, LPCTSTR pszLocalFile);
    BOOL    EnumPwAddrSections(LPCTSTR pszFolder, CStringArray& arrFiles); // 返回 INI 中的节名(文件名)

    // ---- 链接目录注册表（exe 目录\LinkFolders.ini）----
    // 链接时用户可选择任意本地目录，注册后链接管理才会扫描该目录。
    BOOL    RegisterLinkFolder(LPCTSTR pszFolder);       // 登记一个链接目录（去重）
    void    EnumLinkFolders(CStringArray& arrFolders);   // 返回所有链接目录（始终含默认 LinkModel）

    // ---- PW 操作 ----
    // 由文档的任一版本 docid 解析出该文档当前(最新)版本的 docid。
    // [修复] ProjectWise 每个版本是独立的 docid，服务器上检入新版本后旧 docid 仍指向旧版本；
    // 通过 DOC_PROP_ORIGINALNO 关联：当前版本该值=0，历史版本该值=当前版本的 docid。
    LONG    GetLatestDocumentId(LONG lProjectId, LONG lDocumentId);
    // 枚举文档的所有版本（按版本号从新到旧排序），返回版本数量；失败返回<=0。
    LONG    EnumDocumentVersions(LONG lProjectId, LONG lDocumentId,
                                 CArray<PWDocVersionItem, PWDocVersionItem&>& arrVersions);
    // 比较版本串（A<B<...<Z<AA）：返回 a>b ? 1 : (a<b ? -1 : 0)
    int     CompareVersionStrings(LPCTSTR a, LPCTSTR b);
    // 按文件名列出项目内所有同名文档（每个同名文档视为一个"版本"）。
    // [数据源] 版本集枚举 API(SelectDocumentDataBufferVersions) 在本数据源只返回活动版本，
    // 但 SelectDocumentsByProjectId 能返回全部同名文档——用后者枚举版本。
    LONG    EnumSameNameDocuments(LONG lProjectId, LONG lDocumentId,
                                  CArray<PWDocVersionItem, PWDocVersionItem&>& arrVersions);
    // 下载指定 docid 指向的版本到工作目录（不改写为最新版本，调用方自行决定用哪个版本）。
    BOOL    DownloadDocument(LONG lProjectId, LONG lDocumentId, LPCTSTR pszWorkDir, CString& strOutFile); // CopyOutDocument
    // 下载指定版本并覆盖本地链接文件。下载直接落到目标文件所在目录；若 CopyOut 因同名文件
    // 生成了带序号的新文件，则把它覆盖/移动到链接文件上。成功返回 TRUE，失败时 strErr 给出原因；
    // pstrOutFile 非空时回传 CopyOut 实际写入的文件路径（便于诊断）。
    BOOL    DownloadAndReplace(LONG lProjectId, LONG lDocumentId, LPCTSTR pszTargetFile,
                               CString& strErr, CString* pstrOutFile = NULL);
    CString GetVersionDate(LONG lProjectId, LONG lDocumentId);            // 读取指定 docid 对应版本的时间 FILE_UPDATE_TIME（不解析最新）
    CString GetLatestVersionDate(LONG lProjectId, LONG lDocumentId);      // 解析最新版本 + FILE_UPDATE_TIME
    CString GetLatestVersion(LONG lProjectId, LONG lDocumentId);          // 解析最新版本 + DOC_PROP_VERSION
    LONG    GetDocumentAccess(LONG lProjectId, LONG lDocumentId, LONG lIndex = -1); // aaApi_GetDocumentAccess（lIndex>=0时用当前buffer，不重新select）
    // 上传新版本：CheckOut->覆盖工作副本->弹出官方检入对话框（走工作流路径，可生成新版本）。
    // [数据源] Rules Engine 控制版本时，直接 API 检入不建版本；官方检入对话框可让用户
    // 确认"生成新版本"，版本号留空系统自动分配。返回是否成功；pOutNewVersion 非空回传是否生成了新版本。
    // [修复] 检入框解析工作文件用"用户配置的工作目录"而非临时目录，故检出目标直接用配置的
    // 工作目录并覆盖为本地最新内容，否则检入框检入工作区里的陈旧副本（版本号增但内容不是最新）。
    BOOL    UploadNewVersion(HWND hWndParent, LONG lProjectId, LONG lDocumentId,
                             LPCTSTR pszLocalFile, LPCTSTR pszWorkDir,
                             const CString& strComment, CString& strErr,
                             BOOL* pOutNewVersion = NULL);
    BOOL    CreateNewDocument(LONG lProjectId, LPCTSTR pszLocalFile, const CString& strComment,
                              LONG& lDocId, CString& strErr);             // aaApi_CreateDocument

    // ---- 通用 ----
    CString GetLastErrorText();                           // 包装 aaApi_GetLastErrorMessage()
    BOOL    OpenWithShell(LPCTSTR pszFile);               // ShellExecute 打开文件
    CString GetFileName(LPCTSTR pszPath);                 // 取路径最后一段
    CString GetFileFolder(LPCTSTR pszPath);               // 取路径所在目录
}

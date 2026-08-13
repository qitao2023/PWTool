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

        PWDocItem()
            : lProjectId(0), lDocumentId(0), lAccess(0)
        {
        }
    };

    // ---- 登录 / 数据源 ----
    BOOL    IsLoggedIn();                                 // aaApi_GetCurrentUserId()!=0
    BOOL    EnsureLogin(CWnd* pParent);                   // 未登录则弹登录框，返回是否成功
    BOOL    Logout();                                     // 退出登录（断开活动数据源，便于切换账号）
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
    BOOL    SavePwAddr(LPCTSTR pszFolder, LPCTSTR pszLocalFile, const PWAddrInfo& info);
    BOOL    LoadPwAddr(LPCTSTR pszFolder, LPCTSTR pszLocalFile, PWAddrInfo& info);
    BOOL    SetPwAddrLinkState(LPCTSTR pszFolder, LPCTSTR pszLocalFile, BOOL bLink);
    BOOL    DeletePwAddr(LPCTSTR pszFolder, LPCTSTR pszLocalFile);
    BOOL    EnumPwAddrSections(LPCTSTR pszFolder, CStringArray& arrFiles); // 返回 INI 中的节名(文件名)

    // ---- PW 操作 ----
    BOOL    DownloadDocument(LONG lProjectId, LONG lDocumentId, LPCTSTR pszWorkDir, CString& strOutFile); // CopyOutDocument
    CString GetLatestVersionDate(LONG lProjectId, LONG lDocumentId);      // SelectDocument + FILE_UPDATE_TIME
    CString GetLatestVersion(LONG lProjectId, LONG lDocumentId);          // SelectDocument + DOC_PROP_VERSION（服务器当前版本串）
    LONG    GetDocumentAccess(LONG lProjectId, LONG lDocumentId, LONG lIndex = -1); // aaApi_GetDocumentAccess（lIndex>=0时用当前buffer，不重新select）
    BOOL    UploadNewVersion(LONG lProjectId, LONG lDocumentId, LPCTSTR pszLocalFile,
                             LPCTSTR pszWorkDir, const CString& strComment, CString& strErr); // CheckOut->覆盖->CheckInLeaveCopy
    BOOL    CreateNewDocument(LONG lProjectId, LPCTSTR pszLocalFile, const CString& strComment,
                              LONG& lDocId, CString& strErr);             // aaApi_CreateDocument

    // ---- 通用 ----
    CString GetLastErrorText();                           // 包装 aaApi_GetLastErrorMessage()
    BOOL    OpenWithShell(LPCTSTR pszFile);               // ShellExecute 打开文件
    CString GetFileName(LPCTSTR pszPath);                 // 取路径最后一段
    CString GetFileFolder(LPCTSTR pszPath);               // 取路径所在目录
}

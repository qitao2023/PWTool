// PWHelper.cpp: PW 协同公共功能实现

#include "pch.h"
#include "PWHelper.h"

#include <shellapi.h>
#include <direct.h>
#include <afxtempl.h>

namespace PWHelper
{

//--------------------------------------------------------------------------------------+
// DLL 搜索路径（仅Win32，不调用任何 aaApi_*）
//--------------------------------------------------------------------------------------+

static CString GetEnvVar(LPCTSTR pszName)
{
    // 先取所需缓冲大小（含结尾 '\0'），再用 CString 缓冲读取
    DWORD nLen = GetEnvironmentVariable(pszName, NULL, 0);
    if (nLen == 0)
        return CString();
    CString str;
    GetEnvironmentVariable(pszName, str.GetBuffer(nLen), nLen);
    str.ReleaseBuffer();
    return str;
}

// 定位包含 dmawin.dll 的 PW 客户端 bin 目录
static CString FindPwBinDir()
{
    CStringArray arrCands;

    // exe 所在目录
    arrCands.Add(GetAppBaseDir());

    // 常见安装位置（ProgramFiles / ProgramFiles(x86)）
    CString strPF = GetEnvVar(_T("ProgramFiles"));
    if (!strPF.IsEmpty())
    {
        arrCands.Add(strPF + _T("\\Bentley\\ProjectWise"));
        arrCands.Add(strPF + _T("\\Bentley\\ProjectWise\\bin"));
    }
    CString strPF86 = GetEnvVar(_T("ProgramFiles(x86)"));
    if (!strPF86.IsEmpty())
        arrCands.Add(strPF86 + _T("\\Bentley\\ProjectWise"));
    arrCands.Add(_T("D:\\Program Files\\Bentley\\ProjectWise\\bin"));

    // PATH 中的每个目录
    CString strPath = GetEnvVar(_T("PATH"));
    if (!strPath.IsEmpty())
    {
        int nPos = 0;
        while (nPos < strPath.GetLength())
        {
            int nSep = strPath.Find(_T(';'), nPos);
            CString strSeg = (nSep >= 0) ? strPath.Mid(nPos, nSep - nPos) : strPath.Mid(nPos);
            strSeg.Trim();
            if (!strSeg.IsEmpty())
                arrCands.Add(strSeg);
            if (nSep < 0)
                break;
            nPos = nSep + 1;
        }
    }

    // 逐个候选目录检查（含常见子目录）
    static const TCHAR* arrSub[] = { _T(""), _T("\\bin"), _T("\\bin\\x64"), _T("\\Program\\bin"), _T("\\x64\\bin") };
    for (INT_PTR i = 0; i < arrCands.GetSize(); i++)
    {
        CString strDir = arrCands[i];
        strDir.TrimRight(_T('\\'));
        for (size_t j = 0; j < _countof(arrSub); j++)
        {
            CString strTest = strDir + arrSub[j] + _T("\\dmawin.dll");
            if (GetFileAttributes(strTest) != INVALID_FILE_ATTRIBUTES)
                return strDir + arrSub[j];
        }
    }
    return CString();
}

BOOL EnsurePwDllSearchPath()
{
    CString strBin = FindPwBinDir();
    if (strBin.IsEmpty())
        return FALSE;
    return SetDllDirectory(strBin);
}

//--------------------------------------------------------------------------------------+
// 登录 / 数据源
//--------------------------------------------------------------------------------------+

BOOL IsLoggedIn()
{
    return aaApi_GetCurrentUserId() != 0;
}

// 由用户ID取登录账号（USER_PROP_NAME，即登录账号）；ID无效或获取失败返回空串。
// 用于版本枚举时把 DOC_PROP_CREATORID / DOC_PROP_UPDATERID 解析成可读的账号名。
CString GetUserNameById(LONG lUserId)
{
    if (lUserId <= 0)
        return CString();

    if (aaApi_SelectUser(lUserId) != 1)
        return CString();

    const WCHAR* psz = aaApi_GetUserStringProperty(USER_PROP_NAME, 0);
    return (psz != NULL) ? CString(psz) : CString(_T(""));
}

// 当前登录账号：取当前用户记录的用户名（USER_PROP_NAME，即登录账号）。
// 未登录或获取失败时返回空串。
CString GetCurrentUserName()
{
    return GetUserNameById(aaApi_GetCurrentUserId());
}

BOOL EnsureLogin(CWnd* pParent)
{
    if (IsLoggedIn())
        return TRUE;

    HWND hWnd = (pParent != NULL) ? pParent->GetSafeHwnd() : NULL;
    WCHAR szDBSource[256];
    szDBSource[0] = _T('\0');

    LONG_PTR nRes = aaApi_LoginDlgExt(hWnd, _T("CISDI-PW用户登录"), AALOGIN_SILENT,
                                      szDBSource, 256, NULL, NULL, NULL);
    return (nRes == IDOK);
}

BOOL Logout()
{
    // 断开当前活动数据源连接，便于切换账号后重新登录
    HDSOURCE hDs = aaApi_GetActiveDatasource();
    if (hDs == 0)
        return TRUE;   // 当前未连接，视为已退出
    return aaApi_LogoutByHandle(hDs);
}

CString GetDatasourceName()
{
    WCHAR szBuf[256] = { 0 };
    aaApi_GetActiveDatasourceName(szBuf, 256);
    return CString(szBuf);
}

//--------------------------------------------------------------------------------------+
// 文件系统
//--------------------------------------------------------------------------------------+

BOOL CreateDirRecursive(LPCTSTR pszPath)
{
    if (pszPath == NULL || pszPath[0] == _T('\0'))
        return FALSE;

    CString strPath(pszPath);
    strPath.Replace(_T('/'), _T('\\'));
    strPath.TrimRight(_T('\\'));

    if (strPath.IsEmpty())
        return FALSE;

    DWORD dwAttr = GetFileAttributes(strPath);
    if (dwAttr != INVALID_FILE_ATTRIBUTES && (dwAttr & FILE_ATTRIBUTE_DIRECTORY))
        return TRUE;

    // 逐段创建：在每个 '\' 处及路径末尾截取前缀段（D:\、D:\a、D:\a\b、...），
    // 缺哪级补哪级；任一级创建失败立即返回，避免后续级联失败掩盖真实原因。
    for (int i = 0; i <= strPath.GetLength(); i++)
    {
        if (i < strPath.GetLength() && strPath[i] != _T('\\'))
            continue;   // 只在反斜杠或结尾处截断

        CString strSeg = strPath.Left(i);
        if (strSeg.IsEmpty())
            continue;
        // [修复] 仅跳过裸盘符 "D:"（长度==2）。原条件 GetLength()>=2 会把
        // "D:\xxx"、"D:\xxx\yyy" 等所有 D:\ 开头的段都误判为盘符跳过，
        // 导致整条路径一级目录都不会被创建，函数恒返回 FALSE。
        if (strSeg.GetLength() == 2 && strSeg[1] == _T(':'))
            continue;   // 盘符

        if (GetFileAttributes(strSeg) == INVALID_FILE_ATTRIBUTES)
        {
            if (!CreateDirectoryW(strSeg, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
                return FALSE;
        }
    }

    dwAttr = GetFileAttributes(strPath);
    return (dwAttr != INVALID_FILE_ATTRIBUTES && (dwAttr & FILE_ATTRIBUTE_DIRECTORY));
}

CString GetAppBaseDir()
{
    static CString s_base;
    if (s_base.IsEmpty())
    {
        WCHAR szPath[MAX_PATH] = { 0 };
        GetModuleFileName(NULL, szPath, MAX_PATH);
        CString strExe(szPath);
        int nPos = strExe.ReverseFind(_T('\\'));
        if (nPos >= 0)
            s_base = strExe.Left(nPos);
    }
    return s_base;
}

CString GetModelDir()
{
    return GetAppBaseDir() + _T("\\model");
}

CString GetLinkModelDir()
{
    return GetAppBaseDir() + _T("\\LinkModel");
}

//--------------------------------------------------------------------------------------+
// INI（PW 地址来源）
//--------------------------------------------------------------------------------------+

CString GetAddrIniPath(LPCTSTR pszFolder)
{
    CString strIni(pszFolder);
    if (!strIni.IsEmpty() && strIni[strIni.GetLength() - 1] != _T('\\'))
        strIni += _T('\\');
    return strIni + _T("PWAddress.ini");
}

BOOL SavePwAddr(LPCTSTR pszFolder, LPCTSTR pszLocalFile, const PWAddrInfo& info)
{
    CString strIni = GetAddrIniPath(pszFolder);
    CString strSec = GetFileName(pszLocalFile);
    CString strTmp;

    WritePrivateProfileStringW(strSec, _T("datasource"), info.strDatasource, strIni);
    strTmp.Format(_T("%ld"), info.lProjectId);
    WritePrivateProfileStringW(strSec, _T("projectID"), strTmp, strIni);
    WritePrivateProfileStringW(strSec, _T("projectName"), info.strProjectName, strIni);
    strTmp.Format(_T("%ld"), info.lDocumentId);
    WritePrivateProfileStringW(strSec, _T("docID"), strTmp, strIni);
    WritePrivateProfileStringW(strSec, _T("version"), info.strVersion, strIni);      // 版本号（A/B/C）
    WritePrivateProfileStringW(strSec, _T("versionDate"), info.strVersionDate, strIni);
    WritePrivateProfileStringW(strSec, _T("comment"), info.strComment, strIni);
    strTmp.Format(_T("%d"), info.bLink ? 1 : 0);
    WritePrivateProfileStringW(strSec, _T("linkState"), strTmp, strIni);
    // 记录用户实际选择的本地完整路径
    CString strLocal = info.strLocalPath;
    if (strLocal.IsEmpty())
        strLocal = CString(pszFolder) + _T("\\") + GetFileName(pszLocalFile);
    WritePrivateProfileStringW(strSec, _T("localPath"), strLocal, strIni);
    return TRUE;
}

// 便捷版：查询数据源名/项目名/版本号/更新时间并写入 INI（各调用方无需重复拼装 PWAddrInfo）。
// pszLocalPath 非空时作为记录的本地路径，否则默认 folder\文件名。
void SavePwAddrOfDocument(LPCTSTR pszFolder, LPCTSTR pszLocalFile,
                          LONG lProjectId, LONG lDocumentId, BOOL bLink,
                          const CString& strComment, LPCTSTR pszLocalPath)
{
    PWAddrInfo info;
    info.strDatasource = GetDatasourceName();
    info.lProjectId = lProjectId;
    info.lDocumentId = lDocumentId;
    info.bLink = bLink;
    info.strComment = strComment;
    if (pszLocalPath != NULL)
        info.strLocalPath = pszLocalPath;

    if (lProjectId > 0)
    {
        if (aaApi_SelectProject(lProjectId) > 0)
        {
            const WCHAR* psz = aaApi_GetProjectStringProperty(PROJ_PROP_NAME, 0);
            if (psz != NULL)
                info.strProjectName = psz;
        }
        if (lDocumentId > 0)
        {
            info.strVersion = GetVersion(lProjectId, lDocumentId);
            info.strVersionDate = GetVersionDate(lProjectId, lDocumentId);
        }
    }
    SavePwAddr(pszFolder, pszLocalFile, info);
}

BOOL LoadPwAddr(LPCTSTR pszFolder, LPCTSTR pszLocalFile, PWAddrInfo& info)
{
    CString strIni = GetAddrIniPath(pszFolder);
    CString strSec = GetFileName(pszLocalFile);
    TCHAR szBuf[512];

    info = PWAddrInfo();
    DWORD nLen = GetPrivateProfileStringW(strSec, _T("datasource"), _T(""), szBuf, 512, strIni);
    if (nLen == 0)
        return FALSE;   // 该文件没有 PW 地址记录

    info.strDatasource = szBuf;

    GetPrivateProfileStringW(strSec, _T("projectID"), _T("0"), szBuf, 512, strIni);
    info.lProjectId = _tstol(szBuf);
    GetPrivateProfileStringW(strSec, _T("projectName"), _T(""), szBuf, 512, strIni);
    info.strProjectName = szBuf;
    GetPrivateProfileStringW(strSec, _T("docID"), _T("0"), szBuf, 512, strIni);
    info.lDocumentId = _tstol(szBuf);
    GetPrivateProfileStringW(strSec, _T("version"), _T(""), szBuf, 512, strIni);
    info.strVersion = szBuf;
    GetPrivateProfileStringW(strSec, _T("versionDate"), _T(""), szBuf, 512, strIni);
    info.strVersionDate = szBuf;
    GetPrivateProfileStringW(strSec, _T("comment"), _T(""), szBuf, 512, strIni);
    info.strComment = szBuf;
    GetPrivateProfileStringW(strSec, _T("linkState"), _T("0"), szBuf, 512, strIni);
    info.bLink = (_tstol(szBuf) != 0);
    GetPrivateProfileStringW(strSec, _T("localPath"), _T(""), szBuf, 512, strIni);
    info.strLocalPath = szBuf;
    return TRUE;
}

BOOL SetPwAddrLinkState(LPCTSTR pszFolder, LPCTSTR pszLocalFile, BOOL bLink)
{
    CString strIni = GetAddrIniPath(pszFolder);
    CString strSec = GetFileName(pszLocalFile);
    CString strTmp;
    strTmp.Format(_T("%d"), bLink ? 1 : 0);
    return WritePrivateProfileStringW(strSec, _T("linkState"), strTmp, strIni);
}

BOOL DeletePwAddr(LPCTSTR pszFolder, LPCTSTR pszLocalFile)
{
    CString strIni = GetAddrIniPath(pszFolder);
    CString strSec = GetFileName(pszLocalFile);
    return WritePrivateProfileStringW(strSec, NULL, NULL, strIni);
}

BOOL EnumPwAddrSections(LPCTSTR pszFolder, CStringArray& arrFiles)
{
    arrFiles.RemoveAll();
    CString strIni = GetAddrIniPath(pszFolder);
    if (GetFileAttributes(strIni) == INVALID_FILE_ATTRIBUTES)
        return FALSE;

    DWORD nSize = 4096;
    TCHAR* pBuf = new TCHAR[nSize];
    DWORD nLen = GetPrivateProfileStringW(NULL, NULL, _T(""), pBuf, nSize, strIni);
    if (nLen >= nSize - 2)   // 缓冲不够，按实际大小重试
    {
        delete[] pBuf;
        nSize = nLen + 256;
        pBuf = new TCHAR[nSize];
        nLen = GetPrivateProfileStringW(NULL, NULL, _T(""), pBuf, nSize, strIni);
    }

    TCHAR* p = pBuf;
    while (p != NULL && *p != _T('\0'))
    {
        arrFiles.Add(p);
        p += _tcslen(p) + 1;
    }
    delete[] pBuf;
    return (arrFiles.GetSize() > 0);
}

CString GetLinkFolderIniPath()
{
    return GetAppBaseDir() + _T("\\LinkFolders.ini");
}

BOOL RegisterLinkFolder(LPCTSTR pszFolder)
{
    if (pszFolder == NULL || pszFolder[0] == _T('\0'))
        return FALSE;

    CString strIni = GetLinkFolderIniPath();
    CString strFolder(pszFolder);
    strFolder.TrimRight(_T('\\'));
    if (strFolder.IsEmpty())
        return FALSE;

    // 已登记则不重复添加
    int nCount = (int)GetPrivateProfileIntW(_T("folders"), _T("count"), 0, strIni);
    for (int i = 0; i < nCount; i++)
    {
        CString strKey; strKey.Format(_T("dir%d"), i);
        TCHAR szBuf[MAX_PATH] = { 0 };
        GetPrivateProfileStringW(_T("folders"), strKey, _T(""), szBuf, MAX_PATH, strIni);
        if (szBuf[0] != _T('\0') && _wcsicmp(szBuf, strFolder) == 0)
            return TRUE;
    }

    CString strKey; strKey.Format(_T("dir%d"), nCount);
    CString strCnt; strCnt.Format(_T("%d"), nCount + 1);
    WritePrivateProfileStringW(_T("folders"), strKey, strFolder, strIni);
    WritePrivateProfileStringW(_T("folders"), _T("count"), strCnt, strIni);
    return TRUE;
}

void EnumLinkFolders(CStringArray& arrFolders)
{
    arrFolders.RemoveAll();
    // 始终包含默认 LinkModel 目录
    arrFolders.Add(GetLinkModelDir());

    CString strIni = GetLinkFolderIniPath();
    int nCount = (int)GetPrivateProfileIntW(_T("folders"), _T("count"), 0, strIni);
    for (int i = 0; i < nCount; i++)
    {
        CString strKey; strKey.Format(_T("dir%d"), i);
        TCHAR szBuf[MAX_PATH] = { 0 };
        GetPrivateProfileStringW(_T("folders"), strKey, _T(""), szBuf, MAX_PATH, strIni);
        if (szBuf[0] == _T('\0'))
            continue;

        CString strDir(szBuf);
        BOOL bDup = FALSE;
        for (INT_PTR j = 0; j < arrFolders.GetSize(); j++)
        {
            if (arrFolders[j].CompareNoCase(strDir) == 0)
            {
                bDup = TRUE;
                break;
            }
        }
        if (!bDup)
            arrFolders.Add(strDir);
    }
}

//--------------------------------------------------------------------------------------+
// 最近使用参数（%APPDATA%\PWTool\Settings.ini）
//--------------------------------------------------------------------------------------+

// 应用配置目录：默认 %APPDATA%\PWTool。exe 可能部署在 Program Files 等不可写目录
// （写 INI 会被 UAC 静默拦截/虚拟化），配置存到用户目录才保证可读可写；APPDATA 缺失时退回 exe 目录。
static CString GetSettingsDir()
{
    CString strDir = GetEnvVar(_T("APPDATA"));
    if (strDir.IsEmpty())
        return GetAppBaseDir();
    if (strDir[strDir.GetLength() - 1] != _T('\\'))
        strDir += _T('\\');
    return strDir + _T("PWTool");
}

CString GetSettingsIniPath()
{
    return GetSettingsDir() + _T("\\Settings.ini");
}

CString GetLastLocalPath(LocalPathMode mode)
{
    CString strIni = GetSettingsIniPath();
    TCHAR szBuf[MAX_PATH * 2] = { 0 };
    GetPrivateProfileStringW(_T("Recent"),
                             (mode == LOCALPATH_LINK) ? _T("LinkLocalPath") : _T("OpenLocalPath"),
                             _T(""), szBuf, MAX_PATH * 2, strIni);
    return CString(szBuf);
}

void SetLastLocalPath(LPCTSTR pszPath, LocalPathMode mode)
{
    if (pszPath == NULL || pszPath[0] == _T('\0'))
        return;
    CString strIni = GetSettingsIniPath();
    CreateDirRecursive(GetSettingsDir());   // 确保配置目录存在（WritePrivateProfileString 不建目录）
    WritePrivateProfileStringW(_T("Recent"),
                               (mode == LOCALPATH_LINK) ? _T("LinkLocalPath") : _T("OpenLocalPath"),
                               pszPath, strIni);
}

//--------------------------------------------------------------------------------------+
// PW 操作
//--------------------------------------------------------------------------------------+

LONG GetLatestDocumentId(LONG lProjectId, LONG lDocumentId)
{
    if (lProjectId <= 0 || lDocumentId <= 0)
        return lDocumentId;

    LONG nRes = aaApi_SelectDocument(lProjectId, lDocumentId);
    if (nRes != 1)
        return lDocumentId;

    // 当前版本 ORIGINALNO=0；历史版本的 ORIGINALNO 指向当前(活动)版本的 docid
    LONG lOriginalNo = aaApi_GetDocumentNumericProperty(DOC_PROP_ORIGINALNO, 0);
    return (lOriginalNo > 0) ? lOriginalNo : lDocumentId;
}

// 递归删除目录（含只读文件/子目录）
static void RemoveDirRecursive(LPCTSTR pszDir)
{
    if (pszDir == NULL || pszDir[0] == _T('\0'))
        return;

    WIN32_FIND_DATAW fd;
    CString strPattern = CString(pszDir) + _T("\\*");
    HANDLE hFind = FindFirstFileW(strPattern, &fd);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
                continue;
            CString strFull = CString(pszDir) + _T("\\") + fd.cFileName;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                SetFileAttributes(strFull, FILE_ATTRIBUTE_NORMAL);
                RemoveDirRecursive(strFull);
            }
            else
            {
                SetFileAttributes(strFull, FILE_ATTRIBUTE_NORMAL);
                DeleteFileW(strFull);
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }
    SetFileAttributes(pszDir, FILE_ATTRIBUTE_NORMAL);
    RemoveDirectoryW(pszDir);
}

// 把 pszSrcDir 下所有文件（含子目录）移动到 pszDstDir，保留相对结构，已存在则覆盖。
// 单个文件移动失败跳过不中断；返回成功移动的文件数。
static int MoveDirContents(LPCTSTR pszSrcDir, LPCTSTR pszDstDir)
{
    int nMoved = 0;
    if (pszSrcDir == NULL || pszSrcDir[0] == _T('\0'))
        return 0;

    WIN32_FIND_DATAW fd;
    CString strPattern = CString(pszSrcDir) + _T("\\*");
    HANDLE hFind = FindFirstFileW(strPattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE)
        return 0;
    do
    {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;
        CString strSrcFull = CString(pszSrcDir) + _T("\\") + fd.cFileName;
        CString strDstFull = CString(pszDstDir) + _T("\\") + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            CreateDirRecursive(strDstFull);
            nMoved += MoveDirContents(strSrcFull, strDstFull);
            RemoveDirectoryW(strSrcFull);
        }
        else
        {
            SetFileAttributes(strDstFull, FILE_ATTRIBUTE_NORMAL);   // 清只读以便覆盖
            if (MoveFileEx(strSrcFull, strDstFull, MOVEFILE_REPLACE_EXISTING))
                nMoved++;
            else if (CopyFile(strSrcFull, strDstFull, FALSE))
            {
                DeleteFile(strSrcFull);
                nMoved++;
            }
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
    return nMoved;
}

// 下载流程诊断日志：写入 exe 目录\pw_link.log（与链接流程同文件），以 [下载] tag 区分
static void LogPwDebug(LPCTSTR pszLine)
{
    AppendLog(GetAppBaseDir() + _T("\\pw_link.log"), _T("下载"), pszLine);
}

// 下载指定 docid 指向的版本到工作目录（不改写为最新版本，调用方自行决定用哪个版本）。
// [修复] 目标目录已有同名文件时 CopyOutDocument 会失败或生成带序号的新文件，导致覆盖下载
// 被误判失败。先下载到临时子目录（目录内无同名文件，写回原始文件名），再整体移动/覆盖到工作目录。
// 成功返回 TRUE，strOutFile 为最终路径；失败返回 FALSE，pstrErr 非空时给出原因。
BOOL DownloadDocument(LONG lProjectId, LONG lDocumentId, LPCTSTR pszWorkDir,
                      CString& strOutFile, CString* pstrErr)
{
    strOutFile.Empty();
    if (pstrErr != NULL)
        pstrErr->Empty();
    if (lDocumentId <= 0)
        return FALSE;
    if (pszWorkDir == NULL || !CreateDirRecursive(pszWorkDir))
        return FALSE;

    // 清理上次异常残留的 __pwdl_* 临时目录
    {
        WIN32_FIND_DATAW fd;
        CString strPattern = CString(pszWorkDir) + _T("\\__pwdl_*");
        HANDLE hFind = FindFirstFileW(strPattern, &fd);
        if (hFind != INVALID_HANDLE_VALUE)
        {
            do
            {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    RemoveDirRecursive(CString(pszWorkDir) + _T("\\") + fd.cFileName);
            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);
        }
    }

    // 用唯一的临时子目录下载（__pwdl_<docid>_<序号>）。
    // [修复] 同一文档的两个版本拷到同一目录会触发 AAERR_DMS_ERR_CO_LOCATION_IS_USED
    // （"拷出位置已被占用"），每次用新目录绕开。
    static LONG s_nPwTmpSeq = 0;

    // [修复] 首次拷出偶发瞬时失败、重试即成功（服务端会话/缓存未就绪），故自动重试
    // （最多3次、间隔1秒、每次用全新临时目录）；每次失败记日志，最终失败带错误码给调用方。
    const int MAX_TRY = 3;
    BOOL bOk = FALSE;
    TCHAR szOut[MAX_PATH * 2] = { 0 };
    CString strTmpDir;      // 成功那次所使用的临时目录
    CString strDl;          // 成功那次下载写出的文件（含完整路径）
    CString strErrLast;     // 最近一次失败的原因（拷出失败取自 SDK；拷出成功但无输出文件时自拟）
    LONG lLastErrId = 0;
    BOOL bIsActive = FALSE;
    LONG lDocSize = 0;
    LONG lShowVer = -1;

    for (int nTry = 0; nTry < MAX_TRY && !bOk; nTry++)
    {
        strTmpDir.Format(_T("%s\\__pwdl_%ld_%ld"), pszWorkDir, lDocumentId, ++s_nPwTmpSeq);
        if (!CreateDirRecursive(strTmpDir))
        {
            if (pstrErr != NULL)
                *pstrErr = _T("创建临时目录失败。");
            return FALSE;
        }
        szOut[0] = _T('\0');
        strErrLast.Empty();
        lLastErrId = 0;

        // 判断该 docid 是否为活动(当前)版本：活动版本 ORIGINALNO=0，历史版本 ORIGINALNO>0。
        // 两种版本采用不同的取数方式。重试时重新判定——首次失败也可能源于 SelectDocument
        // 在会话刚建立时短暂不可用。
        bIsActive = FALSE;
        lDocSize = 0;
        if (aaApi_SelectDocument(lProjectId, lDocumentId) == 1)
        {
            bIsActive = (aaApi_GetDocumentNumericProperty(DOC_PROP_ORIGINALNO, 0) == 0);
            lDocSize = aaApi_GetDocumentNumericProperty(DOC_PROP_SIZE, 0);
        }

        BOOL bToggled = FALSE;
        lShowVer = -1;
        if (bIsActive)
        {
            // 当前版本：加 AADMS_DOCFETCH_IGNORE_UPTODATECOPY 强制从服务器重新下载，
            // 避免 SDK 复用本地缓存里"判为已最新"的旧版本内容——本数据源创建新版本时会重分配
            // docid，缓存容易把旧版本内容误当成最新版，导致"链接最新版却下到旧内容"。
            ULONG ulFetch = AADMS_DOCFETCH_COPYOUT
                | AADMS_DOCFETCH_IGNORE_UPTODATECOPY
                | AADMS_DOCFETCH_MASTER_AS_SET
                | AADMS_DOCFETCH_NESTED_REFERENCES
                | AADMS_DOCFETCH_REDLINED_REFERENCES;
            bOk = aaApi_FetchDocumentFromServer(ulFetch, lProjectId, lDocumentId,
                                                strTmpDir, szOut, MAX_PATH * 2);
        }
        else
        {
            // 历史版本：SDK 在用户设置 AADMS_PAR_DIS_SHOW_VERSION=0（"只显示当前版本"）时
            // 拒绝拷出非当前版本。临时打开该设置再恢复（仅客户端本地生效，无需管理员权限）。
            // 取数用 CopyOutDocument（原已验证可下载历史版本的路径），不加 IGNORE_UPTODATECOPY。
            lShowVer = aaApi_GetUserNumericSetting(AADMS_PAR_DIS_SHOW_VERSION);
            if (lShowVer == 0)
                bToggled = aaApi_SetUserNumericSetting(AADMS_PAR_DIS_SHOW_VERSION, 1);
            bOk = aaApi_CopyOutDocument(lProjectId, lDocumentId, strTmpDir, szOut, MAX_PATH * 2);
            if (bToggled)
                aaApi_SetUserNumericSetting(AADMS_PAR_DIS_SHOW_VERSION, lShowVer);   // 恢复原值
        }

        if (bOk)
        {
            // 拷出返回成功但未回填文件名时，按文档 FILENAME 属性拼出临时文件路径
            strDl = szOut;
            if (strDl.IsEmpty() && aaApi_SelectDocument(lProjectId, lDocumentId) == 1)
            {
                const WCHAR* psz = aaApi_GetDocumentStringProperty(DOC_PROP_FILENAME, 0);
                if (psz != NULL)
                    strDl = strTmpDir + _T("\\") + psz;
            }
            if (strDl.IsEmpty() || GetFileAttributes(strDl) == INVALID_FILE_ATTRIBUTES)
            {
                // 拷出"成功"却没产出文件，同样按一次失败处理，下次重试
                bOk = FALSE;
                strErrLast = _T("下载后未找到输出文件。");
            }
        }

        if (!bOk)
        {
            // 完整诊断：错误ID + 版本属性(活动? 服务器记录的文件大小) + 设置切换情况，每次尝试各记一行
            if (strErrLast.IsEmpty())   // 拷出失败时取 SDK 错误；"无输出文件"时已在上方赋值
            {
                lLastErrId = aaApi_GetLastErrorId();
                strErrLast = GetLastErrorText();
            }
            CString strDbg;
            strDbg.Format(_T("下载失败(第%d/%d次): 项目=%ld docid=%ld 活动=%d 服务器大小=%ld showVer=%ld 切换=%d errId=%ld err=%s"),
                nTry + 1, MAX_TRY, lProjectId, lDocumentId, (int)bIsActive, lDocSize,
                lShowVer, (int)bToggled, lLastErrId, (LPCTSTR)strErrLast);
            LogPwDebug(strDbg);
            RemoveDirRecursive(strTmpDir);
            if (nTry + 1 < MAX_TRY)
                Sleep(1000);   // 短暂停顿，让服务端会话/状态就绪后再重试
        }
    }

    if (!bOk)
    {
        // 三次尝试均失败：把最终原因（含错误码）带给调用方显示/记录
        if (pstrErr != NULL)
        {
            if (lLastErrId != 0)
            {
                CString strFinal;
                strFinal.Format(_T("%s (错误码 %ld)"), (LPCTSTR)strErrLast, lLastErrId);
                *pstrErr = strFinal;
            }
            else
                *pstrErr = strErrLast;
        }
        return FALSE;
    }

    // 主文件最终路径 = 工作目录 + 下载文件名（与直接落盘时的名字一致）
    CString strMain = CString(pszWorkDir) + _T("\\") + GetFileName(strDl);

    // 把临时目录里所有文件（含引用的参照文件）移动/覆盖到工作目录
    MoveDirContents(strTmpDir, pszWorkDir);

    // [修复] 判断主文件是否真被替换：临时主文件已移走 && 目标已存在，
    // 只判目标存在会漏掉"移动失败但旧文件仍在"的误判成功。
    if (GetFileAttributes(strDl) != INVALID_FILE_ATTRIBUTES
        || GetFileAttributes(strMain) == INVALID_FILE_ATTRIBUTES)
    {
        if (pstrErr != NULL)
            *pstrErr = _T("无法用最新版本覆盖本地文件，文件可能正被 CAD 等程序打开，请先关闭该文件后重试。");
        // 临时目录保留下载结果，避免新版本丢失；下次下载会自动清理。
        return FALSE;
    }

    RemoveDirRecursive(strTmpDir);
    strOutFile = strMain;

    // 成功日志：记录版本属性，便于核对下到的是哪个版本
    {
        CFileStatus fs;
        ULONGLONG nSize = 0;
        if (CFile::GetStatus(strMain, fs))
            nSize = fs.m_size;
        const WCHAR* pszVer = NULL;
        if (aaApi_SelectDocument(lProjectId, lDocumentId) == 1)
            pszVer = aaApi_GetDocumentStringProperty(DOC_PROP_VERSION, 0);
        CString strDbg;
        strDbg.Format(_T("下载成功: docid=%ld 活动=%d showVer=%ld 版本=%s 大小=%I64u"),
            lDocumentId, (int)bIsActive, lShowVer,
            (pszVer != NULL) ? pszVer : _T("?"), nSize);
        LogPwDebug(strDbg);
    }
    return TRUE;
}

// 解析版本记录中的创建人/修改人用户ID为登录账号名（填回 item 的 str*Name）。
// 用户ID为0或解析失败时保持空串，由调用方显示"-"。
static void FillVersionUserNames(PWDocVersionItem& item)
{
    if (item.lCreatorId > 0)
        item.strCreatorName = GetUserNameById(item.lCreatorId);
    if (item.lUpdaterId > 0)
        item.strUpdaterName = GetUserNameById(item.lUpdaterId);
}

// 按文件名列出项目内所有同名文档（每个同名文档视为一个"版本"），从新到旧排序。
// [数据源] 版本集枚举 API 在本数据源只返回活动版本，改用按文件名匹配所有文档行来枚举。
// 返回值=数量。
// 比较版本串（A<B<...<Z<AA）：返回 a>b ? 1 : (a<b ? -1 : 0)
int CompareVersionStrings(LPCTSTR pszA, LPCTSTR pszB)
{
    CString a(pszA), b(pszB);
    auto value = [](const CString& s) -> __int64
    {
        __int64 v = 0;
        for (int i = 0; i < s.GetLength(); i++)
        {
            TCHAR ch = s[i];
            if (ch >= _T('a') && ch <= _T('z')) ch -= 32;
            if (ch < _T('A') || ch > _T('Z'))
                return (__int64)(ch & 0xFF);   // 非字母：用首字符简单比较
            v = v * 26 + (ch - _T('A') + 1);
        }
        return v;
    };
    __int64 va = value(a), vb = value(b);
    if (va > vb) return 1;
    if (va < vb) return -1;
    return 0;
}

LONG EnumSameNameDocuments(LONG lProjectId, LONG lDocumentId,
                           CArray<PWDocVersionItem, PWDocVersionItem&>& arrVersions)
{
    arrVersions.RemoveAll();
    if (lProjectId <= 0 || lDocumentId <= 0)
        return 0;

    // 取本文档的文件名
    CString strName;
    if (aaApi_SelectDocument(lProjectId, lDocumentId) == 1)
    {
        const WCHAR* psz = aaApi_GetDocumentStringProperty(DOC_PROP_FILENAME, 0);
        if (psz) strName = psz;
    }
    if (strName.IsEmpty())
        return 0;

    // 扫描项目内所有文档行（含各版本），按文件名匹配
    LONG nAll = aaApi_SelectDocumentsByProjectId(lProjectId);
    for (LONG i = 0; i < nAll; i++)
    {
        const WCHAR* psz = aaApi_GetDocumentStringProperty(DOC_PROP_FILENAME, i);
        if (!psz || strName.CompareNoCase(psz) != 0)
            continue;

        PWDocVersionItem item;
        item.lDocumentId = aaApi_GetDocumentNumericProperty(DOC_PROP_ID, i);
        item.lVersionNo = aaApi_GetDocumentNumericProperty(DOC_PROP_VERSIONNO, i);
        item.lSize = aaApi_GetDocumentNumericProperty(DOC_PROP_SIZE, i);
        item.lOriginalNo = aaApi_GetDocumentNumericProperty(DOC_PROP_ORIGINALNO, i);
        item.lCreatorId = aaApi_GetDocumentNumericProperty(DOC_PROP_CREATORID, i);
        item.lUpdaterId = aaApi_GetDocumentNumericProperty(DOC_PROP_UPDATERID, i);
        psz = aaApi_GetDocumentStringProperty(DOC_PROP_VERSION, i);
        if (psz) item.strVersion = psz;
        psz = aaApi_GetDocumentStringProperty(DOC_PROP_FILE_UPDATE_TIME, i);
        if (psz) item.strUpdateTime = psz;
        FillVersionUserNames(item);
        arrVersions.Add(item);
    }

    // 排序：活动版本(ORIGINALNO=0)最前(=最新)，其余按版本串从新到旧。
    // [修复] 不能按 docid 判断新旧：创建新版本时旧版本会被重新分配更高的 docid。
    for (INT_PTR i = 0; i + 1 < arrVersions.GetSize(); i++)
        for (INT_PTR j = i + 1; j < arrVersions.GetSize(); j++)
        {
            PWDocVersionItem& a = arrVersions[i];
            PWDocVersionItem& b = arrVersions[j];
            BOOL bSwap = FALSE;
            if (a.lOriginalNo != 0 && b.lOriginalNo == 0)
                bSwap = TRUE;   // a是历史、b是活动 → b应在前
            else if (a.lOriginalNo == b.lOriginalNo && CompareVersionStrings(a.strVersion, b.strVersion) < 0)
                bSwap = TRUE;   // 同类型：版本串大的在前
            if (bSwap)
            {
                PWDocVersionItem t = a; a = b; b = t;
            }
        }

    return (LONG)arrVersions.GetSize();
}

// 在项目内按文件名查找已存在文档，返回其最新(活动)版本 docid；不存在返回 0。
// [用途] 多人上传同一文件名：B 首次上传时目标目录已有 A 上传的同名文档，
// 不能再新建，改为对已存在文档上传新版本。按文件名不区分大小写匹配，优先取活动版本。
LONG FindDocumentIdByName(LONG lProjectId, LPCTSTR pszFileName)
{
    if (lProjectId <= 0 || pszFileName == NULL || pszFileName[0] == _T('\0'))
        return 0;

    CString strName(pszFileName);
    LONG nAll = aaApi_SelectDocumentsByProjectId(lProjectId);
    LONG lActiveDoc = 0, lFallbackDoc = 0;
    for (LONG i = 0; i < nAll; i++)
    {
        const WCHAR* psz = aaApi_GetDocumentStringProperty(DOC_PROP_FILENAME, i);
        if (!psz || strName.CompareNoCase(psz) != 0)
            continue;
        LONG lDocId = aaApi_GetDocumentNumericProperty(DOC_PROP_ID, i);
        if (aaApi_GetDocumentNumericProperty(DOC_PROP_ORIGINALNO, i) == 0)
            lActiveDoc = lDocId;            // 活动版本(最新)优先
        else if (lFallbackDoc == 0)
            lFallbackDoc = lDocId;
    }
    return (lActiveDoc != 0) ? lActiveDoc : lFallbackDoc;
}

// 便捷版：枚举指定文档全部同名版本后写 dump 到 exe 目录 pw_version_dump.txt。
void DumpDocumentVersionsToFile(LONG lProjectId, LONG lDocumentId)
{
    CArray<PWDocVersionItem, PWDocVersionItem&> arr;
    EnumSameNameDocuments(lProjectId, lDocumentId, arr);
    CString strPath = GetAppBaseDir() + _T("pw_version_dump.txt");
    DumpVersionItems(arr, strPath);
}

// 诊断：把各版本原始字段写到日志文件（UTF-8），便于排查创建人/上传人字段被数据源写反的问题。
void DumpVersionItems(const CArray<PWDocVersionItem, PWDocVersionItem&>& arrVersions,
                      LPCTSTR pszLogPath)
{
    FILE* f = NULL;
    _wfopen_s(&f, pszLogPath, L"w, ccs=UTF-8");
    if (f == NULL)
        return;

    fwprintf(f, L"# version | versionno | docid | originalno | creatorid | updaterid | creator | updater | time\r\n");
    for (INT_PTR i = 0; i < arrVersions.GetSize(); i++)
    {
        const PWDocVersionItem& v = arrVersions.GetAt(i);
        fwprintf(f, L"%s | %ld | %ld | %ld | %ld | %ld | %s | %s | %s\r\n",
            (LPCWSTR)v.strVersion, v.lVersionNo, v.lDocumentId, v.lOriginalNo,
            v.lCreatorId, v.lUpdaterId,
            v.strCreatorName.IsEmpty() ? L"-" : (LPCWSTR)v.strCreatorName,
            v.strUpdaterName.IsEmpty() ? L"-" : (LPCWSTR)v.strUpdaterName,
            (LPCWSTR)v.strUpdateTime);
    }
    fclose(f);
}

BOOL DownloadAndReplace(LONG lProjectId, LONG lDocumentId, LPCTSTR pszTargetFile,
                        CString& strErr, CString* pstrOutFile)
{
    strErr.Empty();
    if (pstrOutFile != NULL)
        pstrOutFile->Empty();
    if (pszTargetFile == NULL || pszTargetFile[0] == _T('\0'))
    {
        strErr = _T("目标文件路径为空。");
        return FALSE;
    }

    CString strFolder = GetFileFolder(pszTargetFile);
    if (strFolder.IsEmpty() || !CreateDirRecursive(strFolder))
    {
        strErr = _T("本地目录无效。");
        return FALSE;
    }

    // 下载落到目标文件（链接模型）所在目录。DownloadDocument 现会下载到临时子目录再
    // 覆盖到目标目录，因此 strOut 通常已等于 pszTargetFile；下面的同名移动仅作为兜底。
    CString strOut;
    CString strDlErr;
    if (!DownloadDocument(lProjectId, lDocumentId, strFolder, strOut, &strDlErr))
    {
        strErr = strDlErr.IsEmpty() ? GetLastErrorText() : strDlErr;
        return FALSE;
    }
    if (pstrOutFile != NULL)
        *pstrOutFile = strOut;

    if (strOut.CompareNoCase(pszTargetFile) != 0)
    {
        // 优先用 MoveFileEx 直接替换；失败（目标被占用）时退回复制，仍失败则明确报错
        if (!MoveFileEx(strOut, pszTargetFile, MOVEFILE_REPLACE_EXISTING))
        {
            if (!CopyFile(strOut, pszTargetFile, FALSE))
            {
                CString strWinErr;
                strWinErr.Format(_T("错误码 %lu"), (unsigned long)GetLastError());
                strErr = _T("无法用最新版本覆盖本地文件（") + strWinErr +
                         _T("），文件可能正被 CAD 等程序打开，请先关闭该文件后重试。");
                DeleteFile(strOut);
                return FALSE;
            }
            DeleteFile(strOut);
        }
    }
    return TRUE;
}

CString GetVersionDate(LONG lProjectId, LONG lDocumentId)
{
    if (lDocumentId <= 0)
        return _T("");
    LONG nRes = aaApi_SelectDocument(lProjectId, lDocumentId);
    if (nRes != 1)
        return _T("");
    // [修复] 用 FILE_UPDATE_TIME(文件内容最后更新时间) 作为版本时间
    const WCHAR* psz = aaApi_GetDocumentStringProperty(DOC_PROP_FILE_UPDATE_TIME, 0);
    return (psz != NULL) ? CString(psz) : CString(_T(""));
}

CString GetVersion(LONG lProjectId, LONG lDocumentId)
{
    if (lDocumentId <= 0)
        return _T("");
    LONG nRes = aaApi_SelectDocument(lProjectId, lDocumentId);
    if (nRes != 1)
        return _T("");
    const WCHAR* psz = aaApi_GetDocumentStringProperty(DOC_PROP_VERSION, 0);
    return (psz != NULL) ? CString(psz) : CString(_T(""));
}

CString GetLatestVersionDate(LONG lProjectId, LONG lDocumentId)
{
    lDocumentId = GetLatestDocumentId(lProjectId, lDocumentId);
    if (lDocumentId <= 0)
        return _T("");
    return GetVersionDate(lProjectId, lDocumentId);
}

CString GetLatestVersion(LONG lProjectId, LONG lDocumentId)
{
    lDocumentId = GetLatestDocumentId(lProjectId, lDocumentId);
    if (lDocumentId <= 0)
        return _T("");
    LONG nRes = aaApi_SelectDocument(lProjectId, lDocumentId);
    if (nRes != 1)
        return _T("");
    const WCHAR* psz = aaApi_GetDocumentStringProperty(DOC_PROP_VERSION, 0);
    return (psz != NULL) ? CString(psz) : CString(_T(""));
}

LONG GetDocumentAccess(LONG lProjectId, LONG lDocumentId, LONG lIndex)
{
    // lIndex>=0 时按静态buffer索引取权限（不重新select，避免覆盖buffer）；
    // lIndex<0 时内部先select指定文档再取权限。
    return aaApi_GetDocumentAccess(lProjectId, lDocumentId, lIndex);
}

// 回调触发标记：检入操作是否实际执行（用于区分"取消"与"检入"）
static BOOL g_bCheckInFired = FALSE;

// 检入对话框回调：检入操作时把 isNewVersionCreated 强制置 TRUE，
// 使每次检入都自动生成新版本（无需用户手动勾选"生成新版本"）。
// [注] 该数据源建版本时创建人字段会轮转写错（App 端无法规避），但上传人(DOC_PROP_UPDATERID)
// 每版记录正确，版本列表以"上传人"列为准。
static LONG_PTR CALLBACK CheckInDlgForceNewVersionCb(AAPARAM callbackParam,
    eCheckInDlgMessage_t msgID, LPVOID msgData, LONG_PTR unhandledReturnCode)
{
    if (msgID == AACHECKINDLG_MSG_CHECK_IN)
    {
        g_bCheckInFired = TRUE;
        if (msgData != NULL)
            ((LPAACHECKINDLG_MSG_DATA)msgData)->isNewVersionCreated = TRUE;   // 强制生成新版本
        return IDOK;
    }
    return unhandledReturnCode;
}

BOOL UploadNewVersion(HWND hWndParent, LONG lProjectId, LONG lDocumentId,
                      LPCTSTR pszLocalFile, LPCTSTR pszWorkDir,
                      const CString& strComment, CString& strErr,
                      BOOL* pOutNewVersion)
{
    strErr.Empty();
    if (pOutNewVersion) *pOutNewVersion = FALSE;

    // 始终基于当前(最新)版本检入新版本：INI 中的 docid 可能指向历史版本
    lDocumentId = GetLatestDocumentId(lProjectId, lDocumentId);
    if (lDocumentId <= 0)
    {
        strErr = _T("无效的文档ID。");
        return FALSE;
    }

    // 检查写权限。[修复记录] aaApi_GetDocumentAccess 返回的是访问级别而非位掩码：
    //   AADMS_ACCESS_WRITE=可修改/可checkin，AADMS_ACCESS_READ=只读。
    LONG lAccess = aaApi_GetDocumentAccess(lProjectId, lDocumentId, -1);
    if ((lAccess & AADMS_ACCESS_WRITE) == 0)
    {
        strErr = _T("当前用户对该文档无写入权限，无法上传。");
        return FALSE;
    }

    // 清理上次异常残留的 __pwup_* 临时目录（镜像下载流程的做法，避免累积）
    if (pszWorkDir != NULL && pszWorkDir[0] != _T('\0'))
    {
        WIN32_FIND_DATAW fd;
        CString strPattern = CString(pszWorkDir) + _T("\\__pwup_*");
        HANDLE hFind = FindFirstFileW(strPattern, &fd);
        if (hFind != INVALID_HANDLE_VALUE)
        {
            do
            {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    RemoveDirRecursive(CString(pszWorkDir) + _T("\\") + fd.cFileName);
            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);
        }
    }

    // 用唯一临时子目录作为工作副本（__pwup_<docid>_<序号>）：固定目录名会残留/冲突，
    // 镜像下载流程用唯一目录绕开"拷出位置已被占用"的处理方式。
    static LONG s_nPwUpSeq = 0;
    CString strTempDir(pszWorkDir);
    if (!strTempDir.IsEmpty() && strTempDir[strTempDir.GetLength() - 1] != _T('\\'))
        strTempDir += _T('\\');
    CString strTmpName;
    strTmpName.Format(_T("__pwup_%ld_%ld"), lDocumentId, ++s_nPwUpSeq);
    strTempDir += strTmpName;
    if (!CreateDirRecursive(strTempDir))
    {
        strErr = _T("创建上传临时目录失败。");
        return FALSE;
    }

    // 1. CheckOut 加锁并下载工作副本到临时目录
    TCHAR szOut[MAX_PATH * 2] = { 0 };
    if (!aaApi_CheckOutDocument(lProjectId, lDocumentId, strTempDir, szOut, MAX_PATH * 2))
    {
        strErr = GetLastErrorText();
        return FALSE;
    }

    // 2. 用本地模型文件覆盖工作副本
    if (!CopyFile(pszLocalFile, szOut, FALSE))
    {
        strErr = _T("覆盖工作副本失败。");
        aaApi_CheckInDocument(lProjectId, lDocumentId);   // 释放锁
        return FALSE;
    }

    // 3. 弹出官方检入对话框完成检入；回调强制生成新版本。
    //    [数据源] 该数据源由 Workflow/Rules Engine 控制，直接 API 检入不生成新版本；
    //    官方检入对话框走正确工作流路径。回调把 isNewVersionCreated 置 TRUE，
    //    使每次检入都自动生成新版本，无需用户手动勾选。
    AADOC_ITEM docItem;
    docItem.lProjectId = lProjectId;
    docItem.lDocumentId = lDocumentId;

    WCHAR szComment[512] = { 0 };
    _tcsncpy_s(szComment, 512, strComment, _TRUNCATE);

    // 记录检入前的版本串，用于检入后判断是否真的生成了新版本
    CString strVerBefore = GetVersion(lProjectId, lDocumentId);

    g_bCheckInFired = FALSE;
    BOOL bOk = aaApi_DisplayDocumentCheckInActionDlg2(
        hWndParent,
        _T("上传 - 检入确认（将自动生成新版本，版本号系统分配）"),
        0,                                   // flags（保留，必须为0）
        &docItem, 1,                         // 文档
        szComment,                           // 默认版本说明
        CheckInDlgForceNewVersionCb,         // 回调：强制新版本
        NULL);                               // callbackParam

    // 清理临时工作副本
    DeleteFile(szOut);

    // 回调未触发=用户取消了检入对话框（返回码无法区分取消/成功，用回调标记判断）
    if (!g_bCheckInFired)
    {
        strErr = _T("检入已取消。");
        return FALSE;
    }
    if (!bOk)
    {
        strErr = _T("检入失败：") + GetLastErrorText();
        return FALSE;
    }

    // 4. [修复] 检入框按它自己的工作区解析"要检入哪个文件"，检入的新版本内容往往是旧内容
    //    （版本号有增但内容不是最新）。检入框建出新版本后，再用直接 API（CheckOut->覆盖->
    //    CheckIn）把最新版本内容改写为本地文件，保证上传内容就是本地最新。
    LONG lNewDocId = GetLatestDocumentId(lProjectId, lDocumentId);
    if (lNewDocId > 0)
    {
        TCHAR szOut2[MAX_PATH * 2] = { 0 };
        if (aaApi_CheckOutDocument(lProjectId, lNewDocId, strTempDir, szOut2, MAX_PATH * 2))
        {
            BOOL bCopied = CopyFile(pszLocalFile, szOut2, FALSE);
            BOOL bCheckIn = aaApi_CheckInDocument(lProjectId, lNewDocId);   // 检入时释放锁
            if (!bCopied || !bCheckIn)
            {
                strErr = _T("新版本内容修正失败：") + GetLastErrorText();
                return FALSE;
            }
        }
        else
        {
            strErr = _T("新版本内容修正失败：") + GetLastErrorText();
            return FALSE;
        }
    }

    // 检入后解析最新版本，与检入前版本串比对：不同=用户勾选了"创建新版本"、生成了新版本。
    CString strVerAfter;
    if (lNewDocId > 0)
        strVerAfter = GetVersion(lProjectId, lNewDocId);
    if (pOutNewVersion)
        *pOutNewVersion = (!strVerAfter.IsEmpty() && strVerAfter != strVerBefore);

    // 清理临时工作副本目录
    RemoveDirRecursive(strTempDir);
    return TRUE;
}

// [注] aaApi_CreateDocument 在 SDK samples 中无使用参考，仅按头文件文档实现；
// storageId/fileType/appId 传默认值(0)，已在真机验证"首次上传新建文档"可用。
BOOL CreateNewDocument(LONG lProjectId, LPCTSTR pszLocalFile, const CString& strComment,
                       LONG& lDocId, CString& strErr)
{
    strErr.Empty();
    lDocId = 0;

    // 取目标项目默认存储与工作区
    LONG lStorageId = 0, lWspProfId = 0;
    if (aaApi_SelectProject(lProjectId) > 0)
    {
        lStorageId = aaApi_GetProjectNumericProperty(PROJ_PROP_STORAGEID, 0);
        lWspProfId = aaApi_GetProjectNumericProperty(PROJ_PROP_WSPACEPROFID, 0);
    }

    CString strFileName = GetFileName(pszLocalFile);
    CString strName = strFileName;
    int nDot = strName.ReverseFind(_T('.'));
    if (nDot > 0)
        strName = strName.Left(nDot);

    TCHAR szWork[MAX_PATH * 2] = { 0 };
    LONG lDoc = 0;
    BOOL bOk = aaApi_CreateDocument(
        &lDoc, lProjectId, lStorageId,
        0 /*fileType*/, 0 /*itemType*/, 0 /*applicationId*/, 0 /*departmentId*/,
        lWspProfId,
        pszLocalFile, strFileName, strName, strComment,
        NULL /*version NULL=系统自动生成*/, FALSE /*bLeaveOut 上传后签入*/,
        0 /*ulFlags*/, szWork, MAX_PATH * 2, NULL);

    if (bOk)
        lDocId = lDoc;
    else
        strErr = GetLastErrorText();
    return bOk;
}

//--------------------------------------------------------------------------------------+
// 通用
//--------------------------------------------------------------------------------------+

CString GetLastErrorText()
{
    LPCWSTR psz = aaApi_GetLastErrorMessage();
    if (psz != NULL && psz[0] != _T('\0'))
        return CString(psz);

    LONG nId = aaApi_GetLastErrorId();
    CString str;
    str.Format(_T("错误码: %ld"), nId);
    return str;
}

BOOL OpenWithShell(LPCTSTR pszFile)
{
    HINSTANCE hInst = ShellExecute(NULL, _T("open"), pszFile, NULL, NULL, SW_SHOWNORMAL);
    return ((INT_PTR)hInst > 32);
}

CString GetFileName(LPCTSTR pszPath)
{
    CString str(pszPath);
    int nPos = str.ReverseFind(_T('\\'));
    int nPos2 = str.ReverseFind(_T('/'));
    if (nPos2 > nPos)
        nPos = nPos2;
    if (nPos >= 0)
        str = str.Mid(nPos + 1);
    return str;
}

CString GetFileFolder(LPCTSTR pszPath)
{
    CString str(pszPath);
    int nPos = str.ReverseFind(_T('\\'));
    int nPos2 = str.ReverseFind(_T('/'));
    if (nPos2 > nPos)
        nPos = nPos2;
    if (nPos >= 0)
        str = str.Left(nPos);
    return str;
}

// 追加一行带时间戳的日志（UTF-8）。pszTag 非空时输出 "[时间] [tag] 内容"。
void AppendLog(LPCTSTR pszLogFile, LPCTSTR pszTag, LPCTSTR pszLine)
{
    if (pszLogFile == NULL || pszLine == NULL)
        return;

    CreateDirRecursive(GetFileFolder(pszLogFile));

    FILE* f = NULL;
    if (_wfopen_s(&f, pszLogFile, L"a, ccs=UTF-8") == 0 && f != NULL)
    {
        CTime t = CTime::GetCurrentTime();
        CString strHead = t.Format(_T("%Y-%m-%d %H:%M:%S"));
        if (pszTag != NULL && pszTag[0] != _T('\0'))
            fwprintf(f, L"[%s] [%s] %s\n", (LPCTSTR)strHead, pszTag, pszLine);
        else
            fwprintf(f, L"[%s] %s\n", (LPCTSTR)strHead, pszLine);
        fclose(f);
    }
}

} // namespace PWHelper

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
    DWORD nLen = GetEnvironmentVariable(pszName, NULL, 0);
    if (nLen == 0)
        return CString();
    TCHAR* pBuf = new TCHAR[nLen];
    GetEnvironmentVariable(pszName, pBuf, nLen);
    CString str(pBuf);
    delete[] pBuf;
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
        for (int j = 0; j < 5; j++)
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

    // 逐段创建（跳过盘符）
    for (int i = 0; i <= strPath.GetLength(); i++)
    {
        TCHAR ch = (i < strPath.GetLength()) ? strPath[i] : _T('\\');
        if (ch != _T('\\'))
            continue;

        CString strSeg = strPath.Left(i);
        if (strSeg.IsEmpty())
            continue;
        // [修复] 仅跳过裸盘符 "D:"（长度==2）。原条件 GetLength()>=2 会把
        // "D:\xxx"、"D:\xxx\yyy" 等所有 D:\ 开头的段都误判为盘符跳过，
        // 导致整条路径一级目录都不会被创建，函数恒返回 FALSE。
        if (strSeg.GetLength() == 2 && strSeg[1] == _T(':'))
            continue;   // 盘符

        DWORD dwA2 = GetFileAttributes(strSeg);
        if (dwA2 == INVALID_FILE_ATTRIBUTES)
        {
            // 任一级创建失败立即返回，避免后续级联失败掩盖真实原因
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

BOOL DownloadDocument(LONG lProjectId, LONG lDocumentId, LPCTSTR pszWorkDir, CString& strOutFile)
{
    strOutFile.Empty();
    if (lDocumentId <= 0)
        return FALSE;
    if (pszWorkDir == NULL || !CreateDirRecursive(pszWorkDir))
        return FALSE;

    TCHAR szOut[MAX_PATH * 2] = { 0 };
    BOOL bOk = aaApi_CopyOutDocument(lProjectId, lDocumentId, pszWorkDir, szOut, MAX_PATH * 2);
    if (bOk)
        strOutFile = szOut;
    return bOk;
}

LONG EnumDocumentVersions(LONG lProjectId, LONG lDocumentId,
                          CArray<PWDocVersionItem, PWDocVersionItem&>& arrVersions)
{
    arrVersions.RemoveAll();
    if (lProjectId <= 0 || lDocumentId <= 0)
        return 0;

    // 先从任意版本定位到当前(最新)版本再枚举版本链：
    // SelectDocumentVersions 对非活动(历史)版本可能枚举不到。
    lDocumentId = GetLatestDocumentId(lProjectId, lDocumentId);
    if (lDocumentId <= 0)
        return 0;

    LONG nCount = aaApi_SelectDocumentVersions(lProjectId, lDocumentId);
    if (nCount <= 0)
        return nCount;

    for (LONG i = 0; i < nCount; i++)
    {
        PWDocVersionItem item;
        item.lDocumentId = aaApi_GetDocumentNumericProperty(DOC_PROP_ID, i);
        item.lVersionNo = aaApi_GetDocumentNumericProperty(DOC_PROP_VERSIONNO, i);
        item.lSize = aaApi_GetDocumentNumericProperty(DOC_PROP_SIZE, i);
        const WCHAR* psz = aaApi_GetDocumentStringProperty(DOC_PROP_VERSION, i);
        if (psz) item.strVersion = psz;
        psz = aaApi_GetDocumentStringProperty(DOC_PROP_FILE_UPDATE_TIME, i);
        if (psz) item.strUpdateTime = psz;
        arrVersions.Add(item);
    }

    // 按版本号从大到小排序（最新在前）
    for (INT_PTR i = 0; i + 1 < arrVersions.GetSize(); i++)
        for (INT_PTR j = i + 1; j < arrVersions.GetSize(); j++)
            if (arrVersions[i].lVersionNo < arrVersions[j].lVersionNo)
            {
                PWDocVersionItem t = arrVersions[i];
                arrVersions[i] = arrVersions[j];
                arrVersions[j] = t;
            }

    return arrVersions.GetSize();
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

    // 下载落到目标文件（链接模型）所在目录，CopyOut 实际写出的路径回传出来便于诊断。
    // 目标目录已有同名文件时 CopyOut 可能生成带序号的新文件（如 xxx_1.dgn），
    // 因此下载后把实际输出文件覆盖/移动到链接文件上，确保本地文件被真正更新。
    CString strOut;
    if (!DownloadDocument(lProjectId, lDocumentId, strFolder, strOut))
    {
        strErr = GetLastErrorText();
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

BOOL UploadNewVersion(LONG lProjectId, LONG lDocumentId, LPCTSTR pszLocalFile,
                      LPCTSTR pszWorkDir, const CString& strComment, CString& strErr)
{
    strErr.Empty();

    // 始终基于当前(最新)版本检入新版本：INI 中的 docid 可能指向历史版本
    lDocumentId = GetLatestDocumentId(lProjectId, lDocumentId);
    if (lDocumentId <= 0)
    {
        strErr = _T("无效的文档ID。");
        return FALSE;
    }

    // 检查写权限。[修复记录] aaApi_GetDocumentAccess 返回的是访问级别而非位掩码：
    //   AADMS_ACCESS_WRITE=可修改/可checkin，AADMS_ACCESS_READ=只读。
    //   原误用 AADMS_ACCESS_FWRITE(0x08) 位判断会误判只读，已改为 AADMS_ACCESS_WRITE。
    LONG lAccess = aaApi_GetDocumentAccess(lProjectId, lDocumentId, -1);
    if ((lAccess & AADMS_ACCESS_WRITE) == 0)
    {
        strErr = _T("当前用户对该文档无写入权限，无法上传。");
        return FALSE;
    }

    // 用临时子目录作为工作副本，避免 CheckOut 覆盖用户本地编辑过的文件
    CString strTempDir(pszWorkDir);
    if (!strTempDir.IsEmpty() && strTempDir[strTempDir.GetLength() - 1] != _T('\\'))
        strTempDir += _T('\\');
    strTempDir += _T(".pw_upload");
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

    // 3. 回传服务器，保留工作副本
    if (!aaApi_CheckInDocumentLeaveCopy(lProjectId, lDocumentId))
    {
        strErr = GetLastErrorText();
        return FALSE;
    }

    // 清理临时工作副本
    DeleteFile(szOut);
    return TRUE;
}

// [真机验证注意] aaApi_CreateDocument 在 SDK samples 中无使用参考，
// 仅按头文件文档实现；storageId/fileType/appId 传默认值(0)。
// 首次真机上传新文档时需确认服务器是否接受，如失败请调整参数。
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

} // namespace PWHelper

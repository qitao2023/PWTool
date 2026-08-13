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

//--------------------------------------------------------------------------------------+
// PW 操作
//--------------------------------------------------------------------------------------+

BOOL DownloadDocument(LONG lProjectId, LONG lDocumentId, LPCTSTR pszWorkDir, CString& strOutFile)
{
    strOutFile.Empty();
    if (pszWorkDir == NULL || !CreateDirRecursive(pszWorkDir))
        return FALSE;

    TCHAR szOut[MAX_PATH * 2] = { 0 };
    BOOL bOk = aaApi_CopyOutDocument(lProjectId, lDocumentId, pszWorkDir, szOut, MAX_PATH * 2);
    if (bOk)
        strOutFile = szOut;
    return bOk;
}

CString GetLatestVersionDate(LONG lProjectId, LONG lDocumentId)
{
    LONG nRes = aaApi_SelectDocument(lProjectId, lDocumentId);
    if (nRes != 1)
        return _T("");
    // [修复] 用 FILE_UPDATE_TIME(文件内容最后更新时间) 作为"最新版本"时间
    const WCHAR* psz = aaApi_GetDocumentStringProperty(DOC_PROP_FILE_UPDATE_TIME, 0);
    return (psz != NULL) ? CString(psz) : CString(_T(""));
}

CString GetLatestVersion(LONG lProjectId, LONG lDocumentId)
{
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


// PWExampleFroTSZ-VS2017Dlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "PWExampleFroTSZ-VS2017.h"
#include "PWExampleFroTSZ-VS2017Dlg.h"
#include "afxdialogex.h"
#include "PWHelper.h"
#include "DocListDlg.h"
#include "LinkMgrDlg.h"
#include "VersionListDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 链接流程诊断日志：追加写入 exe 目录\pw_link.log，便于排查"链接不到最新"问题
static void LogLink(LPCTSTR pszLine)
{
    PWHelper::AppendLog(PWHelper::GetAppBaseDir() + _T("\\pw_link.log"), NULL, pszLine);
}


// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
    CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_ABOUTBOX };
#endif

    protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
    DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CPWExampleFroTSZVS2017Dlg 对话框



CPWExampleFroTSZVS2017Dlg::CPWExampleFroTSZVS2017Dlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_PWEXAMPLEFROTSZVS2017_DIALOG, pParent)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CPWExampleFroTSZVS2017Dlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CPWExampleFroTSZVS2017Dlg, CDialogEx)
    ON_WM_SYSCOMMAND()
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_BN_CLICKED(IDC_BTN_PW_LOGIN, &CPWExampleFroTSZVS2017Dlg::OnBnClickedPwLogin)
    ON_BN_CLICKED(IDC_BTN_PW_LOGOUT, &CPWExampleFroTSZVS2017Dlg::OnBnClickedPwLogout)
    ON_BN_CLICKED(IDC_BTN_PW_OPEN, &CPWExampleFroTSZVS2017Dlg::OnBnClickedPwOpen)
    ON_BN_CLICKED(IDC_BTN_PW_LINK, &CPWExampleFroTSZVS2017Dlg::OnBnClickedPwLink)
    ON_BN_CLICKED(IDC_BTN_PW_LINKMGR, &CPWExampleFroTSZVS2017Dlg::OnBnClickedPwLinkMgr)
    ON_BN_CLICKED(IDC_BTN_PW_UPLOAD, &CPWExampleFroTSZVS2017Dlg::OnBnClickedPwUpload)
END_MESSAGE_MAP()


// CPWExampleFroTSZVS2017Dlg 消息处理程序

BOOL CPWExampleFroTSZVS2017Dlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // 把"关于..."菜单项添加到系统菜单
    ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
    ASSERT(IDM_ABOUTBOX < 0xF000);

    CMenu* pSysMenu = GetSystemMenu(FALSE);
    if (pSysMenu != nullptr)
    {
        CString strAboutMenu;
        if (strAboutMenu.LoadString(IDS_ABOUTBOX) && !strAboutMenu.IsEmpty())
        {
            pSysMenu->AppendMenu(MF_SEPARATOR);
            pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
        }
    }

    SetIcon(m_hIcon, TRUE);   // 设置大图标
    SetIcon(m_hIcon, FALSE);  // 设置小图标

    // PW 采用惰性登录：需要登录时由各功能按钮或"登录PW"按钮触发。
    // 启动时若已处于登录状态（如登录会话未过期），标题直接显示当前账号。
    UpdateLoginTitle();

    return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CPWExampleFroTSZVS2017Dlg::OnSysCommand(UINT nID, LPARAM lParam)
{
    if ((nID & 0xFFF0) == IDM_ABOUTBOX)
    {
        CAboutDlg dlgAbout;
        dlgAbout.DoModal();
    }
    else
    {
        CDialogEx::OnSysCommand(nID, lParam);
    }
}

void CPWExampleFroTSZVS2017Dlg::OnPaint()
{
    if (IsIconic())
    {
        CPaintDC dc(this);

        SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

        // 使图标在工作区矩形中居中
        int cxIcon = GetSystemMetrics(SM_CXICON);
        int cyIcon = GetSystemMetrics(SM_CYICON);
        CRect rect;
        GetClientRect(&rect);
        int x = (rect.Width() - cxIcon + 1) / 2;
        int y = (rect.Height() - cyIcon + 1) / 2;

        dc.DrawIcon(x, y, m_hIcon);
    }
    else
    {
        CDialogEx::OnPaint();
    }
}

// 窗口最小化时，系统拖动图标调用此函数取得光标
HCURSOR CPWExampleFroTSZVS2017Dlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}



void CPWExampleFroTSZVS2017Dlg::OnBnClickedPwLogin()
{
    if (PWHelper::EnsureLogin(this))
    {
        UpdateLoginTitle();
        AfxMessageBox(_T("已登录PW系统。"));
    }
}


// 退出登录：断开当前账号，便于切换其他账号
void CPWExampleFroTSZVS2017Dlg::OnBnClickedPwLogout()
{
    if (AfxMessageBox(_T("确定退出当前PW账号？"), MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    if (PWHelper::Logout())
    {
        UpdateLoginTitle();
        AfxMessageBox(_T("已退出登录，可切换到其他账号重新登录。"));
    }
    else
        AfxMessageBox(_T("退出登录失败：") + PWHelper::GetLastErrorText());
}


// 登录状态同步到窗口标题：已登录时标题显示当前账号，退出登录时恢复默认标题
void CPWExampleFroTSZVS2017Dlg::UpdateLoginTitle()
{
    // 首次调用时记录窗口默认标题（来自资源对话框标题），之后作为标题前缀
    if (m_strBaseTitle.IsEmpty())
        GetWindowText(m_strBaseTitle);

    CString strTitle = m_strBaseTitle;
    if (PWHelper::IsLoggedIn())
    {
        CString strUser = PWHelper::GetCurrentUserName();
        if (!strUser.IsEmpty())
        {
            strTitle += _T(" - 已登录：");
            strTitle += strUser;
        }
    }
    SetWindowText(strTitle);
}


// 打开PW：下载到本地 model 目录并打开模型文件
void CPWExampleFroTSZVS2017Dlg::OnBnClickedPwOpen()
{
    if (!PWHelper::EnsureLogin(this))
        return;
    UpdateLoginTitle();   // 本次可能刚完成登录，同步标题账号

    CDocListDlg dlg(CDocListDlg::MODE_OPEN, this);
    if (dlg.DoModal() != IDOK)
        return;
    if (dlg.m_arrSelected.GetSize() == 0)
        return;

    const PWHelper::PWDocItem& item = dlg.m_arrSelected.GetAt(0);

    CString strDir = dlg.m_strLocalPath;
    if (!PWHelper::CreateDirRecursive(strDir))
    {
        AfxMessageBox(_T("创建本地目录失败：") + strDir);
        return;
    }

    // 决定要打开的版本：
    //  - 若用户在文档列表的"历史版本"列已选定版本，直接使用；
    //  - 否则弹出版本列表让用户选择（默认最新，可回退到历史版本）。
    // [修复] 原仅在版本数>0时才弹窗：单版本文档(SelectDocumentVersions返回0)会静默跳过，
    // 导致版本界面从未出现。现改为总是弹出，保证界面可见；枚举失败时明确报错。
    LONG lDocId = item.lDocumentId;
    if (item.lChosenDocId > 0)
    {
        lDocId = item.lChosenDocId;
    }
    else
    {
        CArray<PWHelper::PWDocVersionItem, PWHelper::PWDocVersionItem&> arrVersions;
        LONG nVer = PWHelper::EnumSameNameDocuments(item.lProjectId, item.lDocumentId, arrVersions);
        if (nVer < 0)
        {
            AfxMessageBox(_T("获取版本列表失败：") + PWHelper::GetLastErrorText());
            return;
        }
        // 打开前本地尚无"当前版本"概念，curVersionDate 传空，仅标记最新
        {
            CDlgVersionList dlgVer(item.lProjectId, item.lDocumentId, _T(""), this);
            if (dlgVer.DoModal() != IDOK)
                return;
            lDocId = dlgVer.m_sel.lDocumentId;
        }
    }
    // 版本选择异常时退回最新版本：列表中的 docid 可能已落后于服务器
    if (lDocId <= 0)
        lDocId = PWHelper::GetLatestDocumentId(item.lProjectId, item.lDocumentId);

    CString strOutFile;
    CString strDlErr;
    if (!PWHelper::DownloadDocument(item.lProjectId, lDocId, strDir, strOutFile, &strDlErr))
    {
        AfxMessageBox(_T("下载失败：") + (strDlErr.IsEmpty() ? PWHelper::GetLastErrorText() : strDlErr));
        return;
    }

    // 保存 PW 地址来源：记录所选版本的 docid、版本号与更新时间，精确对应"本地文件同步自哪个版本"
    PWHelper::SavePwAddrOfDocument(strDir, strOutFile, item.lProjectId, lDocId, FALSE, _T(""), strOutFile);

    // 打开本地模型文件
    PWHelper::OpenWithShell(strOutFile);

    m_strCurrentModelPath = strOutFile;

    CString strMsg;
    strMsg.Format(_T("已下载并打开：\n%s"), (LPCTSTR)strOutFile);
    AfxMessageBox(strMsg);
}


// 链接：多选下载到本地 LinkModel 目录并保存PW地址来源
void CPWExampleFroTSZVS2017Dlg::OnBnClickedPwLink()
{
    if (!PWHelper::EnsureLogin(this))
        return;
    UpdateLoginTitle();   // 本次可能刚完成登录，同步标题账号

    CDocListDlg dlg(CDocListDlg::MODE_LINK, this);
    if (dlg.DoModal() != IDOK)
        return;
    if (dlg.m_arrSelected.GetSize() == 0)
        return;

    CString strDir = dlg.m_strLocalPath;
    if (!PWHelper::CreateDirRecursive(strDir))
    {
        AfxMessageBox(_T("创建本地目录失败：") + strDir);
        return;
    }

    int nOk = 0, nFail = 0;
    CString strFailInfo;   // 汇总各文件失败原因
    for (INT_PTR i = 0; i < dlg.m_arrSelected.GetSize(); i++)
    {
        const PWHelper::PWDocItem& item = dlg.m_arrSelected.GetAt(i);

        CString strFileName = item.strFileName.IsEmpty() ? item.strName : item.strFileName;
        CString strTarget = strDir + _T("\\") + strFileName;

        // 本地已有该文件时询问是否更新
        if (GetFileAttributes(strTarget) != INVALID_FILE_ATTRIBUTES)
        {
            CString strAsk;
            strAsk.Format(_T("本地已有文件\"%s\"，是否更新为所选版本？"), (LPCTSTR)strFileName);
            if (AfxMessageBox(strAsk, MB_YESNO | MB_ICONQUESTION) != IDYES)
                continue;
        }

        // 链接取用户经"历史版本"列选定的版本；未选定则用最新（列表中的 docid 可能已落后于服务器）
        LONG lDocId = (item.lChosenDocId > 0)
            ? item.lChosenDocId
            : PWHelper::GetLatestDocumentId(item.lProjectId, item.lDocumentId);
        if (lDocId <= 0)
            lDocId = item.lDocumentId;

        CString strOutFile;
        CString strDlErr;
        if (!PWHelper::DownloadDocument(item.lProjectId, lDocId, strDir, strOutFile, &strDlErr))
        {
            nFail++;
            CString strErr = strDlErr.IsEmpty() ? PWHelper::GetLastErrorText() : strDlErr;
            strFailInfo += strFileName + _T("：") + strErr + _T("\n");
            CString strLog;
            strLog.Format(_T("链接失败: %s docid=%ld 原因=%s"), (LPCTSTR)strFileName, lDocId, (LPCTSTR)strErr);
            LogLink(strLog);
            continue;
        }

        // 记录下载结果（含文件大小，便于核对是否真下到最新版本）
        {
            CFileStatus fs;
            ULONGLONG nSize = 0;
            if (CFile::GetStatus(strOutFile, fs))
                nSize = fs.m_size;
            CString strLog;
            strLog.Format(_T("链接成功: %s docid=%ld 大小=%I64u"), (LPCTSTR)strFileName, lDocId, nSize);
            LogLink(strLog);
        }

        // 记录 PW 地址来源（版本号/时间/项目名由 SavePwAddrOfDocument 统一查询）
        PWHelper::SavePwAddrOfDocument(strDir, strOutFile, item.lProjectId, lDocId, TRUE, _T(""), strOutFile);
        nOk++;
    }

    // 登记用户选择的链接目录，链接管理按此目录显示/更新
    PWHelper::RegisterLinkFolder(strDir);

    CString strMsg;
    strMsg.Format(_T("链接完成：成功 %d 个，失败 %d 个。\n模型已下载到：\n%s"),
        nOk, nFail, (LPCTSTR)strDir);
    if (!strFailInfo.IsEmpty())
        strMsg += _T("\n\n失败原因：\n") + strFailInfo;
    AfxMessageBox(strMsg);
}


// 链接管理
void CPWExampleFroTSZVS2017Dlg::OnBnClickedPwLinkMgr()
{
    CDlgLinkMgr dlg(this);
    dlg.DoModal();
}


// 上传：有PW地址来源→直接检出并弹PW检入框生成新版本；无地址→选目标目录创建新文档。
// [简化] 不再弹工具自己的上传框，版本说明在PW检入框里填。
void CPWExampleFroTSZVS2017Dlg::OnBnClickedPwUpload()
{
    CString strModel = m_strCurrentModelPath;

    if (strModel.IsEmpty())
    {
        CFileDialog dlg(TRUE, _T("dwg"), NULL,
            OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
            _T("模型文件 (*.dwg;*.dgn;*.tsmgn;*.txt)|*.dwg;*.dgn;*.tsmgn;*.txt|所有文件 (*.*)|*.*||"), this);
        if (dlg.DoModal() != IDOK)
            return;
        strModel = dlg.GetPathName();
    }

    if (!PWHelper::EnsureLogin(this))
        return;
    UpdateLoginTitle();   // 本次可能刚完成登录，同步标题账号

    CString strFolder = PWHelper::GetFileFolder(strModel);
    CString strFileName = PWHelper::GetFileName(strModel);

    PWHelper::PWAddrInfo addr;
    BOOL bHasAddr = PWHelper::LoadPwAddr(strFolder, strModel, addr);

    // 确定目标项目与文档：
    //  - 有 PW 地址来源：直接对该文档上传新版本；
    //  - 无地址：弹 PW 目录选择框；目标目录若已有同名文档（他人先传过），
    //    改为对该文档上传新版本（版本继续往后排），否则创建新文档。
    LONG lProjectId = 0;
    LONG lDocId = 0;
    if (bHasAddr && addr.lDocumentId > 0)
    {
        lProjectId = addr.lProjectId;
        lDocId = addr.lDocumentId;
    }
    else
    {
        lProjectId = aaApi_SelectProjectDlg(this->m_hWnd, _T("请选择上传目标目录"), 0);
        if (lProjectId <= 0)
            return;
        lDocId = PWHelper::FindDocumentIdByName(lProjectId, strFileName);
    }

    if (lDocId > 0)
    {
        // 更新已有文档版本：直接检出 + 弹PW检入框（跳过工具上传框），版本说明在检入框里填
        CString strErr;
        BOOL bNewVer = FALSE;
        if (!PWHelper::UploadNewVersion(this->GetSafeHwnd(),
            lProjectId, lDocId, strModel, strFolder, _T(""), strErr, &bNewVer))
        {
            AfxMessageBox(_T("上传失败：") + strErr);
            return;
        }

        // 记录最新版本 docid 与 PW 地址来源（版本号/时间/项目名由 helper 统一查询）
        LONG lNewDocId = PWHelper::GetLatestDocumentId(lProjectId, lDocId);
        if (lNewDocId <= 0)
            lNewDocId = lDocId;
        // 有地址时保留原 INI 中的链接状态与版本说明
        PWHelper::SavePwAddrOfDocument(strFolder, strFileName, lProjectId, lNewDocId,
            bHasAddr ? addr.bLink : FALSE, bHasAddr ? addr.strComment : _T(""));

        // 诊断：上传后把服务器记录的每版创建人/修改人原始字段写到 exe 目录 pw_version_dump.txt
        PWHelper::DumpDocumentVersionsToFile(lProjectId, lNewDocId);

        CString strMsg;
        if (bHasAddr)
            strMsg = bNewVer ? _T("上传成功，已生成新版本。") : _T("上传成功。");
        else
            strMsg = _T("上传成功，已作为新版本保存PW地址来源。");
        AfxMessageBox(strMsg);
    }
    else
    {
        // 目标目录无同名文档：作为新文档创建
        LONG lNewDocId = 0;
        CString strErr;
        if (!PWHelper::CreateNewDocument(lProjectId, strModel, _T(""), lNewDocId, strErr))
        {
            AfxMessageBox(_T("上传失败：") + strErr);
            return;
        }

        PWHelper::SavePwAddrOfDocument(strFolder, strFileName, lProjectId, lNewDocId, FALSE);
        AfxMessageBox(_T("上传成功，已作为新文档保存PW地址来源。"));
    }
}

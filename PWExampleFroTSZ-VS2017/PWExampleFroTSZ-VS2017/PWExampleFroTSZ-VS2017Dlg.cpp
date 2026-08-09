
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
#include "UploadDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


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
	ON_BN_CLICKED(IDC_BTN_PW_OPEN, &CPWExampleFroTSZVS2017Dlg::OnBnClickedPwOpen)
	ON_BN_CLICKED(IDC_BTN_PW_LINK, &CPWExampleFroTSZVS2017Dlg::OnBnClickedPwLink)
	ON_BN_CLICKED(IDC_BTN_PW_LINKMGR, &CPWExampleFroTSZVS2017Dlg::OnBnClickedPwLinkMgr)
	ON_BN_CLICKED(IDC_BTN_PW_UPLOAD, &CPWExampleFroTSZVS2017Dlg::OnBnClickedPwUpload)
END_MESSAGE_MAP()


// CPWExampleFroTSZVS2017Dlg 消息处理程序

BOOL CPWExampleFroTSZVS2017Dlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码

	// PW 采用惰性登录：需要登录时由各功能按钮或"登录PW"按钮触发

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

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CPWExampleFroTSZVS2017Dlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CPWExampleFroTSZVS2017Dlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CPWExampleFroTSZVS2017Dlg::OnBnClickedPwLogin()
{
	if (PWHelper::EnsureLogin(this))
		AfxMessageBox(_T("已登录PW系统。"));
}


// 打开PW：下载到本地 model 目录并打开模型文件
void CPWExampleFroTSZVS2017Dlg::OnBnClickedPwOpen()
{
	if (!PWHelper::EnsureLogin(this))
		return;

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

	CString strOutFile;
	if (!PWHelper::DownloadDocument(item.lProjectId, item.lDocumentId, strDir, strOutFile))
	{
		AfxMessageBox(_T("下载失败：") + PWHelper::GetLastErrorText());
		return;
	}

	// 保存 PW 地址来源
	PWHelper::PWAddrInfo info;
	info.strDatasource = PWHelper::GetDatasourceName();
	info.lProjectId = item.lProjectId;
	info.lDocumentId = item.lDocumentId;
	info.strVersionDate = PWHelper::GetLatestVersionDate(item.lProjectId, item.lDocumentId);
	info.bLink = FALSE;
	aaApi_SelectProject(item.lProjectId);
	LPCWSTR psz = aaApi_GetProjectStringProperty(PROJ_PROP_NAME, 0);
	if (psz != NULL)
		info.strProjectName = psz;
	PWHelper::SavePwAddr(strDir, strOutFile, info);

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
	for (INT_PTR i = 0; i < dlg.m_arrSelected.GetSize(); i++)
	{
		const PWHelper::PWDocItem& item = dlg.m_arrSelected.GetAt(i);

		CString strFileName = item.strFileName.IsEmpty() ? item.strName : item.strFileName;
		CString strTarget = strDir + _T("\\") + strFileName;

		// 本地已有该文件时询问是否更新
		if (GetFileAttributes(strTarget) != INVALID_FILE_ATTRIBUTES)
		{
			CString strAsk;
			strAsk.Format(_T("本地已有文件\"%s\"，是否更新为最新版本？"), (LPCTSTR)strFileName);
			if (AfxMessageBox(strAsk, MB_YESNO | MB_ICONQUESTION) != IDYES)
				continue;
		}

		CString strOutFile;
		if (!PWHelper::DownloadDocument(item.lProjectId, item.lDocumentId, strDir, strOutFile))
		{
			nFail++;
			continue;
		}

		PWHelper::PWAddrInfo info;
		info.strDatasource = PWHelper::GetDatasourceName();
		info.lProjectId = item.lProjectId;
		info.lDocumentId = item.lDocumentId;
		info.strVersionDate = PWHelper::GetLatestVersionDate(item.lProjectId, item.lDocumentId);
		info.bLink = TRUE;
		aaApi_SelectProject(item.lProjectId);
		LPCWSTR psz = aaApi_GetProjectStringProperty(PROJ_PROP_NAME, 0);
		if (psz != NULL)
			info.strProjectName = psz;
		PWHelper::SavePwAddr(strDir, strOutFile, info);
		nOk++;
	}

	CString strMsg;
	strMsg.Format(_T("链接完成：成功 %d 个，失败 %d 个。\n模型已下载到：\n%s\n\n注：独立演示程序不包含探索者CAD的\"插入当前模型\"功能。"),
		nOk, nFail, (LPCTSTR)strDir);
	AfxMessageBox(strMsg);
}


// 链接管理
void CPWExampleFroTSZVS2017Dlg::OnBnClickedPwLinkMgr()
{
	CDlgLinkMgr dlg(this);
	dlg.DoModal();
}


// 上传：按PW地址来源上传当前模型
void CPWExampleFroTSZVS2017Dlg::OnBnClickedPwUpload()
{
	if (m_strCurrentModelPath.IsEmpty())
	{
		AfxMessageBox(_T("请先通过\"打开PW\"打开模型文件，再执行上传。"));
		return;
	}

	CDlgUpload dlg(m_strCurrentModelPath, this);
	dlg.DoModal();
}

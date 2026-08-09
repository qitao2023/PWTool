// LinkMgrDlg.cpp: 链接管理对话框实现

#include "pch.h"
#include "framework.h"
#include "PWExampleFroTSZ-VS2017.h"
#include "LinkMgrDlg.h"
#include "PWHelper.h"

#include <afxtempl.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// 本地文件版本 = 文件修改时间
static CString GetLocalVersion(LPCTSTR pszPath)
{
	CFileStatus fs;
	if (CFile::GetStatus(pszPath, fs))
		return fs.m_mtime.Format(_T("%Y-%m-%d %H:%M"));
	return _T("--");
}

BEGIN_MESSAGE_MAP(CDlgLinkMgr, CDialogEx)
	ON_BN_CLICKED(IDC_BTN_ADD, &CDlgLinkMgr::OnBnClickedAdd)
	ON_BN_CLICKED(IDC_BTN_DEL, &CDlgLinkMgr::OnBnClickedDel)
	ON_BN_CLICKED(IDC_BTN_LINK, &CDlgLinkMgr::OnBnClickedLink)
	ON_BN_CLICKED(IDC_BTN_UNLINK, &CDlgLinkMgr::OnBnClickedUnlink)
	ON_BN_CLICKED(IDC_BTN_UPDATE, &CDlgLinkMgr::OnBnClickedUpdate)
END_MESSAGE_MAP()


CDlgLinkMgr::CDlgLinkMgr(CWnd* pParent)
	: CDialogEx(IDD_LINKMGR_DLG, pParent)
{
}

CDlgLinkMgr::~CDlgLinkMgr()
{
}

void CDlgLinkMgr::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_LINKMODELS, m_list);
}

BOOL CDlgLinkMgr::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	m_list.InsertColumn(0, _T("文件名"), LVCFMT_LEFT, 170);
	m_list.InsertColumn(1, _T("当前版本"), LVCFMT_LEFT, 90);
	m_list.InsertColumn(2, _T("最新版本"), LVCFMT_LEFT, 110);
	m_list.InsertColumn(3, _T("状态"), LVCFMT_LEFT, 60);
	m_list.InsertColumn(4, _T("PW来源"), LVCFMT_LEFT, 60);

	ReloadList();
	if (PWHelper::IsLoggedIn())
		DetectLatestVersions();
	else
		SetStatusText(_T("未登录PW系统，最新版本未检测。"));

	return TRUE;
}

void CDlgLinkMgr::ReloadList()
{
	m_list.DeleteAllItems();
	m_arrItems.RemoveAll();

	CString strDir = PWHelper::GetLinkModelDir();
	PWHelper::CreateDirRecursive(strDir);

	// 1. INI 中记录的节名（文件名）
	CStringArray arrIniFiles;
	BOOL bHasIni = PWHelper::EnumPwAddrSections(strDir, arrIniFiles);

	// 2. 扫描 LinkModel 目录磁盘文件（排除 PWAddress.ini）
	CStringArray arrDiskFiles;
	WIN32_FIND_DATA fd;
	HANDLE hFind = FindFirstFile(strDir + _T("\\*"), &fd);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				CString strName(fd.cFileName);
				if (strName.CompareNoCase(_T("PWAddress.ini")) != 0)
					arrDiskFiles.Add(strName);
			}
		} while (FindNextFile(hFind, &fd));
		FindClose(hFind);
	}

	// 3. 合并：磁盘文件 + INI 节
	for (INT_PTR i = 0; i < arrDiskFiles.GetSize(); i++)
	{
		LinkItem item;
		item.strFileName = arrDiskFiles[i];
		item.strLocalPath = strDir + _T("\\") + item.strFileName;
		item.bHasAddr = PWHelper::LoadPwAddr(strDir, item.strFileName, item.addr);
		item.bLink = item.addr.bLink;
		m_arrItems.Add(item);
	}

	for (INT_PTR i = 0; i < arrIniFiles.GetSize(); i++)
	{
		BOOL bFound = FALSE;
		for (INT_PTR j = 0; j < m_arrItems.GetSize(); j++)
		{
			if (m_arrItems[j].strFileName.CompareNoCase(arrIniFiles[i]) == 0)
			{
				bFound = TRUE;
				break;
			}
		}
		if (bFound)
			continue;

		LinkItem item;
		item.strFileName = arrIniFiles[i];
		item.strLocalPath = strDir + _T("\\") + item.strFileName;
		item.bHasAddr = PWHelper::LoadPwAddr(strDir, item.strFileName, item.addr);
		item.bLink = item.addr.bLink;
		m_arrItems.Add(item);
	}

	// 4. 填充列表
	for (INT_PTR i = 0; i < m_arrItems.GetSize(); i++)
	{
		LinkItem& it = m_arrItems[i];
		int nRow = m_list.InsertItem(m_list.GetItemCount(), it.strFileName);
		m_list.SetItemText(nRow, 1, GetLocalVersion(it.strLocalPath));
		m_list.SetItemText(nRow, 2, _T("未检测"));
		m_list.SetItemText(nRow, 3, it.bLink ? _T("已链接") : _T("未链接"));
		m_list.SetItemText(nRow, 4, it.bHasAddr ? _T("PW") : _T("本地"));
	}
}

void CDlgLinkMgr::DetectLatestVersions()
{
	if (!PWHelper::IsLoggedIn())
	{
		SetStatusText(_T("未登录PW系统，最新版本未检测。"));
		return;
	}

	int nUpdated = 0;
	for (INT_PTR i = 0; i < m_arrItems.GetSize(); i++)
	{
		LinkItem& it = m_arrItems[i];
		CString strDate;
		if (it.bHasAddr && it.addr.lDocumentId > 0)
			strDate = PWHelper::GetLatestVersionDate(it.addr.lProjectId, it.addr.lDocumentId);
		m_list.SetItemText((int)i, 2, strDate.IsEmpty() ? _T("未检测") : strDate);
		if (!strDate.IsEmpty())
			nUpdated++;
	}

	CString str;
	str.Format(_T("已登录PW系统，检测到 %d 个链接模型的最新版本。"), nUpdated);
	SetStatusText(str);
}

int CDlgLinkMgr::GetSelectedRow() const
{
	POSITION pos = m_list.GetFirstSelectedItemPosition();
	if (pos == NULL)
		return -1;
	return m_list.GetNextSelectedItem(pos);
}

void CDlgLinkMgr::SetStatusText(LPCTSTR psz)
{
	SetDlgItemText(IDC_STATIC_STATUS, psz);
}

void CDlgLinkMgr::OnBnClickedAdd()
{
	CFileDialog dlg(TRUE, _T("dwg"), NULL,
		OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
		_T("模型文件 (*.dwg;*.dgn;*.dxf;*.model)|*.dwg;*.dgn;*.dxf;*.model|所有文件 (*.*)|*.*||"),
		this);
	if (dlg.DoModal() != IDOK)
		return;

	CString strSrc = dlg.GetPathName();
	CString strFileName = PWHelper::GetFileName(strSrc);

	CString strDir = PWHelper::GetLinkModelDir();
	PWHelper::CreateDirRecursive(strDir);
	CString strDst = strDir + _T("\\") + strFileName;

	if (GetFileAttributes(strDst) != INVALID_FILE_ATTRIBUTES)
	{
		if (AfxMessageBox(_T("链接列表中已有同名文件，是否覆盖？"),
			MB_YESNO | MB_ICONQUESTION) != IDYES)
			return;
	}
	if (!CopyFile(strSrc, strDst, FALSE))
	{
		AfxMessageBox(_T("添加失败，复制文件出错。"));
		return;
	}

	// 写 INI（无 PW 地址，未链接）
	PWHelper::PWAddrInfo info;
	info.bLink = FALSE;
	PWHelper::SavePwAddr(strDir, strFileName, info);

	ReloadList();
	SetStatusText(_T("已添加本地模型。"));
}

void CDlgLinkMgr::OnBnClickedDel()
{
	int nRow = GetSelectedRow();
	if (nRow < 0)
	{
		AfxMessageBox(_T("请先选择一个链接模型。"));
		return;
	}

	LinkItem& it = m_arrItems[nRow];

	CString strMsg;
	strMsg.Format(_T("确定要删除链接模型\"%s\"吗？%s"),
		(LPCTSTR)it.strFileName,
		it.bLink ? _T("\n该模型当前为已链接状态，将同时卸载链接。") : _T(""));
	if (AfxMessageBox(strMsg, MB_YESNO | MB_ICONQUESTION) != IDYES)
		return;

	if (GetFileAttributes(it.strLocalPath) != INVALID_FILE_ATTRIBUTES)
	{
		if (!DeleteFile(it.strLocalPath))
		{
			AfxMessageBox(_T("删除本地文件失败，文件可能正被占用。"));
			return;
		}
	}

	PWHelper::DeletePwAddr(PWHelper::GetLinkModelDir(), it.strFileName);
	ReloadList();
	SetStatusText(_T("已删除。"));
}

void CDlgLinkMgr::OnBnClickedLink()
{
	int nRow = GetSelectedRow();
	if (nRow < 0)
	{
		AfxMessageBox(_T("请先选择一个链接模型。"));
		return;
	}

	LinkItem& it = m_arrItems[nRow];
	if (it.bLink)
	{
		AfxMessageBox(_T("该模型当前已处于链接状态。"));
		return;
	}

	if (it.bHasAddr && it.addr.lDocumentId > 0)
	{
		// 有 PW 地址：登录后重新下载最新版本再链接
		if (!PWHelper::EnsureLogin(this))
			return;

		CString strOut;
		if (!PWHelper::DownloadDocument(it.addr.lProjectId, it.addr.lDocumentId,
			PWHelper::GetLinkModelDir(), strOut))
		{
			AfxMessageBox(_T("重新下载最新版本失败：") + PWHelper::GetLastErrorText());
			return;
		}

		CString strNewDate = PWHelper::GetLatestVersionDate(it.addr.lProjectId, it.addr.lDocumentId);
		it.addr.bLink = TRUE;
		if (!strNewDate.IsEmpty())
			it.addr.strVersionDate = strNewDate;
		PWHelper::SavePwAddr(PWHelper::GetLinkModelDir(), it.strFileName, it.addr);
	}
	else
	{
		// 无 PW 地址的本地模型，仅标记已链接
		PWHelper::SetPwAddrLinkState(PWHelper::GetLinkModelDir(), it.strFileName, TRUE);
	}

	ReloadList();
	SetStatusText(_T("已重新链接。"));
}

void CDlgLinkMgr::OnBnClickedUnlink()
{
	int nRow = GetSelectedRow();
	if (nRow < 0)
	{
		AfxMessageBox(_T("请先选择一个链接模型。"));
		return;
	}

	LinkItem& it = m_arrItems[nRow];
	if (!it.bLink)
	{
		AfxMessageBox(_T("该模型当前未处于链接状态。"));
		return;
	}

	PWHelper::SetPwAddrLinkState(PWHelper::GetLinkModelDir(), it.strFileName, FALSE);
	ReloadList();
	SetStatusText(_T("已卸载链接。"));
}

void CDlgLinkMgr::OnBnClickedUpdate()
{
	int nRow = GetSelectedRow();
	if (nRow < 0)
	{
		AfxMessageBox(_T("请先选择一个链接模型。"));
		return;
	}

	LinkItem& it = m_arrItems[nRow];
	if (!it.bHasAddr || it.addr.lDocumentId <= 0)
	{
		AfxMessageBox(_T("该模型没有PW地址来源，无法更新。"));
		return;
	}

	if (!PWHelper::EnsureLogin(this))
		return;

	CString strOut;
	if (!PWHelper::DownloadDocument(it.addr.lProjectId, it.addr.lDocumentId,
		PWHelper::GetLinkModelDir(), strOut))
	{
		AfxMessageBox(_T("更新失败：") + PWHelper::GetLastErrorText());
		return;
	}

	CString strNewDate = PWHelper::GetLatestVersionDate(it.addr.lProjectId, it.addr.lDocumentId);
	it.addr.strVersionDate = strNewDate;
	PWHelper::SavePwAddr(PWHelper::GetLinkModelDir(), it.strFileName, it.addr);

	m_list.SetItemText(nRow, 1, GetLocalVersion(strOut));
	m_list.SetItemText(nRow, 2, strNewDate.IsEmpty() ? _T("未检测") : strNewDate);
	SetStatusText(_T("更新完成。"));
}

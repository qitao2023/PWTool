// LinkMgrDlg.cpp: 链接管理对话框实现

#include "pch.h"
#include "framework.h"
#include "PWExampleFroTSZ-VS2017.h"
#include "LinkMgrDlg.h"
#include "VersionListDlg.h"
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
		return fs.m_mtime.Format(_T("%Y-%m-%d %H:%M:%S"));
	return _T("--");
}

// 更新流程诊断日志：追加写入 LinkModel\pw_update.log，便于排查"更新不到最新"问题
static void LogUpdate(LPCTSTR pszLine)
{
	CString strDir = PWHelper::GetLinkModelDir();
	PWHelper::CreateDirRecursive(strDir);
	CString strPath = strDir + _T("\\pw_update.log");

	FILE* f = NULL;
	if (_wfopen_s(&f, strPath, L"a, ccs=UTF-8") == 0 && f != NULL)
	{
		CTime t = CTime::GetCurrentTime();
		CString strHead = t.Format(_T("%Y-%m-%d %H:%M:%S"));
		fwprintf(f, L"[%s] %s\n", (LPCTSTR)strHead, pszLine);
		fclose(f);
	}
}

// "历史版本"列索引：点击该列弹出版本选择
static const int COL_HISTVER = 5;

BEGIN_MESSAGE_MAP(CDlgLinkMgr, CDialogEx)
	ON_BN_CLICKED(IDC_BTN_ADD, &CDlgLinkMgr::OnBnClickedAdd)
	ON_BN_CLICKED(IDC_BTN_DEL, &CDlgLinkMgr::OnBnClickedDel)
	ON_BN_CLICKED(IDC_BTN_LINK, &CDlgLinkMgr::OnBnClickedLink)
	ON_BN_CLICKED(IDC_BTN_UNLINK, &CDlgLinkMgr::OnBnClickedUnlink)
	ON_BN_CLICKED(IDC_BTN_UPDATE, &CDlgLinkMgr::OnBnClickedUpdate)
	ON_NOTIFY(NM_CLICK, IDC_LIST_LINKMODELS, &CDlgLinkMgr::OnNMClickList)
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
	m_list.InsertColumn(COL_HISTVER, _T("历史版本"), LVCFMT_CENTER, 70);

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

	// 扫描所有已登记的链接目录（链接时用户可自定义下载目录，默认也含 LinkModel）
	CStringArray arrFolders;
	PWHelper::EnumLinkFolders(arrFolders);

	for (INT_PTR k = 0; k < arrFolders.GetSize(); k++)
	{
		CString strDir = arrFolders[k];
		PWHelper::CreateDirRecursive(strDir);

		// 1. INI 中记录的节名（文件名）
		CStringArray arrIniFiles;
		PWHelper::EnumPwAddrSections(strDir, arrIniFiles);

		// 2. 扫描目录磁盘文件（排除 PWAddress.ini）
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

		// 3. 合并：磁盘文件 + INI 节（同名文件在多个目录时取最先扫描到的）
		for (INT_PTR i = 0; i < arrDiskFiles.GetSize(); i++)
		{
			if (FindItem(arrDiskFiles[i]) >= 0)
				continue;
			LinkItem item;
			item.strFileName = arrDiskFiles[i];
			item.strLocalPath = strDir + _T("\\") + item.strFileName;
			item.bHasAddr = PWHelper::LoadPwAddr(strDir, item.strFileName, item.addr);
			item.bLink = item.addr.bLink;
			m_arrItems.Add(item);
		}

		for (INT_PTR i = 0; i < arrIniFiles.GetSize(); i++)
		{
			if (FindItem(arrIniFiles[i]) >= 0)
				continue;
			LinkItem item;
			item.strFileName = arrIniFiles[i];
			item.strLocalPath = strDir + _T("\\") + item.strFileName;
			item.bHasAddr = PWHelper::LoadPwAddr(strDir, item.strFileName, item.addr);
			item.bLink = item.addr.bLink;
			m_arrItems.Add(item);
		}
	}

	// 4. 填充列表
	for (INT_PTR i = 0; i < m_arrItems.GetSize(); i++)
	{
		LinkItem& it = m_arrItems[i];
		int nRow = m_list.InsertItem(m_list.GetItemCount(), it.strFileName);
		// 当前版本优先取 INI 记录的 PW 版本时间（本地文件修改时间不反映它对应服务器的哪个版本，
		// 本地另存/复制后 mtime 会变但"同步自哪个版本"不变）；无 PW 地址或 INI 无记录时退回文件修改时间。
		CString strCur = (it.bHasAddr && !it.addr.strVersionDate.IsEmpty())
			? it.addr.strVersionDate : GetLocalVersion(it.strLocalPath);
		m_list.SetItemText(nRow, 1, strCur);
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

int CDlgLinkMgr::FindItem(LPCTSTR pszFileName) const
{
	for (INT_PTR i = 0; i < m_arrItems.GetSize(); i++)
	{
		if (m_arrItems[i].strFileName.CompareNoCase(pszFileName) == 0)
			return (int)i;
	}
	return -1;
}

void CDlgLinkMgr::SetStatusText(LPCTSTR psz)
{
	SetDlgItemText(IDC_STATIC_STATUS, psz);
}

void CDlgLinkMgr::OnBnClickedAdd()
{
	CFileDialog dlg(TRUE, _T("dwg"), NULL,
		OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
		_T("模型文件 (*.dwg;*.dgn;*.tsmgn;*.txt)|*.dwg;*.dgn;*.tsmgn;*.txt|所有文件 (*.*)|*.*||"),
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

	PWHelper::DeletePwAddr(PWHelper::GetFileFolder(it.strLocalPath), it.strFileName);
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

		// 解析当前最新版本 docid：INI 中记录的 docid 可能指向历史版本
		LONG lDocId = PWHelper::GetLatestDocumentId(it.addr.lProjectId, it.addr.lDocumentId);
		if (lDocId <= 0)
			lDocId = it.addr.lDocumentId;

		CString strErr;
		if (!PWHelper::DownloadAndReplace(it.addr.lProjectId, lDocId,
			it.strLocalPath, strErr))
		{
			AfxMessageBox(_T("重新下载最新版本失败：") + strErr);
			return;
		}

		CString strNewDate = PWHelper::GetLatestVersionDate(it.addr.lProjectId, lDocId);
		it.addr.bLink = TRUE;
		it.addr.lDocumentId = lDocId;
		if (!strNewDate.IsEmpty())
			it.addr.strVersionDate = strNewDate;
		PWHelper::SavePwAddr(PWHelper::GetFileFolder(it.strLocalPath), it.strFileName, it.addr);
	}
	else
	{
		// 无 PW 地址的本地模型，仅标记已链接
		PWHelper::SetPwAddrLinkState(PWHelper::GetFileFolder(it.strLocalPath), it.strFileName, TRUE);
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

	PWHelper::SetPwAddrLinkState(PWHelper::GetFileFolder(it.strLocalPath), it.strFileName, FALSE);
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

	// 解析当前最新版本 docid：INI 中记录的 docid 可能指向历史版本
	LONG lDocId = PWHelper::GetLatestDocumentId(it.addr.lProjectId, it.addr.lDocumentId);
	if (lDocId <= 0)
		lDocId = it.addr.lDocumentId;

	// 弹出版本列表让用户选择要下载的版本（默认最新，可回退到历史版本）。
	// [修复] 原仅在版本数>0时才弹窗，单版本文档会静默跳过；现改为总是弹出，失败时明确报错。
	CArray<PWHelper::PWDocVersionItem, PWHelper::PWDocVersionItem&> arrVersions;
	LONG nVer = PWHelper::EnumDocumentVersions(it.addr.lProjectId, lDocId, arrVersions);
	if (nVer < 0)
	{
		AfxMessageBox(_T("获取版本列表失败：") + PWHelper::GetLastErrorText());
		return;
	}
	{
		CDlgVersionList dlg(it.addr.lProjectId, lDocId, it.addr.strVersionDate, this);
		if (dlg.DoModal() != IDOK)
			return;
		lDocId = dlg.m_sel.lDocumentId;
	}

	CString strLog;
	strLog.Format(_T("开始更新 文件=%s 存储docID=%ld 解析docID=%ld 版本数=%d 选择docID=%ld"),
		(LPCTSTR)it.strFileName, it.addr.lDocumentId, lDocId,
		(int)arrVersions.GetSize(), lDocId);
	LogUpdate(strLog);

	CString strOutFile;
	CString strErr;
	if (!PWHelper::DownloadAndReplace(it.addr.lProjectId, lDocId,
		it.strLocalPath, strErr, &strOutFile))
	{
		LogUpdate(_T("下载/替换失败：") + strErr);
		AfxMessageBox(_T("更新失败：") + strErr);
		return;
	}

	// 当前版本 = 所选版本的更新时间；最新版本 = 服务器上的最新更新时间
	CString strCurDate = PWHelper::GetVersionDate(it.addr.lProjectId, lDocId);
	CString strNewDate = PWHelper::GetLatestVersionDate(it.addr.lProjectId, it.addr.lDocumentId);
	CString strNewVer = PWHelper::GetLatestVersion(it.addr.lProjectId, it.addr.lDocumentId);
	it.addr.lDocumentId = lDocId;
	it.addr.strVersionDate = strCurDate;
	it.addr.strLocalPath = it.strLocalPath;
	PWHelper::SavePwAddr(PWHelper::GetFileFolder(it.strLocalPath), it.strFileName, it.addr);

	strLog.Format(_T("更新完成 文件=%s 目标=%s 下载路径=%s 本地文件时间=%s 服务器最新=%s 服务器版本串=%s"),
		(LPCTSTR)it.strFileName, (LPCTSTR)it.strLocalPath, (LPCTSTR)strOutFile,
		(LPCTSTR)GetLocalVersion(it.strLocalPath), (LPCTSTR)strNewDate, (LPCTSTR)strNewVer);
	LogUpdate(strLog);

	m_list.SetItemText(nRow, 1, strCurDate.IsEmpty() ? GetLocalVersion(it.strLocalPath) : strCurDate);
	m_list.SetItemText(nRow, 2, strNewDate.IsEmpty() ? _T("未检测") : strNewDate);
	SetStatusText(_T("更新完成。"));
}

// 点击"历史版本"列：弹出版本列表，选定版本后下载覆盖本地链接文件并更新该行
void CDlgLinkMgr::OnNMClickList(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMIA = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	*pResult = 0;

	if (pNMIA->iItem < 0 || pNMIA->iItem >= (int)m_arrItems.GetSize())
		return;
	if (pNMIA->iSubItem != COL_HISTVER)
		return;

	LinkItem& it = m_arrItems[pNMIA->iItem];
	if (!it.bHasAddr || it.addr.lDocumentId <= 0)
	{
		AfxMessageBox(_T("该模型没有PW地址来源，无法查看历史版本。"));
		return;
	}

	if (!PWHelper::EnsureLogin(this))
		return;

	// 解析当前最新版本 docid：INI 中记录的 docid 可能指向历史版本
	LONG lDocId = PWHelper::GetLatestDocumentId(it.addr.lProjectId, it.addr.lDocumentId);
	if (lDocId <= 0)
		lDocId = it.addr.lDocumentId;

	CArray<PWHelper::PWDocVersionItem, PWHelper::PWDocVersionItem&> arrVersions;
	LONG nVer = PWHelper::EnumDocumentVersions(it.addr.lProjectId, lDocId, arrVersions);
	if (nVer < 0)
	{
		AfxMessageBox(_T("获取版本列表失败：") + PWHelper::GetLastErrorText());
		return;
	}

	CDlgVersionList dlg(it.addr.lProjectId, lDocId, it.addr.strVersionDate, this);
	if (dlg.DoModal() != IDOK)
		return;
	lDocId = dlg.m_sel.lDocumentId;

	// 下载所选版本覆盖本地链接文件，并更新该行"当前版本"列
	CString strErr;
	CString strOutFile;
	if (!PWHelper::DownloadAndReplace(it.addr.lProjectId, lDocId, it.strLocalPath, strErr, &strOutFile))
	{
		LogUpdate(_T("下载/替换失败：") + strErr);
		AfxMessageBox(_T("切换版本失败：") + strErr);
		return;
	}

	it.addr.lDocumentId = lDocId;
	it.addr.strVersionDate = PWHelper::GetVersionDate(it.addr.lProjectId, lDocId);
	it.addr.strLocalPath = it.strLocalPath;
	PWHelper::SavePwAddr(PWHelper::GetFileFolder(it.strLocalPath), it.strFileName, it.addr);

	CString strCur = it.addr.strVersionDate;
	m_list.SetItemText(pNMIA->iItem, 1, strCur.IsEmpty() ? GetLocalVersion(it.strLocalPath) : strCur);
	m_list.SetItemText(pNMIA->iItem, COL_HISTVER,
		dlg.m_sel.strVersion.IsEmpty() ? _T("?") : dlg.m_sel.strVersion);

	CString strLog;
	strLog.Format(_T("切换历史版本 文件=%s 版本docID=%ld 版本串=%s 下载路径=%s"),
		(LPCTSTR)it.strFileName, lDocId, (LPCTSTR)dlg.m_sel.strVersion, (LPCTSTR)strOutFile);
	LogUpdate(strLog);
	SetStatusText(_T("已切换并下载所选版本。"));
}

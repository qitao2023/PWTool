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

// 列表列索引（"历史版本"列点击弹出版本选择）
static const int COL_CUR_VER  = 1;   // 当前版本号（A/B）
static const int COL_CUR_DATE = 2;   // 当前版本时间
static const int COL_NEW_VER  = 3;   // 最新/所选版本号（A/B）
static const int COL_NEW_DATE = 4;   // 最新/所选版本时间
static const int COL_HISTVER  = 5;   // 历史版本（点击弹出版本选择）

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
	m_list.InsertColumn(0, _T("文件名"), LVCFMT_LEFT, 245);
	m_list.InsertColumn(COL_CUR_VER, _T("当前版本"), LVCFMT_LEFT, 85);
	m_list.InsertColumn(COL_CUR_DATE, _T("当前版本时间"), LVCFMT_LEFT, 200);
	m_list.InsertColumn(COL_NEW_VER, _T("最新/所选版本"), LVCFMT_LEFT, 130);
	m_list.InsertColumn(COL_NEW_DATE, _T("最新/所选版本时间"), LVCFMT_LEFT, 200);
	m_list.InsertColumn(COL_HISTVER, _T("历史版本"), LVCFMT_CENTER, 145);

	// 暂时去掉"添加/删除"按钮：隐藏并左移压缩剩余按钮（需要时可改回来）。
	// 注意 MoveWindow 用像素、对话框模板用对话框单位(DLU)，先 MapDialogRect 换算再移动。
	if (CWnd* pWnd = GetDlgItem(IDC_BTN_ADD))
		pWnd->ShowWindow(SW_HIDE);
	if (CWnd* pWnd = GetDlgItem(IDC_BTN_DEL))
		pWnd->ShowWindow(SW_HIDE);
	{
		CRect rcLink(10, 205, 70, 219);      // 链接：x=10,y=205,w=60,h=14 (DLU)
		MapDialogRect(&rcLink);
		if (CWnd* pWnd = GetDlgItem(IDC_BTN_LINK))
			pWnd->MoveWindow(rcLink);

		CRect rcUnlink(75, 205, 135, 219);   // 卸载
		MapDialogRect(&rcUnlink);
		if (CWnd* pWnd = GetDlgItem(IDC_BTN_UNLINK))
			pWnd->MoveWindow(rcUnlink);

		CRect rcUpdate(140, 205, 200, 219);  // 更新
		MapDialogRect(&rcUpdate);
		if (CWnd* pWnd = GetDlgItem(IDC_BTN_UPDATE))
			pWnd->MoveWindow(rcUpdate);
	}

	ReloadList();   // 内部会检测最新版本

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
		// 当前版本：版本号取 INI 记录的 PW 版本串(A/B)，时间取 INI 版本时间（本地文件修改时间
		// 不反映它对应服务器的哪个版本）；无 PW 地址或 INI 无记录时版本号"—"、时间退回文件修改时间。
		CString strCurVer = (it.bHasAddr && !it.addr.strVersion.IsEmpty())
			? it.addr.strVersion : _T("—");
		CString strCurDate = (it.bHasAddr && !it.addr.strVersionDate.IsEmpty())
			? it.addr.strVersionDate : GetLocalVersion(it.strLocalPath);
		m_list.SetItemText(nRow, COL_CUR_VER, strCurVer);
		m_list.SetItemText(nRow, COL_CUR_DATE, strCurDate);
		m_list.SetItemText(nRow, COL_NEW_VER, _T("未检测"));
		m_list.SetItemText(nRow, COL_NEW_DATE, _T("未检测"));
		// 历史版本列先置"—"，登录检测时对有 PW 地址的行填版本数
		m_list.SetItemText(nRow, COL_HISTVER, _T("—"));
	}

	// 重建列表后刷新"最新/所选版本"列（链接/卸载/添加/删除后都会重新检测最新），
	// 否则最新版本信息会一直停在"未检测"。
	if (PWHelper::IsLoggedIn())
		DetectLatestVersions();
	else
		SetStatusText(_T("未登录PW系统，最新版本未检测。"));
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
		CString strVer;
		LONG lVerCnt = 0;
		if (it.bHasAddr && it.addr.lDocumentId > 0)
		{
			strDate = PWHelper::GetLatestVersionDate(it.addr.lProjectId, it.addr.lDocumentId);
			strVer = PWHelper::GetLatestVersion(it.addr.lProjectId, it.addr.lDocumentId);
			// 版本数用"按文件名枚举"统计（本数据源版本集API只返回活动版本），与历史版本弹窗口径一致
			CArray<PWHelper::PWDocVersionItem, PWHelper::PWDocVersionItem&> arrVersions;
			if (PWHelper::EnumSameNameDocuments(it.addr.lProjectId, it.addr.lDocumentId, arrVersions) > 0)
				lVerCnt = (LONG)arrVersions.GetSize();
		}
		// "最新/所选版本"列：默认显示最新；用户已选定过非最新版本则保留其选择
		if (it.lChosenDocId > 0)
		{
			m_list.SetItemText((int)i, COL_NEW_VER,
				it.strChosenVer.IsEmpty() ? _T("?") : it.strChosenVer);
			m_list.SetItemText((int)i, COL_NEW_DATE,
				it.strChosenDate.IsEmpty() ? _T("—") : it.strChosenDate);
		}
		else
		{
			m_list.SetItemText((int)i, COL_NEW_VER, strVer.IsEmpty() ? _T("未检测") : strVer);
			m_list.SetItemText((int)i, COL_NEW_DATE, strDate.IsEmpty() ? _T("未检测") : strDate);
		}
		// 历史版本列显示版本数量并带省略号，提示可点开查看更多版本
		CString strCnt = _T("—");
		if (lVerCnt > 0)
			strCnt.Format(_T("%d个版本..."), lVerCnt);
		m_list.SetItemText((int)i, COL_HISTVER, strCnt);
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

		CString strNewDate = PWHelper::GetVersionDate(it.addr.lProjectId, lDocId);
		CString strNewVer = PWHelper::GetVersion(it.addr.lProjectId, lDocId);
		it.addr.bLink = TRUE;
		it.addr.lDocumentId = lDocId;
		if (!strNewVer.IsEmpty())
			it.addr.strVersion = strNewVer;
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

	// 更新目标：用户经"历史版本"列选定的版本（若有），否则链接当前版本。
	// [改动] 不再默认跳到最新版本；本数据源建新版本时旧版本会被重新分配 docid，
	// 存储 docid 可能已失效，故按版本串 addr.strVersion 解析当前版本真实 docid。
	LONG lDocId = it.lChosenDocId;
	if (lDocId <= 0)
	{
		lDocId = it.addr.lDocumentId;
		if (lDocId > 0 && !it.addr.strVersion.IsEmpty())
		{
			CArray<PWHelper::PWDocVersionItem, PWHelper::PWDocVersionItem&> arr;
			if (PWHelper::EnumSameNameDocuments(it.addr.lProjectId, lDocId, arr) > 0)
			{
				for (INT_PTR i = 0; i < arr.GetSize(); i++)
				{
					if (arr[i].strVersion == it.addr.strVersion)
					{
						lDocId = arr[i].lDocumentId;
						break;
					}
				}
			}
		}
	}
	if (lDocId <= 0)
		lDocId = it.addr.lDocumentId;

	CString strLog;
	strLog.Format(_T("开始更新 文件=%s 存储docID=%ld 目标docID=%ld(%s)"),
		(LPCTSTR)it.strFileName, it.addr.lDocumentId, lDocId,
		it.lChosenDocId > 0 ? (LPCTSTR)it.strChosenVer : (LPCTSTR)it.addr.strVersion);
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

	// 更新后：只更新"当前版本/当前版本时间"列与本地 INI（反映刚下载的版本）；
	// "最新/所选版本"列与"历史版本"选择保持不变（不刷最新、不清空选择）。
	it.addr.lDocumentId = lDocId;
	it.addr.strVersion = PWHelper::GetVersion(it.addr.lProjectId, lDocId);
	it.addr.strVersionDate = PWHelper::GetVersionDate(it.addr.lProjectId, lDocId);
	it.addr.strLocalPath = it.strLocalPath;
	PWHelper::SavePwAddr(PWHelper::GetFileFolder(it.strLocalPath), it.strFileName, it.addr);

	m_list.SetItemText(nRow, COL_CUR_VER, it.addr.strVersion.IsEmpty() ? _T("—") : it.addr.strVersion);
	m_list.SetItemText(nRow, COL_CUR_DATE,
		it.addr.strVersionDate.IsEmpty() ? GetLocalVersion(it.strLocalPath) : it.addr.strVersionDate);

	strLog.Format(_T("更新完成 文件=%s 下载路径=%s 当前版本=%s 当前版本时间=%s"),
		(LPCTSTR)it.strFileName, (LPCTSTR)strOutFile,
		(LPCTSTR)it.addr.strVersion, (LPCTSTR)it.addr.strVersionDate);
	LogUpdate(strLog);

	SetStatusText(_T("更新完成。"));
}

// 点击"历史版本"列：弹出版本列表选定版本，仅记录到"最新/所选版本"列，点"更新"才生效下载
void CDlgLinkMgr::OnNMClickList(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMIA = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	*pResult = 0;

	if (pNMIA->iItem < 0 || pNMIA->iItem >= (int)m_arrItems.GetSize())
		return;
	// 点击"历史版本"列弹出版本选择
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
	LONG nVer = PWHelper::EnumSameNameDocuments(it.addr.lProjectId, lDocId, arrVersions);
	if (nVer < 0)
	{
		AfxMessageBox(_T("获取版本列表失败：") + PWHelper::GetLastErrorText());
		return;
	}

	CDlgVersionList dlg(it.addr.lProjectId, lDocId, it.addr.strVersionDate, this);
	if (dlg.DoModal() != IDOK)
		return;

	// 只记录所选版本，不立即下载；点"更新"时才按此版本更新本地模型。
	it.lChosenDocId = dlg.m_sel.lDocumentId;
	it.strChosenVer = dlg.m_sel.strVersion;
	it.strChosenDate = dlg.m_sel.strUpdateTime;

	// 所选版本显示在"最新/所选版本"列（默认是最新，选了非最新就显示所选）
	CString strSelVer = it.strChosenVer.IsEmpty() ? _T("?") : it.strChosenVer;
	CString strSelDate = it.strChosenDate.IsEmpty() ? _T("—") : it.strChosenDate;
	m_list.SetItemText(pNMIA->iItem, COL_NEW_VER, strSelVer);
	m_list.SetItemText(pNMIA->iItem, COL_NEW_DATE, strSelDate);

	CString strLog;
	strLog.Format(_T("选择历史版本 文件=%s 版本docID=%ld 版本串=%s（待点\"更新\"生效）"),
		(LPCTSTR)it.strFileName, it.lChosenDocId, (LPCTSTR)it.strChosenVer);
	LogUpdate(strLog);
	SetStatusText(_T("已选择版本，点\"更新\"后生效。"));
}

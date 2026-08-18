// DocListDlg.cpp: PW 文档列表对话框实现

#include "pch.h"
#include "framework.h"
#include "PWExampleFroTSZ-VS2017.h"
#include "DocListDlg.h"
#include "PWHelper.h"
#include "VersionListDlg.h"

#include <afxtempl.h>
#include <shlobj.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// "历史版本"列索引：点击该列弹出版本选择
static const int COL_HISTVER = 4;

BEGIN_MESSAGE_MAP(CDocListDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BTN_SELECTDIR, &CDocListDlg::OnBnClickedSelectdir)
	ON_BN_CLICKED(IDC_BTN_BROWSE_LOCALPATH, &CDocListDlg::OnBnClickedBrowseLocalpath)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_LIST_DOCS, &CDocListDlg::OnNMCustomdrawList)
	ON_NOTIFY(NM_CLICK, IDC_LIST_DOCS, &CDocListDlg::OnNMClickList)
END_MESSAGE_MAP()


CDocListDlg::CDocListDlg(Mode mode, CWnd* pParent)
	: CDialogEx(IDD_DOCLIST_DLG, pParent)
	, m_mode(mode)
	, m_lCurrentProjectId(0)
{
}

CDocListDlg::~CDocListDlg()
{
}

void CDocListDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_DOCS, m_list);
	DDX_Text(pDX, IDC_EDIT_LOCALPATH, m_strLocalPath);
	DDX_Text(pDX, IDC_EDIT_CURFOLDER, m_strCurFolder);
}

BOOL CDocListDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 列表列：名称 / 版本 / 更新时间 / 权限
	m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	m_list.InsertColumn(0, _T("名称"), LVCFMT_LEFT, 210);
	m_list.InsertColumn(1, _T("版本"), LVCFMT_LEFT, 60);
	m_list.InsertColumn(2, _T("更新时间"), LVCFMT_LEFT, 200);
	m_list.InsertColumn(3, _T("权限"), LVCFMT_LEFT, 60);
	m_list.InsertColumn(COL_HISTVER, _T("历史版本"), LVCFMT_CENTER, 105);

	if (m_mode == MODE_OPEN)
	{
		m_list.ModifyStyle(0, LVS_SINGLESEL);      // 单选
		SetDlgItemText(IDOK, _T("打开"));
	}
	else
	{
		m_list.ModifyStyle(LVS_SINGLESEL, 0);      // 多选
		SetDlgItemText(IDOK, _T("链接"));
	}

	// 默认本地路径：优先取上次使用过的路径（打开/链接分开记忆），无记录时用默认 model / LinkModel
	if (m_strLocalPath.IsEmpty())
	{
		m_strLocalPath = PWHelper::GetLastLocalPath(m_mode == MODE_LINK);
		if (m_strLocalPath.IsEmpty())
			m_strLocalPath = (m_mode == MODE_OPEN) ? PWHelper::GetModelDir() : PWHelper::GetLinkModelDir();
	}
	UpdateData(FALSE);

	return TRUE;
}

// 判断 cand 是否比 existing 更新：活动版本(ORIGINALNO=0)最新，其次版本串，最后 docid
static BOOL IsDocNewer(const PWHelper::PWDocItem& cand, const PWHelper::PWDocItem& existing)
{
	if (cand.lOriginalNo == 0 && existing.lOriginalNo != 0) return TRUE;
	if (existing.lOriginalNo == 0 && cand.lOriginalNo != 0) return FALSE;
	int c = PWHelper::CompareVersionStrings(cand.strVersion, existing.strVersion);
	if (c != 0) return (c > 0);
	return (cand.lDocumentId > existing.lDocumentId);
}

void CDocListDlg::FillDocumentList(LONG lProjectId)
{
	m_list.DeleteAllItems();
	m_arrAll.RemoveAll();
	m_lCurrentProjectId = lProjectId;

	LONG nCount = aaApi_SelectDocumentsByProjectId(lProjectId);
	if (nCount <= 0)
		return;

	// 先收集所有文档行（含各版本）
	CArray<PWHelper::PWDocItem, PWHelper::PWDocItem&> arrTmp;
	for (LONG i = 0; i < nCount; i++)
	{
		PWHelper::PWDocItem item;
		item.lProjectId = lProjectId;
		item.lDocumentId = aaApi_GetDocumentNumericProperty(DOC_PROP_ID, i);
		item.lOriginalNo = aaApi_GetDocumentNumericProperty(DOC_PROP_ORIGINALNO, i);

		const WCHAR* psz = aaApi_GetDocumentStringProperty(DOC_PROP_NAME, i);
		if (psz) item.strName = psz;
		psz = aaApi_GetDocumentStringProperty(DOC_PROP_FILENAME, i);
		if (psz) item.strFileName = psz;
		psz = aaApi_GetDocumentStringProperty(DOC_PROP_VERSION, i);
		if (psz) item.strVersion = psz;
		// [修复] 用 FILE_UPDATE_TIME(文件内容最后更新时间) 代替 UPDATE_TIME(文档属性更新时间)，
		// 检入新版本后 UPDATE_TIME 可能不变，导致列表显示旧时间。
		psz = aaApi_GetDocumentStringProperty(DOC_PROP_FILE_UPDATE_TIME, i);
		if (psz) item.strUpdateTime = psz;

		// [修复记录] 必须传当前buffer索引 i（而非 -1）：GetDocumentAccess 传负数会重新select文档并覆盖静态buffer，
		// 导致后续各行的名称/版本等属性读取错位。
		item.lAccess = PWHelper::GetDocumentAccess(lProjectId, item.lDocumentId, i);

		arrTmp.Add(item);
	}

	// 按文件名去重：同一文件名只保留最新版本（活动版本优先，其次版本串，最后 docid）。
	// [修复] 不能按 docid 判断最新：创建新版本时旧版本会被重新分配更高的 docid。
	// 列表每行一个文件，历史版本只在"历史版本"弹窗里查看。
	for (INT_PTR i = 0; i < arrTmp.GetSize(); i++)
	{
		PWHelper::PWDocItem cand = arrTmp.GetAt(i);   // 非const副本，供 CArray::Add/SetAt
		BOOL bDupe = FALSE;
		for (INT_PTR j = 0; j < m_arrAll.GetSize(); j++)
		{
			if (m_arrAll.GetAt(j).strFileName.CompareNoCase(cand.strFileName) == 0)
			{
				PWHelper::PWDocItem& existing = m_arrAll.GetAt(j);
				existing.lVersionCount++;   // 同名(版本)计数 +1
				if (IsDocNewer(cand, existing))   // cand 更新则替换，但保留累计的版本数
				{
					cand.lVersionCount = existing.lVersionCount;
					m_arrAll.SetAt(j, cand);
				}
				bDupe = TRUE;
				break;
			}
		}
		if (!bDupe)
		{
			cand.lVersionCount = 1;
			m_arrAll.Add(cand);
		}
	}

	// 填充列表
	for (INT_PTR i = 0; i < m_arrAll.GetSize(); i++)
	{
		const PWHelper::PWDocItem& item = m_arrAll.GetAt(i);
		int nRow = m_list.InsertItem(m_list.GetItemCount(), item.strName);
		m_list.SetItemText(nRow, 1, item.strVersion);
		m_list.SetItemText(nRow, 2, item.strUpdateTime);
		m_list.SetItemText(nRow, 3, (item.lAccess & AADMS_ACCESS_WRITE) ? _T("读写") : _T("只读"));
		// 历史版本列显示版本数量并带省略号，提示可点开查看更多版本
		CString strCnt;
		strCnt.Format(_T("%d个版本..."), item.lVersionCount);
		m_list.SetItemText(nRow, COL_HISTVER, strCnt);
	}
}

BOOL CDocListDlg::IsReadOnly(const PWHelper::PWDocItem& item) const
{
	return ((item.lAccess & AADMS_ACCESS_WRITE) == 0);
}

void CDocListDlg::OnBnClickedSelectdir()
{
	if (!PWHelper::EnsureLogin(this))
		return;

	LONG nPrjID = aaApi_SelectProjectDlg(this->m_hWnd, _T("请选择待遍历的一个目录"), 0);
	if (nPrjID <= 0)
		return;

	aaApi_SelectProject(nPrjID);
	const WCHAR* psz = aaApi_GetProjectStringProperty(PROJ_PROP_NAME, 0);
	m_strCurFolder = (psz != NULL) ? CString(psz) : CString(_T(""));
	UpdateData(FALSE);

	FillDocumentList(nPrjID);
}

void CDocListDlg::OnBnClickedBrowseLocalpath()
{
	UpdateData(TRUE);

	BROWSEINFO bi = { 0 };
	bi.hwndOwner = m_hWnd;
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	bi.lpszTitle = _T("选择本地下载目录");

	LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
	if (pidl != NULL)
	{
		TCHAR szPath[MAX_PATH] = { 0 };
		if (SHGetPathFromIDList(pidl, szPath))
		{
			m_strLocalPath = szPath;
			UpdateData(FALSE);
			// 记住本次选择，下次打开对话框直接带出
			PWHelper::SetLastLocalPath(m_strLocalPath, m_mode == MODE_LINK);
		}
		CoTaskMemFree(pidl);
	}
}

void CDocListDlg::OnOK()
{
	UpdateData(TRUE);

	m_arrSelected.RemoveAll();

	POSITION pos = m_list.GetFirstSelectedItemPosition();
	if (pos == NULL)
	{
		AfxMessageBox(_T("请至少选择一个文档。"));
		return;
	}

	while (pos != NULL)
	{
		int nRow = m_list.GetNextSelectedItem(pos);
		if (nRow >= 0 && nRow < m_arrAll.GetSize())
		{
			PWHelper::PWDocItem item = m_arrAll.GetAt(nRow);
			if (m_mode == MODE_OPEN && IsReadOnly(item))
			{
				AfxMessageBox(_T("只读文档不能选择，请选择有读写权限的文档。"));
				return;
			}
			m_arrSelected.Add(item);
		}
	}

	m_strLocalPath.Trim();
	if (m_strLocalPath.IsEmpty())
	{
		AfxMessageBox(_T("请填写本地下载路径。"));
		return;
	}

	// 记住本次使用的本地路径，下次打开自动带出
	PWHelper::SetLastLocalPath(m_strLocalPath, m_mode == MODE_LINK);

	CDialogEx::OnOK();
}

void CDocListDlg::OnNMCustomdrawList(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMLVCUSTOMDRAW pLVCD = reinterpret_cast<LPNMLVCUSTOMDRAW>(pNMHDR);
	*pResult = CDRF_DODEFAULT;

	if (pLVCD->nmcd.dwDrawStage == CDDS_PREPAINT)
	{
		*pResult = CDRF_NOTIFYITEMDRAW;
		return;
	}

	if (pLVCD->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
	{
		int nRow = (int)pLVCD->nmcd.dwItemSpec;
		if (nRow >= 0 && nRow < m_arrAll.GetSize())
		{
			const PWHelper::PWDocItem& item = m_arrAll.GetAt(nRow);
			if (IsReadOnly(item))
			{
				pLVCD->clrText = RGB(160, 160, 160);   // 只读行置灰
				*pResult = CDRF_NEWFONT;
			}
		}
	}
}

// 点击"历史版本"列：弹出版本列表，选定版本后该行显示所选版本
void CDocListDlg::OnNMClickList(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMIA = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	*pResult = 0;

	if (pNMIA->iItem < 0 || pNMIA->iItem >= (int)m_arrAll.GetSize())
		return;
	if (pNMIA->iSubItem != COL_HISTVER)
		return;

	if (!PWHelper::EnsureLogin(this))
		return;

	PWHelper::PWDocItem& item = m_arrAll.GetAt(pNMIA->iItem);

	CArray<PWHelper::PWDocVersionItem, PWHelper::PWDocVersionItem&> arrVersions;
	LONG nVer = PWHelper::EnumSameNameDocuments(item.lProjectId, item.lDocumentId, arrVersions);
	if (nVer < 0)
	{
		AfxMessageBox(_T("获取版本列表失败：") + PWHelper::GetLastErrorText());
		return;
	}

	// 已选定过版本时，传入其更新时间供弹窗标记"(当前)"
	CDlgVersionList dlg(item.lProjectId, item.lDocumentId,
		(item.lChosenDocId > 0) ? item.strUpdateTime : _T(""), this);
	if (dlg.DoModal() != IDOK)
		return;

	// 记录选定版本；外层表格"版本"列只显示版本号（A/B），"历史版本"列保持"N个版本..."原状不改。
	item.lChosenDocId = dlg.m_sel.lDocumentId;
	item.strVersion = dlg.m_sel.strVersion;
	item.strUpdateTime = dlg.m_sel.strUpdateTime;

	CString strVer = dlg.m_sel.strVersion;
	if (strVer.IsEmpty())
		strVer = _T("?");
	m_list.SetItemText(pNMIA->iItem, 1, strVer);
}

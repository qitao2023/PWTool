// VersionListDlg.cpp: 版本选择对话框实现

#include "pch.h"
#include "framework.h"
#include "PWExampleFroTSZ-VS2017.h"
#include "VersionListDlg.h"
#include "PWHelper.h"

#include <afxtempl.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// 文件大小格式化：B / KB / MB
static CString FormatFileSize(LONG lBytes)
{
    CString str;
    if (lBytes < 1024)
        str.Format(_T("%ld B"), lBytes);
    else if (lBytes < 1024 * 1024)
        str.Format(_T("%.1f KB"), (double)lBytes / 1024.0);
    else
        str.Format(_T("%.1f MB"), (double)lBytes / (1024.0 * 1024.0));
    return str;
}

BEGIN_MESSAGE_MAP(CDlgVersionList, CDialogEx)
END_MESSAGE_MAP()


CDlgVersionList::CDlgVersionList(LONG lProjectId, LONG lDocumentId,
	LPCTSTR pszCurVersionDate, CWnd* pParent)
	: CDialogEx(IDD_VERSIONLIST_DLG, pParent)
	, m_lProjectId(lProjectId)
	, m_lDocumentId(lDocumentId)
	, m_strCurVersionDate(pszCurVersionDate)
{
}

CDlgVersionList::~CDlgVersionList()
{
}

void CDlgVersionList::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_VERSIONS, m_list);
}

BOOL CDlgVersionList::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	m_list.InsertColumn(0, _T("版本号"), LVCFMT_LEFT, 50);
	m_list.InsertColumn(1, _T("版本"), LVCFMT_LEFT, 80);
	m_list.InsertColumn(2, _T("更新时间"), LVCFMT_LEFT, 150);
	m_list.InsertColumn(3, _T("大小"), LVCFMT_LEFT, 70);

	int nCurRow = -1;
	PWHelper::EnumDocumentVersions(m_lProjectId, m_lDocumentId, m_arrVersions);
	for (INT_PTR i = 0; i < m_arrVersions.GetSize(); i++)
	{
		const PWHelper::PWDocVersionItem& v = m_arrVersions.GetAt(i);

		// 版本串后附加标记：最新 / 当前（与本地 INI versionDate 一致）
		CString strVer = v.strVersion;
		if (i == 0)
			strVer += _T(" (最新)");
		if (!m_strCurVersionDate.IsEmpty() && v.strUpdateTime == m_strCurVersionDate)
		{
			strVer += _T(" (当前)");
			nCurRow = (int)i;
		}

		CString strNo;
		strNo.Format(_T("%ld"), v.lVersionNo);

		int nRow = m_list.InsertItem(m_list.GetItemCount(), strNo);
		m_list.SetItemText(nRow, 1, strVer);
		m_list.SetItemText(nRow, 2, v.strUpdateTime);
		m_list.SetItemText(nRow, 3, FormatFileSize(v.lSize));
	}

	// 默认选中当前版本；找不到则选最新
	int nSel = (nCurRow >= 0) ? nCurRow : 0;
	if (m_list.GetItemCount() > 0 && nSel < m_list.GetItemCount())
		m_list.SetItemState(nSel, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

	return TRUE;
}

void CDlgVersionList::OnOK()
{
	POSITION pos = m_list.GetFirstSelectedItemPosition();
	if (pos == NULL)
	{
		AfxMessageBox(_T("请选择一个版本。"));
		return;
	}

	int nRow = m_list.GetNextSelectedItem(pos);
	if (nRow < 0 || nRow >= m_arrVersions.GetSize())
		return;

	m_sel = m_arrVersions.GetAt(nRow);
	CDialogEx::OnOK();
}

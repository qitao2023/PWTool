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
    m_list.InsertColumn(4, _T("上传人"), LVCFMT_LEFT, 90);
    m_list.InsertColumn(5, _T("对象ID"), LVCFMT_LEFT, 90);

    int nCurRow = -1;
    // 按文件名列出所有同名文档作为"版本"（版本集枚举 API 在本数据源只返回活动版本，
    // 文档列表枚举能返回全部，故用 EnumSameNameDocuments）。
    PWHelper::EnumSameNameDocuments(m_lProjectId, m_lDocumentId, m_arrVersions);

    CString strTitle;
    strTitle.Format(_T("选择版本（共 %d 个）"), (int)m_arrVersions.GetSize());
    SetWindowText(strTitle);

    // 本对话框只负责"选择"版本，不在对话框内下载：底部提示与确认按钮改说"选择"
    SetDlgItemText(IDOK, _T("选择"));
    SetDlgItemText(IDC_STATIC_VINFO, _T("请选择版本（默认最新，可回退到历史版本）"));

    for (INT_PTR i = 0; i < m_arrVersions.GetSize(); i++)
    {
        const PWHelper::PWDocVersionItem& v = m_arrVersions.GetAt(i);

        // 只标"(最新)"（列表第一行即最新）；不再显示"(当前)"——
        // 本数据源按 FILE_UPDATE_TIME 匹配版本易错位，标记会标到错误的行上。
        CString strVer = v.strVersion;
        if (i == 0)
            strVer += _T(" (最新)");
        if (!m_strCurVersionDate.IsEmpty() && v.strUpdateTime == m_strCurVersionDate)
            nCurRow = (int)i;   // 仍用于默认选中当前版本，但不再显示标记

        CString strNo;
        strNo.Format(_T("%ld"), v.lVersionNo);

        int nRow = m_list.InsertItem(m_list.GetItemCount(), strNo);
        m_list.SetItemText(nRow, 1, strVer);
        m_list.SetItemText(nRow, 2, v.strUpdateTime);
        m_list.SetItemText(nRow, 3, FormatFileSize(v.lSize));
        // 上传人=修改人(DOC_PROP_UPDATERID)：检入该版本的用户，每版记录正确。
        // [注] 创建人(DOC_PROP_CREATORID)在该数据源建版本时会被轮转写错（新版本继承旧版
        // 创建人、旧版创建人被覆盖成当前人），不可靠，故不显示。
        m_list.SetItemText(nRow, 4, v.strUpdaterName.IsEmpty() ? _T("-") : v.strUpdaterName);
        CString strId;
        strId.Format(_T("%ld"), v.lDocumentId);
        m_list.SetItemText(nRow, 5, strId);
    }

    // 列表为空（异常情况）：加一行占位提示，避免空白弹窗
    if (m_list.GetItemCount() == 0)
    {
        int nRow = m_list.InsertItem(0, _T("-"));
        m_list.SetItemText(nRow, 1, _T("（未获取到版本）"));
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

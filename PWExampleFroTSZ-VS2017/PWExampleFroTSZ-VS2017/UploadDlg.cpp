// UploadDlg.cpp: 上传对话框实现

#include "pch.h"
#include "framework.h"
#include "PWExampleFroTSZ-VS2017.h"
#include "UploadDlg.h"
#include "PWHelper.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CDlgUpload, CDialogEx)
	ON_BN_CLICKED(IDC_BTN_BROWSE_TARGET, &CDlgUpload::OnBnClickedBrowseTarget)
END_MESSAGE_MAP()


CDlgUpload::CDlgUpload(const CString& strModelPath, CWnd* pParent)
	: CDialogEx(IDD_UPLOAD_DLG, pParent)
	, m_strModelPath(strModelPath)
	, m_bHasAddr(FALSE)
	, m_lTargetProjectId(0)
{
}

CDlgUpload::~CDlgUpload()
{
}

void CDlgUpload::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_MODELPATH2, m_strModelPath);
	DDX_Text(pDX, IDC_EDIT_VERSIONCOMMENT, m_strVersionComment);
}

BOOL CDlgUpload::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// [修复] 地址来源应跟文件走：在文件所在目录找 PWAddress.ini，而不是写死 exe\model。
	// 原写死 GetModelDir() 导致下载到自定义路径的文件上传时找不到地址，被误当"新文档"上传，
	// 原 PW 文档永远不生成新版本（版本号 A 不变 B）。
	m_bHasAddr = PWHelper::LoadPwAddr(PWHelper::GetFileFolder(m_strModelPath), m_strModelPath, m_addr);
	if (m_bHasAddr)
	{
		CString strAddr;
		strAddr.Format(_T("数据源:%s  目录:%s(ID:%ld)  文档ID:%ld  版本:%s"),
			(LPCTSTR)m_addr.strDatasource, (LPCTSTR)m_addr.strProjectName,
			m_addr.lProjectId, m_addr.lDocumentId, (LPCTSTR)m_addr.strVersionDate);
		SetDlgItemText(IDC_EDIT_PWADDR, strAddr);
		// 有地址=更新已有文档版本，目标目录选择无意义，禁用浏览
		SetDlgItemText(IDC_EDIT_TARGETDIR, _T("更新已有文档版本，无需选择目录"));
		GetDlgItem(IDC_BTN_BROWSE_TARGET)->EnableWindow(FALSE);
	}
	else
	{
		// 无地址=首次上传新文档：提示点[浏览...]或[上传]时选择目标目录
		SetDlgItemText(IDC_EDIT_PWADDR, _T("无PW地址来源：首次上传，将作为新文档"));
		m_lTargetProjectId = 0;
		SetDlgItemText(IDC_EDIT_TARGETDIR, _T("（未选择，点[浏览...]或[上传]时选择）"));
	}
	return TRUE;
}


// 浏览：预先选择上传目标目录（仅新文档上传时使用）
void CDlgUpload::OnBnClickedBrowseTarget()
{
	if (!PWHelper::EnsureLogin(this))
		return;

	LONG nPrjID = aaApi_SelectProjectDlg(this->m_hWnd, _T("请选择上传目标目录"), 0);
	if (nPrjID <= 0)
		return;

	m_lTargetProjectId = nPrjID;

	// 显示所选目录名
	CString strName;
	aaApi_SelectProject(nPrjID);
	LPCWSTR psz = aaApi_GetProjectStringProperty(PROJ_PROP_NAME, 0);
	if (psz != NULL)
		strName = psz;
	m_strTargetProjectName = strName;
	SetDlgItemText(IDC_EDIT_TARGETDIR, strName.IsEmpty() ? _T("（已选择）") : strName);
}

void CDlgUpload::OnOK()
{
	UpdateData(TRUE);
	if (DoUpload())
		CDialogEx::OnOK();   // 仅成功时关闭对话框
}

BOOL CDlgUpload::DoUpload()
{
	CString strModelFileName = PWHelper::GetFileName(m_strModelPath);
	CString strFolder = PWHelper::GetFileFolder(m_strModelPath);   // [修复] 地址来源/工作目录跟随文件所在目录

	if (m_bHasAddr && m_addr.lDocumentId > 0)
	{
		// 有 PW 地址：CheckOut -> 覆盖 -> CheckIn 上传新版本
		if (!PWHelper::EnsureLogin(this))
			return FALSE;

		CString strErr;
		BOOL bNewVersion = FALSE;
		if (!PWHelper::UploadNewVersion(this->GetSafeHwnd(),
			m_addr.lProjectId, m_addr.lDocumentId,
			m_strModelPath, strFolder, m_strVersionComment, strErr, &bNewVersion))
		{
			AfxMessageBox(_T("上传失败：") + strErr);
			return FALSE;
		}

		// 更新本地 INI 中的版本日期与版本说明
		// [修复] 检入生成了新版本（新 docid），INI 应记录最新版本的 docid；
		// 原来一直停留在检入前的旧 docid 上（虽然读取都会先 GetLatestDocumentId 解析）。
		LONG lNewDocId = PWHelper::GetLatestDocumentId(m_addr.lProjectId, m_addr.lDocumentId);
		if (lNewDocId > 0)
			m_addr.lDocumentId = lNewDocId;
		CString strNewDate = PWHelper::GetVersionDate(m_addr.lProjectId, m_addr.lDocumentId);
		// 回读服务器当前版本串，用于确认是否真的生成了新版本（A->B）
		CString strNewVer = PWHelper::GetLatestVersion(m_addr.lProjectId, m_addr.lDocumentId);
		m_addr.strVersionDate = strNewDate;
		m_addr.strComment = m_strVersionComment;
		PWHelper::SavePwAddr(strFolder, strModelFileName, m_addr);

		CString strMsg;
		if (bNewVersion)
			strMsg.Format(_T("上传成功，已生成新版本。\n服务器当前版本：%s\n文件更新时间：%s"),
				(LPCTSTR)strNewVer, (LPCTSTR)strNewDate);
		else
			strMsg.Format(_T("上传成功（未勾选\"生成新版本\"，仅更新当前版本）。\n服务器当前版本：%s\n文件更新时间：%s"),
				(LPCTSTR)strNewVer, (LPCTSTR)strNewDate);
		AfxMessageBox(strMsg);
		return TRUE;
	}
	else
	{
		// 无 PW 地址：选目标目录后作为新文档上传
		if (!PWHelper::EnsureLogin(this))
			return FALSE;

		LONG nPrjID = m_lTargetProjectId;   // [改进] 优先用对话框中已选目录
		if (nPrjID <= 0)
		{
			nPrjID = aaApi_SelectProjectDlg(this->m_hWnd, _T("请选择上传目标目录"), 0);
			if (nPrjID <= 0)
				return FALSE;
		}

		LONG lDocId = 0;
		CString strErr;
		if (!PWHelper::CreateNewDocument(nPrjID, m_strModelPath, m_strVersionComment, lDocId, strErr))
		{
			AfxMessageBox(_T("上传失败：") + strErr);
			return FALSE;
		}

		// 记录新文档的 PW 地址来源
		PWHelper::PWAddrInfo info;
		info.strDatasource = PWHelper::GetDatasourceName();
		info.lProjectId = nPrjID;
		info.lDocumentId = lDocId;
		aaApi_SelectProject(nPrjID);
		LPCWSTR psz = aaApi_GetProjectStringProperty(PROJ_PROP_NAME, 0);
		if (psz != NULL)
			info.strProjectName = psz;
		info.strVersionDate = PWHelper::GetLatestVersionDate(nPrjID, lDocId);
		info.strComment = m_strVersionComment;
		info.bLink = FALSE;
		PWHelper::SavePwAddr(strFolder, strModelFileName, info);

		AfxMessageBox(_T("上传成功，已作为新文档保存PW地址来源。"));
		return TRUE;
	}
}

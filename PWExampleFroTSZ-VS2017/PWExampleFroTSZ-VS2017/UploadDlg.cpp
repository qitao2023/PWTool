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
END_MESSAGE_MAP()


CDlgUpload::CDlgUpload(const CString& strModelPath, CWnd* pParent)
	: CDialogEx(IDD_UPLOAD_DLG, pParent)
	, m_strModelPath(strModelPath)
	, m_bHasAddr(FALSE)
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
	}
	else
	{
		SetDlgItemText(IDC_EDIT_PWADDR, _T("无PW地址来源（将作为新文档上传到所选目录）"));
	}
	return TRUE;
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
		if (!PWHelper::UploadNewVersion(m_addr.lProjectId, m_addr.lDocumentId,
			m_strModelPath, strFolder, m_strVersionComment, strErr))
		{
			AfxMessageBox(_T("上传失败：") + strErr);
			return FALSE;
		}

		// 更新本地 INI 中的版本日期与版本说明
		CString strNewDate = PWHelper::GetLatestVersionDate(m_addr.lProjectId, m_addr.lDocumentId);
		// 回读服务器当前版本串，用于确认是否真的生成了新版本（A->B）
		CString strNewVer = PWHelper::GetLatestVersion(m_addr.lProjectId, m_addr.lDocumentId);
		m_addr.strVersionDate = strNewDate;
		m_addr.strComment = m_strVersionComment;
		PWHelper::SavePwAddr(strFolder, strModelFileName, m_addr);

		CString strMsg;
		strMsg.Format(_T("上传成功，已更新PW服务器上的文档版本。\n服务器当前版本：%s\n文件更新时间：%s"),
			(LPCTSTR)strNewVer, (LPCTSTR)strNewDate);
		AfxMessageBox(strMsg);
		return TRUE;
	}
	else
	{
		// 无 PW 地址：选目标目录后作为新文档上传
		if (!PWHelper::EnsureLogin(this))
			return FALSE;

		LONG nPrjID = aaApi_SelectProjectDlg(this->m_hWnd, _T("请选择上传目标目录"), 0);
		if (nPrjID <= 0)
			return FALSE;

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

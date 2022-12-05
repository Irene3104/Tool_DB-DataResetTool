#pragma once
#include <QVector>
#include "DBSQLite.h"
#include "DbStruct.h"

using namespace std;
class CDBSQLite;
class CPatient;
class CQuery;
class CServerInfo;

class CDBUtils
{
public:
	CDBUtils();
	CDBUtils(CServerInfo* pServerInfo);
	~CDBUtils();
	
	int SetServerInfo(CServerInfo* pServerInfo);
	void* GetDB() { return &m_dbSqlite; };
	CServerInfo* m_pServerInfo;
	
	// ���̺� ���� 
	int CreateTable_Patient(bool bCheckDB = true);
	int CreateTable_ImageDB();
	int CreateTable_PhysicianCategory();
	int CreateTable_Version();
	int CreateImgDBTable(string strPath, string strName); 	// Converter������ ���
		
	// �߰� �ʵ� �߰� 
	int AddPatientField();
	int AddImageField();

	int UpdatePatientTable(char* cpTable, char* cpId, char* cpChart);
	
	// DB Open 
	int OpenDB(string strPath, string strName);
	int OpenDB_Conv(string strPath, string strName);

	// DB Load 
	int LoadDB(string strPath, string strName);

	// DB Close
	void CloseDB();
	
	// DB IsOpen 
	bool IsOpen() {return m_dbSqlite.isConnected();};

	// DB Open 
	bool IsCheckValidDBConnection();

	// �ܼ� ���� ����
	int	Execute(char* cpQuery);
	
	// Backup & Restore
	void SetBackupPath(char* cpPath);
	bool BackupDB();
	bool RestoreDB(QString strBack);

	// Merge DB 
	bool MergeDB(QString strAddDB);

	// Transaction
	int BeginTransaction();
	int CommitTransection();
	
	// ȯ�� �߰� 
	int  AddPatient(PATIENT_INFO* pPatient, bool bCheckDB = true);
	
	// ȯ�� �˻� 
	void SetQuickSearchKeyword(QString strKey);
	QVector<PATIENT_INFO> SearchPatient(CQuery* pQuery, int& nRet,  bool bCheckDB = true);
	QVector<PATIENT_INFO> RecentPatient(int days, int nMaxCount, int& nRet);

	// ����ȯ�ڿ� ID, Name�� �����ϴ��� ���� 
	int IsExistPatientID(QString strId, int &nCount, bool bDeleteTable = false);
	int IsExistSamePatient_Conv(QString strId, QString strName, QString strBirth, int &nCount, bool bCheckDB = true);
	int IsExistPatientChartNum(QString strChartNum, int &nCount, bool bDeleteTable = false);
		
	// ȯ�� ����
	int DeletePatient(PATIENT_INFO* pPatient, bool bDeleteTable = false);
	int MovePatientToDeleteTable(PATIENT_INFO* pPatient);
	int DeletePatientDB(char* cpID, bool bCheckDB = true);

	// ȯ�� ���� 
	int RecoveryPatient(PATIENT_INFO* pPatient);
	int MoveDeleteToPatientTable(PATIENT_INFO* pPatient);

	// ȯ�� ���� ���� ��� 
	int PermanentlyDeletePatient(PATIENT_INFO* pPatient);

	// �ֱٿ������ð� ���� 
	int UpdateLastAcqTime(PATIENT_INFO* pPatient, bool bCheckDB = true);

	// ���� �ֱٽð� �������� 
	int GetLastUpdateTime();
	
	// ȯ�� ���� ����, ����� ����, ���� ���� ��..
	int EditPatient(PATIENT_INFO* pPatient);
	int EditUpdateTime(PATIENT_INFO* pPatient, bool bCheckDB = true);
	int EditPhysicianInfo(PATIENT_INFO* pPatient);
	int EditCategoryInfo(PATIENT_INFO* pPatient);
	int EditCommentInfo(PATIENT_INFO* pPatient);
	int EditPatientInfo(PATIENT_INFO* pPatient, int nType, bool bDeleteTable = false);
	int EditPatientInfo_Conv(PATIENT_INFO* pPatient, int nType);
	
	//20210719 jangbok PX_UpdateTime ������ UpdateTime�ð����� �������ִ� ��� �߰�
	int RestorePXUpdateTime();

	//���� ���� ���̺� 
	int  AddImageInfo(IMG_DB_INFO* pPatient);
	int  AddImageInfo_Conv(IMG_DB_INFO* pImgInfo);
	int  EditImageInfo(IMG_DB_INFO* pImgInfo);
	int	 DeleteImageDB(IMG_DB_INFO* pPatient);
	int  DeleteImageDB(char* strId, bool bCheckDB = true);

	int	 LastUpdateAddImageDB(QVector<IMG_DB_INFO> pInfo, bool bAcq = true);

	// ���� ��ȸ
	QVector<IMG_DB_INFO> SearchImage(CQuery* pQuery, bool bCheckDB = true, bool bDelete = false);
	QVector<IMG_DB_INFO> SearchImageJoinQuery(CQuery* pQuery, int& nRetCode, bool bDelete = false);
	QVector<PATIENT_INFO> SearchPatientByImage(CQuery* pQuery, bool bDelete = false);
	int  SearchImageInfo(QString strId, QString strFileName, IMG_DB_INFO* pResult, bool bCheckConnect = true);
	QMap<QString, QString> LoadImageComments(QString strId);

	int	 UpdateImageDBForDelete(IMG_DB_INFO* pInfo);  // ���� ������ ����DB Sync
	int	 UpdateImageDBForRecovery(IMG_DB_INFO* pInfo);  // ���� ������ ����DB Sync

	int  UpdateImageDBForComment(IMG_DB_INFO* pInfo); // ���� �ڸ�Ʈ ������ ����DB Sync
	int	 UpdateImageDBForChartNum(QString strId, QString strChartNum); // ChartNum ����� �� �����������.
	int	 UpdateImageDBForReading(IMG_DB_INFO* pInfo);
	int  LastUpdateImageDBForReading(QVector<IMG_DB_READING> pInfo);

	int UpdateDeleteImage(PATIENT_INFO* pPatient, QString strModality, int nCount);
	int UpdateImageCounts_Conv(PATIENT_INFO* pPatient, QVector<int> oCountList);
	int UpdateImageCounts(PATIENT_INFO* pPatient, QVector<int> oCountList);

	// ���� �� ����� ���� 
	QVector<DB_TABLE_CATEGORY> GetCategoryList();
	int AddCategory(DB_TABLE_CATEGORY* pCat, bool bCheckDB = true);
	int ModifyCategory(DB_TABLE_CATEGORY* pCat);
	int DeleteCategory(DB_TABLE_CATEGORY* pCat);

	QVector<DB_TABLE_PHYSICIAN> GetPhysicianList();
	int AddPhysician(DB_TABLE_PHYSICIAN* pCat, bool bCheckDB = true);
	int ModifyPhysician(DB_TABLE_PHYSICIAN* pCat);
	int DeletePhysician(DB_TABLE_PHYSICIAN* pCat);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//// jggang 2019.1211 ���� ���� üũ
	int ModifyPhysicianLogInStatus(DB_TABLE_PHYSICIAN* pCat, int use=false); 
	int GetPhysicianLogInStatus(DB_TABLE_PHYSICIAN* pCat);
	
	PATIENT_INFO GetPatientRowID(char *PatientID, int flag = true); //// jggang 2020.0609
	int UpdatePatientDBPid(int rowid, char *PatientID, int flag=true);
	QVector<IMG_DB_INFO > GetImageDBImgID(char *PatientID);
	int UpdateImageDBPid(int ImgId, char *PatientID);

	std::string IsExistPatientIDAndChartNum(QString strId, int &nCount, int chartnum = false, int flag = true);
	int CopyPatientTable(PATIENT_INFO &info);
	QVector<IMG_DB_INFO> SearchImageExclusiveDeleteImage(CQuery* pQuery, bool bCheckDB = true);
	//
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
protected:
	CDBSQLite	m_dbSqlite;

	char		m_cpDBFileFullPath[256];
	char		m_cpBackupFolder[256];

	char		m_cpKeyword[64];
};
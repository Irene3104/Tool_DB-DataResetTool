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
	
	// 테이블 생성 
	int CreateTable_Patient(bool bCheckDB = true);
	int CreateTable_ImageDB();
	int CreateTable_PhysicianCategory();
	int CreateTable_Version();
	int CreateImgDBTable(string strPath, string strName); 	// Converter에서만 사용
		
	// 추가 필드 추가 
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

	// 단순 쿼리 실행
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
	
	// 환자 추가 
	int  AddPatient(PATIENT_INFO* pPatient, bool bCheckDB = true);
	
	// 환자 검색 
	void SetQuickSearchKeyword(QString strKey);
	QVector<PATIENT_INFO> SearchPatient(CQuery* pQuery, int& nRet,  bool bCheckDB = true);
	QVector<PATIENT_INFO> RecentPatient(int days, int nMaxCount, int& nRet);

	// 가존환자에 ID, Name등 존재하는지 여부 
	int IsExistPatientID(QString strId, int &nCount, bool bDeleteTable = false);
	int IsExistSamePatient_Conv(QString strId, QString strName, QString strBirth, int &nCount, bool bCheckDB = true);
	int IsExistPatientChartNum(QString strChartNum, int &nCount, bool bDeleteTable = false);
		
	// 환자 삭제
	int DeletePatient(PATIENT_INFO* pPatient, bool bDeleteTable = false);
	int MovePatientToDeleteTable(PATIENT_INFO* pPatient);
	int DeletePatientDB(char* cpID, bool bCheckDB = true);

	// 환자 복구 
	int RecoveryPatient(PATIENT_INFO* pPatient);
	int MoveDeleteToPatientTable(PATIENT_INFO* pPatient);

	// 환자 영구 삭제 기능 
	int PermanentlyDeletePatient(PATIENT_INFO* pPatient);

	// 최근영상취득시간 갱신 
	int UpdateLastAcqTime(PATIENT_INFO* pPatient, bool bCheckDB = true);

	// 가장 최근시간 가져오기 
	int GetLastUpdateTime();
	
	// 환자 정보 수정, 담당의 수정, 업종 수정 등..
	int EditPatient(PATIENT_INFO* pPatient);
	int EditUpdateTime(PATIENT_INFO* pPatient, bool bCheckDB = true);
	int EditPhysicianInfo(PATIENT_INFO* pPatient);
	int EditCategoryInfo(PATIENT_INFO* pPatient);
	int EditCommentInfo(PATIENT_INFO* pPatient);
	int EditPatientInfo(PATIENT_INFO* pPatient, int nType, bool bDeleteTable = false);
	int EditPatientInfo_Conv(PATIENT_INFO* pPatient, int nType);
	
	//20210719 jangbok PX_UpdateTime 없을시 UpdateTime시간으로 복구해주는 기능 추가
	int RestorePXUpdateTime();

	//영상 정보 테이블 
	int  AddImageInfo(IMG_DB_INFO* pPatient);
	int  AddImageInfo_Conv(IMG_DB_INFO* pImgInfo);
	int  EditImageInfo(IMG_DB_INFO* pImgInfo);
	int	 DeleteImageDB(IMG_DB_INFO* pPatient);
	int  DeleteImageDB(char* strId, bool bCheckDB = true);

	int	 LastUpdateAddImageDB(QVector<IMG_DB_INFO> pInfo, bool bAcq = true);

	// 영상 조회
	QVector<IMG_DB_INFO> SearchImage(CQuery* pQuery, bool bCheckDB = true, bool bDelete = false);
	QVector<IMG_DB_INFO> SearchImageJoinQuery(CQuery* pQuery, int& nRetCode, bool bDelete = false);
	QVector<PATIENT_INFO> SearchPatientByImage(CQuery* pQuery, bool bDelete = false);
	int  SearchImageInfo(QString strId, QString strFileName, IMG_DB_INFO* pResult, bool bCheckConnect = true);
	QMap<QString, QString> LoadImageComments(QString strId);

	int	 UpdateImageDBForDelete(IMG_DB_INFO* pInfo);  // 영상 삭제시 영상DB Sync
	int	 UpdateImageDBForRecovery(IMG_DB_INFO* pInfo);  // 영상 삭제시 영상DB Sync

	int  UpdateImageDBForComment(IMG_DB_INFO* pInfo); // 영상 코멘트 수정시 영상DB Sync
	int	 UpdateImageDBForChartNum(QString strId, QString strChartNum); // ChartNum 변경시 다 변경해줘야함.
	int	 UpdateImageDBForReading(IMG_DB_INFO* pInfo);
	int  LastUpdateImageDBForReading(QVector<IMG_DB_READING> pInfo);

	int UpdateDeleteImage(PATIENT_INFO* pPatient, QString strModality, int nCount);
	int UpdateImageCounts_Conv(PATIENT_INFO* pPatient, QVector<int> oCountList);
	int UpdateImageCounts(PATIENT_INFO* pPatient, QVector<int> oCountList);

	// 업종 및 담당의 관련 
	QVector<DB_TABLE_CATEGORY> GetCategoryList();
	int AddCategory(DB_TABLE_CATEGORY* pCat, bool bCheckDB = true);
	int ModifyCategory(DB_TABLE_CATEGORY* pCat);
	int DeleteCategory(DB_TABLE_CATEGORY* pCat);

	QVector<DB_TABLE_PHYSICIAN> GetPhysicianList();
	int AddPhysician(DB_TABLE_PHYSICIAN* pCat, bool bCheckDB = true);
	int ModifyPhysician(DB_TABLE_PHYSICIAN* pCat);
	int DeletePhysician(DB_TABLE_PHYSICIAN* pCat);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//// jggang 2019.1211 다중 접속 체크
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
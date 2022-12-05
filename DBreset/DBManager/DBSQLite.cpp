#include <codecvt>
#include <QDebug>
#include "DBSQLite.h"
#include "DbStruct.h"

#define		THEIA_DB_KEY			"theia_admin"

// 아래는 반드시 상용화 버전에는 On 해줘야, 암호화 로직 발동 됨.
#define		USE_SQLITE_SEE	

//hjpark@20210121:데이터베이스 최적화 옵션 처리 
//DB Open하고 Exec Param 설정 On/Off로 동작함. 
//#define     USE_CACHE_SIZE      // 캐시 사이즈 변경 사용, 5000*4KB = 20MB 
//#define	  USE_WAL				// Write-Ahead Logging 기능 활성화
//#define	  USE_SYNC_OFF		// 디스크 동기화 끄기 

static int sqlite_callback(void *NotUsed, int argc, char **argv, char **azColName) {
//	printf("\n----argc %d -------callback-----------%s-----\n", argc, NotUsed);
	int i;

	for (i = 0; i<argc; i++) {
#if 0
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> con;
		std::string str = con.to_bytes(argv[i]);
		printf("%s = %s\n", azColName[i], str.c_str() ? str.c_str() : NULL);
#else
		printf("%s = %s\n", azColName[i], argv[i]?argv[i]:NULL);
		
#endif		
	}
	printf("\n");
	return 0;
}

CDBSQLite::CDBSQLite()
{
	m_pDB = NULL;
	m_pDBTemp = NULL;

}
CDBSQLite::~CDBSQLite()
{

}
//  hjpark@20191014:DB암호화/복호화 진행
int CDBSQLite::EncryptDB()
{
	return sqlite3_rekey_v2(m_pDB, NULL, THEIA_DB_KEY, strlen(THEIA_DB_KEY));
}

int CDBSQLite::DecryptDB()
{
	return sqlite3_key_v2(m_pDB, NULL, THEIA_DB_KEY, strlen(THEIA_DB_KEY));
}

//DB 존재 여부
bool CDBSQLite::IsDBExists()
{
	QString strDBFile = QString::fromStdString(m_strDBPath);
	QFile dbFile(strDBFile);

	return dbFile.exists();
}

//무조건 닫고 다시 열기 
bool CDBSQLite::CheckValidDBConnection(bool readonly /*= false*/)
{
	int flag = SQLITE_OPEN_READWRITE;
	if(readonly) flag = SQLITE_OPEN_READONLY;

	// hjpark@20210310: 32bit Client 다중접속시 아래 Close 시도시 문제 발생. m_pDB값 체크가 유효하지 않을 수 있음.
	//열려있는 경우 대비 Close
	//if (m_pDB) 
	//	sqlite3_close(m_pDB);

	int nRet;
	
	qDebug() << "[CDBSQLite] CheckValidDBConnection Start";

	////int nRet = sqlite3_open_v2(m_strDBPath.c_str(), &m_pDB, SQLITE_OPEN_READWRITE, NULL);
	//// jggang 2019.1212
	nRet = sqlite3_open_v2(m_strDBPath.c_str(), &m_pDB, flag, NULL);
	if (nRet != SQLITE_OK)
	{
		sqlite3_close(m_pDB);
		// Retry 100ms 30회 시도 
		int nCount = 30;
		for (int ii = 0; ii < nCount; ii++)
		{
			nRet = sqlite3_open_v2(m_strDBPath.c_str(), &m_pDB, flag, NULL);
			qDebug() << "[CDBSQLite] sqlite3_open_v2 Open Retry." << ii;			
			if (nRet == SQLITE_OK)
			{
				break;
			}
			else
			{
				sqlite3_close(m_pDB);
			}
			sqlite3_sleep(100);
		}

		if (nRet != SQLITE_OK)
		{
			qDebug() << "[CDBSQLite] CheckValidDBConnection Open Fail.";
			sqlite3_close(m_pDB);
			return false;
		}
	}

#ifdef USE_SQLITE_SEE
	nRet = DecryptDB();
	if (nRet != SQLITE_OK)
	{
		sqlite3_close(m_pDB);
		qDebug() << "[CDBSQLite] CheckValidDBConnection DecryptDB Fail.";
		return false;		
	}
#endif//USE_SQLITE_SEE

	// hjpark@20210310: 한번 WAL모드로 해놓으면 계속 동시 접속시 -WAL파일 생성, 동시접속시 문제 발생.
	/*char *szErrMsg = NULL;
	nRet = sqlite3_exec(m_pDB, "PRAGMA journal_mode=DELETE;", NULL, NULL, &szErrMsg);
	if (nRet != SQLITE_OK && nRet != SQLITE_LOCKED && nRet != SQLITE_BUSY)
	{
		qDebug() << "[CDBSQLite] CheckValidDBConnection PRAGMA journal_mode DELETE Fail." << nRet;
		return false;
	}*/
		
#ifdef USE_CACHE_SIZE	
	qDebug() << "[CDBSQLite] CheckValidDBConnection Optimized Start.";
	nRet = sqlite3_exec(m_pDB, "PRAGMA cache_size=5000;", NULL, NULL, &szErrMsg);
	if (nRet != SQLITE_OK)
	{
		qDebug() << "[CDBSQLite] CheckValidDBConnection PRAGMA cache_siz Fail.";
		return false;
	}
#endif//USE_CACHE_SIZE
	

#ifdef USE_WAL
	nRet = sqlite3_exec(m_pDB, "PRAGMA journal_mode=WAL;", NULL, NULL, &szErrMsg);
	if (nRet != SQLITE_OK)
	{
		qDebug() << "[CDBSQLite] CheckValidDBConnection PRAGMA journal_mode Fail.";
		return false;
	}
#endif//USE_WAL

#ifdef USE_SYNC_OFF
	nRet = sqlite3_exec(m_pDB, "PRAGMA synchronous=OFF;", NULL, NULL, &szErrMsg);
	if (nRet != SQLITE_OK)
	{
		qDebug() << "[CDBSQLite] CheckValidDBConnection PRAGMA synchronous Fail.";
		return false;
	}
#endif//USE_SYNC_OFF

	qDebug() << "[CDBSQLite] CheckValidDBConnection End";
	
	return true;
}

int CDBSQLite::OpenConnection(string DatabaseName, string DatabaseDir)
{
	m_strDBName = DatabaseName;
	m_strDBPath = DatabaseDir + DatabaseName;

	// hjpark@20191014:DB암호화/복호화 진행
	int nRet = sqlite3_open_v2(m_strDBPath.c_str(), &m_pDB, SQLITE_OPEN_READWRITE, NULL);
	if (nRet == SQLITE_OK) 
	{
#ifdef USE_SQLITE_SEE
		nRet = sqlite3_key_v2(m_pDB, NULL, THEIA_DB_KEY, strlen(THEIA_DB_KEY));
#endif//USE_SQLITE_SEE
	}
	else // 파일 없어서 새로 맹긂
	{
		QString strFileName = QString::fromStdString(m_strDBPath);
		if (QFile(strFileName).exists() == false)
		{
			sqlite3_close(m_pDB);
		
			nRet = sqlite3_open_v2(m_strDBPath.c_str(), &m_pDB, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
#ifdef USE_SQLITE_SEE
			if (nRet == SQLITE_OK)
				nRet = sqlite3_rekey_v2(m_pDB, NULL, THEIA_DB_KEY, strlen(THEIA_DB_KEY));
#endif//USE_SQLITE_SEE
		}
		else
		{
			sqlite3_close(m_pDB);

			// Retry 100ms 30회 시도 
			int nCount = 30;
			for (int ii = 0; ii < nCount; ii++)
			{
				nRet = sqlite3_open_v2(m_strDBPath.c_str(), &m_pDB, SQLITE_OPEN_READWRITE, NULL);
				qDebug() << "[CDBSQLite] OpenConnection sqlite3_open_v2 Retry." << ii;
				if (nRet == SQLITE_OK)
				{
					break;
				}
				else
				{
					sqlite3_close(m_pDB);
				}
				sqlite3_sleep(100);
			}

			if (nRet == SQLITE_OK)
			{
#ifdef USE_SQLITE_SEE
				nRet = sqlite3_key_v2(m_pDB, NULL, THEIA_DB_KEY, strlen(THEIA_DB_KEY));
#endif//USE_SQLITE_SEE
			}
		}
	}

	if (nRet == SQLITE_OK)
	{
		char *szErrMsg = NULL;
		nRet = sqlite3_exec(m_pDB, "PRAGMA journal_mode=DELETE;", NULL, NULL, &szErrMsg);
		if (nRet != SQLITE_OK && nRet != SQLITE_LOCKED && nRet != SQLITE_BUSY)
		{
			qDebug() << "[CDBSQLite] CheckValidDBConnection PRAGMA journal_mode DELETE Fail." << sqlite3_errstr(nRet);
			//return nRet;
			return SQLITE_OK;
		}
	}

	return nRet;
}

bool CDBSQLite::IsValidDBConnection(char* cpDatabaseFullPath)
{
	sqlite3 *conn;
	int nRet = sqlite3_open_v2(cpDatabaseFullPath, &conn, SQLITE_OPEN_READONLY, NULL);
	sqlite3_close(conn);
	if (nRet == SQLITE_OK)
		return true;

	return false;
}
//DB 암호 해제
bool CDBSQLite::RekeyDatabase(char* cpDatabaseFullPath)
{
	sqlite3 *conn;
	int nRet = sqlite3_open_v2(cpDatabaseFullPath, &conn, SQLITE_OPEN_READWRITE, NULL);
	if (nRet != SQLITE_OK)
	{
		sqlite3_close(conn);
		return false;
	}
		
	nRet = sqlite3_key_v2(conn, NULL, THEIA_DB_KEY, strlen(THEIA_DB_KEY));
	if (nRet != SQLITE_OK) return false;

	nRet = sqlite3_rekey_v2(conn, NULL, NULL, 0);
	sqlite3_close(conn);

	if (nRet != SQLITE_OK) return false;

	return true;
}

bool CDBSQLite::EncryptDatabase(char* cpDatabaseFullPath)
{
	sqlite3 *conn;
	int nRet = sqlite3_open_v2(cpDatabaseFullPath, &conn, SQLITE_OPEN_READWRITE, NULL);
	if (nRet != SQLITE_OK)
	{
		sqlite3_close(conn);
		return false;
	}

	nRet = sqlite3_rekey_v2(conn, NULL, THEIA_DB_KEY, strlen(THEIA_DB_KEY));
	sqlite3_close(conn);

	if (nRet != SQLITE_OK) return false;

	return true;
}

int CDBSQLite::CloseConnection()
{
	if (m_pDB == NULL) return -1;

	int nRet = sqlite3_close(m_pDB);
	m_pDB = NULL;

	return nRet;
}
string CDBSQLite::GetLatestError()
{
	return string(sqlite3_errmsg(m_pDB));
}

int CDBSQLite::BeginTransaction()
{
	return Excute("BEGIN TRANSACTION;");
}

int CDBSQLite::CommitTransection()
{
	return Excute("COMMIT;");
}

int CDBSQLite::Excute(string Query)
{
	char *szErrMsg = NULL; // Error 발생시메세지를저장하는변수 
	PATIENT_INFO pInfo;
 	int ret = sqlite3_exec(m_pDB, Query.c_str(), sqlite_callback, (void*)&pInfo, &szErrMsg);
	//객체해제
	if (ret != SQLITE_OK)
		sqlite3_free( szErrMsg ); 
	return ret;
}


bool  CDBSQLite::isConnected()
{
	if (m_pDB == NULL) return false;

	int iCur, iHiwtr, ret;
	iHiwtr = iCur = -1;
	ret = sqlite3_db_status(m_pDB, SQLITE_DBSTATUS_CACHE_USED, &iCur, &iHiwtr, 0);

	if (iCur > 0 ) 
		return true;
	
	return false;
}

int CDBSQLite::BackupDB(char* zFileName)
{
	int rc;                     /* Function return code */
	sqlite3 *pFile;             /* Database connection opened on zFilename */
	sqlite3_backup *pBackup;    /* Backup handle used to copy data */
	sqlite3 *pDb;               /* Database to back up */
	
	pDb = m_pDB;
	/* Open the database file identified by zFilename. */
	rc = sqlite3_open(zFileName, &pFile);
	if (rc == SQLITE_OK) {
		/* Open the sqlite3_backup object used to accomplish the transfer */
		pBackup = sqlite3_backup_init(pFile, "main", pDb, "main");
		if (pBackup) {
			/* Each iteration of this loop copies 5 database pages from database
			** pDb to the backup database. If the return value of backup_step()
			** indicates that there are still further pages to copy, sleep for
			** 250 ms before repeating. */
			do {
				rc = sqlite3_backup_step(pBackup, 5);
				if (rc == SQLITE_OK || rc == SQLITE_BUSY || rc == SQLITE_LOCKED) {
					sqlite3_sleep(25);
				}
			} while (rc == SQLITE_OK || rc == SQLITE_BUSY || rc == SQLITE_LOCKED);

			/* Release resources allocated by backup_init(). */
			(void)sqlite3_backup_finish(pBackup);
		}
		rc = sqlite3_errcode(pFile);

#ifdef USE_SQLITE_SEE
		if (rc == SQLITE_OK)
		{
			int nRet = sqlite3_open_v2(zFileName, &m_pDBTemp, SQLITE_OPEN_READWRITE, NULL);
			if (nRet != SQLITE_OK) 
			{
				sqlite3_close(m_pDBTemp);
				return false;
			}

			nRet = sqlite3_rekey_v2(m_pDBTemp, NULL, THEIA_DB_KEY, strlen(THEIA_DB_KEY));

			sqlite3_close(m_pDBTemp);
			if (nRet == SQLITE_OK)
				return nRet;
		}
#endif//USE_SQLITE_SEE
	}

	printf("Done backing up the database%s\n", zFileName);

	/* Close the database connection opened on database file zFilename
	** and return the result of this function. */
	(void)sqlite3_close(pFile);

	return rc;
}

// 해당 테이블에 특정 필드명이 존재하는지 체크하는 함수 
bool CDBSQLite::ColumnExists(const std::string& a_table, const std::string& a_column)
{
	std::string query = "SELECT * FROM " + a_table + " WHERE rowid = 0;";

	sqlite3		 *pDB = m_pDB;
	sqlite3_stmt *pStmt = m_pStmt;
	
	int ret = sqlite3_prepare_v2(pDB, query.c_str(), -1, &pStmt, NULL);
	if (ret != SQLITE_OK)
	{
		printf("ERROR : %s\n", GetLatestError().c_str());
		return false;
	}

	ret = sqlite3_step(pStmt);
	if (ret != SQLITE_DONE)
	{
		printf("ERROR ColumnExists : %s\n", GetLatestError().c_str());
		return false;
	}

	const int columnsCount = sqlite3_column_count(pStmt);
	for (int index = 0; index < columnsCount; ++index)
	{
		const char* const columnName = sqlite3_column_name(pStmt, index);
		if (strncmp(columnName, a_column.c_str(), a_column.size()) == 0)
		{
			sqlite3_finalize(pStmt);
			return true;
		}
	}

	sqlite3_finalize(pStmt);
	return false;
}


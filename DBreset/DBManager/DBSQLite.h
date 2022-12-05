#pragma once
#include "sqlite3.h"
#include <string>

using namespace std;

class CDBSQLite
{

public:
	CDBSQLite();
	~CDBSQLite();
			
	int OpenConnection(string DatabaseName,string DatabaseDir); 
	int CloseConnection();

	// DB Connection Valid Check
	bool CheckValidDBConnection(bool readonly = false);
	bool IsDBExists();
	
	//hjpark@20191014:DB암호화 / 복호화 진행
	int EncryptDB();
	int DecryptDB();
	
	// 서버 연결 상태  Test, ReadOnly
	bool IsValidDBConnection(char* cpValue);
	bool RekeyDatabase(char* cpDatabaseFullPath);
	bool EncryptDatabase(char* cpDatabaseFullPath);

	// 특정 필드 존재하는지 체크
	bool ColumnExists(const std::string& a_table, const std::string& a_column);
	
	// Transaction
	int BeginTransaction();
	int CommitTransection();

	// 쿼리 실행
	int Excute(string Query);

	string GetLatestError();

	bool isConnected() ;	

	sqlite3 * GetDB() { return m_pDB; }
	sqlite3_stmt* GetStatement() {	return m_pStmt;	};

	int BackupDB(char* zFileName);
	
public:
	char *m_errmsg;

protected:
	bool	m_bConnected;     
	bool    m_bConsole;	      
	string  m_strLastError;   

	sqlite3		 *m_pDB = NULL;		
	sqlite3		 *m_pDBTemp = NULL;
	sqlite3_stmt *m_pStmt;				  
	
	string   m_strDBName;
	string   m_strDBPath;

};

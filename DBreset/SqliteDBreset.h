#pragma once
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <Qtsql/QtSql>
#include <QDialog>
#include <QDebug>
#include <QVariant>
#include <qtextstream.h>
#include <qexception.h>
#include <iostream>
#include <codecvt>
#include <QDebug>
#include "qstring.h"
#include "Define.h" 


class CDBSQLite;

class TheiaSqliteDB : public QObject
{
	Q_OBJECT

public:
	TheiaSqliteDB();
	~TheiaSqliteDB();
	bool run(QString* strlog, QString strServerPath, QString strDataPath);

private:
	bool DbOpen(QString strServerPath);
	bool DbClose ();
	bool DbReset(QString tablename);
	bool ResetPatientFolder(QString strDataPath);

private:

	QString TheiaSqliteDB::stringtoquery(QString strinput);
	CDBSQLite*				m_SqliteDB = NULL;

signals:
	void	UpdateStep(QString text, int num);
};


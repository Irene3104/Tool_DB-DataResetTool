#pragma once
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QDialog>
#include <QDebug>
#include <QVariant>
#include <Qtsql/QtSql>
#include <qtextstream.h>
#include <iostream>
#include "Define.h" 

class TheiaWebDB : public QObject
{
    Q_OBJECT

public:
	bool run(QString* strlog, QString strdataPath);

private:
    bool DbOpen();
	bool PatientTableReset();
	bool ImagedbTableReset();
	bool StmountTableReset();
	bool tlitemTableReset();
	bool tlsplitterTableReset();
	bool ResetPatientFolder(QString strdataPath);

private:
    QSqlDatabase m_db;


signals:
    void	UpdateStep(QString text, int num);
};

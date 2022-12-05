/********************************************************************************
** Form generated from reading UI file 'DBreset.ui'
**
** Created by: Qt User Interface Compiler version 5.13.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_DBRESET_H
#define UI_DBRESET_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTextBrowser>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_DBresetClass
{
public:
    QWidget *centralWidget;
    QPushButton *btn_clear;
    QProgressBar *progressBar;
    QTextBrowser *textBrowser;
    QLineEdit *edit_serverPath;
    QPushButton *btn_serverPath;
    QGroupBox *groupBox;
    QRadioButton *radio_sqlite;
    QRadioButton *radio_web;
    QLineEdit *edit_dataPath;
    QPushButton *btn_dataPath;
    QLabel *label;
    QLabel *label_2;
    QToolBar *mainToolBar;
    QStatusBar *statusBar;

    void setupUi(QMainWindow *DBresetClass)
    {
        if (DBresetClass->objectName().isEmpty())
            DBresetClass->setObjectName(QString::fromUtf8("DBresetClass"));
        DBresetClass->resize(499, 393);
        QSizePolicy sizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(DBresetClass->sizePolicy().hasHeightForWidth());
        DBresetClass->setSizePolicy(sizePolicy);
        DBresetClass->setMinimumSize(QSize(499, 393));
        DBresetClass->setMaximumSize(QSize(499, 393));
        centralWidget = new QWidget(DBresetClass);
        centralWidget->setObjectName(QString::fromUtf8("centralWidget"));
        btn_clear = new QPushButton(centralWidget);
        btn_clear->setObjectName(QString::fromUtf8("btn_clear"));
        btn_clear->setGeometry(QRect(310, 230, 171, 101));
        progressBar = new QProgressBar(centralWidget);
        progressBar->setObjectName(QString::fromUtf8("progressBar"));
        progressBar->setGeometry(QRect(20, 180, 461, 23));
        progressBar->setValue(24);
        textBrowser = new QTextBrowser(centralWidget);
        textBrowser->setObjectName(QString::fromUtf8("textBrowser"));
        textBrowser->setGeometry(QRect(20, 230, 281, 101));
        edit_serverPath = new QLineEdit(centralWidget);
        edit_serverPath->setObjectName(QString::fromUtf8("edit_serverPath"));
        edit_serverPath->setGeometry(QRect(120, 100, 291, 31));
        btn_serverPath = new QPushButton(centralWidget);
        btn_serverPath->setObjectName(QString::fromUtf8("btn_serverPath"));
        btn_serverPath->setGeometry(QRect(420, 100, 61, 31));
        groupBox = new QGroupBox(centralWidget);
        groupBox->setObjectName(QString::fromUtf8("groupBox"));
        groupBox->setGeometry(QRect(20, 30, 461, 61));
        radio_sqlite = new QRadioButton(groupBox);
        radio_sqlite->setObjectName(QString::fromUtf8("radio_sqlite"));
        radio_sqlite->setGeometry(QRect(90, 30, 86, 16));
        radio_web = new QRadioButton(groupBox);
        radio_web->setObjectName(QString::fromUtf8("radio_web"));
        radio_web->setGeometry(QRect(290, 30, 86, 16));
        edit_dataPath = new QLineEdit(centralWidget);
        edit_dataPath->setObjectName(QString::fromUtf8("edit_dataPath"));
        edit_dataPath->setGeometry(QRect(120, 140, 291, 31));
        btn_dataPath = new QPushButton(centralWidget);
        btn_dataPath->setObjectName(QString::fromUtf8("btn_dataPath"));
        btn_dataPath->setGeometry(QRect(420, 140, 61, 31));
        label = new QLabel(centralWidget);
        label->setObjectName(QString::fromUtf8("label"));
        label->setGeometry(QRect(20, 100, 91, 31));
        label_2 = new QLabel(centralWidget);
        label_2->setObjectName(QString::fromUtf8("label_2"));
        label_2->setGeometry(QRect(20, 140, 91, 31));
        DBresetClass->setCentralWidget(centralWidget);
        mainToolBar = new QToolBar(DBresetClass);
        mainToolBar->setObjectName(QString::fromUtf8("mainToolBar"));
        DBresetClass->addToolBar(Qt::TopToolBarArea, mainToolBar);
        statusBar = new QStatusBar(DBresetClass);
        statusBar->setObjectName(QString::fromUtf8("statusBar"));
        DBresetClass->setStatusBar(statusBar);

        retranslateUi(DBresetClass);

        QMetaObject::connectSlotsByName(DBresetClass);
    } // setupUi

    void retranslateUi(QMainWindow *DBresetClass)
    {
        DBresetClass->setWindowTitle(QCoreApplication::translate("DBresetClass", "DBreset", nullptr));
        btn_clear->setText(QCoreApplication::translate("DBresetClass", "All Clear", nullptr));
        btn_serverPath->setText(QCoreApplication::translate("DBresetClass", "...", nullptr));
        groupBox->setTitle(QCoreApplication::translate("DBresetClass", "Select Database Type", nullptr));
        radio_sqlite->setText(QCoreApplication::translate("DBresetClass", "Sqlite DB", nullptr));
        radio_web->setText(QCoreApplication::translate("DBresetClass", "Web DB", nullptr));
        btn_dataPath->setText(QCoreApplication::translate("DBresetClass", "...", nullptr));
        label->setText(QCoreApplication::translate("DBresetClass", "Database Folder:", nullptr));
        label_2->setText(QCoreApplication::translate("DBresetClass", "Image Folder:", nullptr));
    } // retranslateUi

};

namespace Ui {
    class DBresetClass: public Ui_DBresetClass {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_DBRESET_H

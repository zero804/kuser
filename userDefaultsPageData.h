/**********************************************************************

	--- Qt Architect generated file ---

	File: userDefaultsPageData.h
	Last generated: Thu Dec 10 22:47:22 1998

	DO NOT EDIT!!!  This file will be automatically
	regenerated by qtarch.  All changes will be lost.

 *********************************************************************/

#ifndef userDefaultsPageData_included
#define userDefaultsPageData_included

#include <qwidget.h>
#include <qcheckbox.h>
#include <qlineedit.h>
#include <qcombobox.h>

class userDefaultsPageData : public QWidget
{
    Q_OBJECT

public:

    userDefaultsPageData
    (
        QWidget* parent = NULL,
        const char* name = NULL
    );

    virtual ~userDefaultsPageData();

public slots:


protected slots:


protected:
    QComboBox* shell;
    QLineEdit* home;
    QCheckBox* createHomeDir;
    QCheckBox* copySkel;
    QCheckBox* usePrivateGroup;

};

#endif // userDefaultsPageData_included

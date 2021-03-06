//===========================================
//  Lumina-DE source code
//  Copyright (c) 2014, Susanne Jaeckel
//  Available under the 3-clause BSD license
//  See the LICENSE file for full details
//===========================================
#ifndef _LUMINA_DESKTOP_SWITCHER_H
#define _LUMINA_DESKTOP_SWITCHER_H

#include <QTimer>
#include <QWidget>
#include <QString>
//#include <QX11Info>
#include <QMenu>

#include <LuminaUtils.h>
#include <LuminaXDG.h>
#include <LuminaX11.h>

#include "../LTBWidget.h"
#include "../LPPlugin.h"

//#include <X11/Xlib.h>
//#include <X11/Xutil.h>
//#include <X11/Xatom.h>

class LDesktopSwitcher : public LPPlugin{
	Q_OBJECT
public:
	LDesktopSwitcher(QWidget *parent = 0);
	~LDesktopSwitcher();
	
private:
	QTimer *timer;
	QToolButton *label;
	QMenu *menu;
	int iconOld;

	//void setNumberOfDesktops(int);
	//void setCurrentDesktop(int);
	//int getNumberOfDesktops();
	//int getCurrentDesktop();


	QAction* newAction(int, QString);
	
private slots:
	void createMenu();
	void menuActionTriggered(QAction*);
};

#endif

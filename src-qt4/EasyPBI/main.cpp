#include <QTranslator>
#include <qtsingleapplication.h>
#include <QtGui/QApplication>
#include <QDebug>
#include "mainGUI.h"
#define PREFIX QString("/usr/local")

int main(int argc, char ** argv)
{
    QtSingleApplication a(argc, argv);
    if( a.isRunning() )
      return !(a.sendMessage("show"));

    
    QTranslator translator;
    QLocale mylocale;
    QString langCode = mylocale.name();
    
    if ( ! QFile::exists(PREFIX + "/share/EasyPBI/i18n/EasyPBI_" + langCode + ".qm" ) )  langCode.truncate(langCode.indexOf("_"));
    translator.load( QString("EasyPBI_") + langCode, PREFIX + "/share/EasyPBI/i18n/" );
    a.installTranslator( &translator );
    qDebug() << "Locale:" << langCode;
    

    MainGUI w;
    QObject::connect(&a, SIGNAL(messageReceived(const QString&)), &w, SLOT(slotSingleInstance()) );
    w.show();

    //Look for additional input files and load the first pbi.conf
    if(argc > 1){
      QString input = argv[1];
      if(input.endsWith("pbi.conf")){
	//append the local path if necessary
	QDir dir(input);
	//Now load the module
	w.loadModule(dir.absolutePath());
      }
    }
    int retCode = a.exec();
    return retCode;
}

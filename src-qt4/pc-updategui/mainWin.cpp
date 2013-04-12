/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you want to add, delete, or rename functions or slots, use
** Qt Designer to update this file, preserving your code.
**
** You should not define a constructor or destructor in this file.
** Instead, write your code in functions called init() and destroy().
** These will automatically be called by the form's constructor and
** destructor.
*****************************************************************************/
#include <fcntl.h>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QProcess>
#include <QProgressDialog>
#include <QSocketNotifier>
#include <QString>
#include <QTextStream>
#include <pcbsd-utils.h>
#include "mainWin.h"
#include "../config.h"

void mainWin::ProgramInit(QString ch, QString ip)
{
  // Set any warden directories
  doingUpdate=false;
  lastError="";
  wDir = ch;
  wIP = ip;
  if ( ! wDir.isEmpty() )
     setWindowTitle(tr("Updates for Jail:") + " " + wIP );

  //Grab the username
  //username = QString::fromLocal8Bit(getenv("LOGNAME"));
  connect(buttonRescan, SIGNAL(clicked()), this, SLOT(slotRescanUpdates()));
  connect(pushInstallUpdates, SIGNAL(clicked()), this, SLOT(slotInstallClicked()));
  connect(pushUpdatePkgs, SIGNAL(clicked()), this, SLOT(slotUpdatePkgsClicked()));
  connect(pushClose, SIGNAL(clicked()), this, SLOT(slotCloseClicked()));
  connect(buttonRescanPkgs, SIGNAL(clicked()), this, SLOT(slotRescanPkgsClicked()));
  connect(checkAll, SIGNAL(clicked()), this, SLOT(slotSelectAllClicked()));
  connect(listViewUpdates, SIGNAL(itemClicked(QListWidgetItem *)),this,SLOT(slotListClicked()));
  connect(listViewUpdates, SIGNAL(itemActivated(QListWidgetItem *)),this,SLOT(slotListClicked()));
  connect(listViewUpdates, SIGNAL(itemChanged(QListWidgetItem *)),this,SLOT(slotListClicked()));
  connect(listViewUpdates, SIGNAL(itemPressed(QListWidgetItem *)),this,SLOT(slotListClicked()));
  connect(listViewUpdates, SIGNAL(itemDoubleClicked(QListWidgetItem *)),this,SLOT(slotListDoubleClicked(QListWidgetItem *)));
  progressUpdate->setHidden(true);

  QTimer::singleShot(100, this, SLOT(slotRescanUpdates() ) );
  QTimer::singleShot(200, this, SLOT(slotRescanPkgsClicked() ) );
}

void mainWin::slotListDoubleClicked(QListWidgetItem *cItem)
{
  if ( listUpdates.at(listViewUpdates->row(cItem)).at(7).isEmpty() )
     return;

  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  QString username = env.value( "LOGNAME" );


  QString url = listUpdates.at(listViewUpdates->row(cItem)).at(7);
  qDebug() << url;
  system("su -m " + username.toLatin1() + " -c 'openwith " + url.toLatin1() + "' &"); 
}

//Check whether an update was selected to enable the button
void mainWin::slotListClicked(){
  bool found = false;

  // Get the total number of updates
  for (int z=0; z < listViewUpdates->count(); ++z)
    if ( listViewUpdates->item(z)->checkState() == Qt::Checked )
      found = true;

  if(!found)
    pushInstallUpdates->setEnabled(false);
  else
    pushInstallUpdates->setEnabled(true);
}

bool mainWin::sanityCheck()
{
  bool haveUP, haveMU;
  int sa = 0;
  haveUP = false;
  haveMU = false;

  for (int z=0; z < listViewUpdates->count(); ++z) {
    if ( listViewUpdates->item(z)->checkState() == Qt::Checked ) {
      if ( listUpdates.at(z).at(1) == "PACKAGE") {
	haveUP = true;
      }
      if ( listUpdates.at(z).at(1) == "PATCH") {
	haveUP = true;
	if ( listUpdates.at(z).at(5) == "YES" ) 
	  sa++;
      }
      if ( listUpdates.at(z).at(1) == "SYSUPDATE") {
	haveMU = true;
      }
    }
  }

  if ( (haveMU && haveUP ) || sa > 1 ) {
    QMessageBox::warning(this, tr("Update Conflict"), tr("More than one stand-alone update has been selected! Please unselect all other updates and try again."));
    return false;
  }

  return true;
}

void mainWin::doUpdates()
{
  // Set our UI elements
  progressUpdate->setHidden(false);
  tabUpdates->setEnabled(false);

  curUpdate = -1;
  curUpdateIndex = 0;
  totUpdate = 0;

  // Get the total number of updates
  for (int z=0; z < listViewUpdates->count(); ++z)
    if ( listViewUpdates->item(z)->checkState() == Qt::Checked )
      totUpdate++;

  // Start our loop processing updates
  slotUpdateLoop();
}

void mainWin::slotUpdateLoop()
{
  QString tmp, tmp2, mUrl, PkgSet, Version, Arch;

  // Check if the last update process finished
  if ( curUpdate != -1 ) {
    qDebug() << "Finished Update";
    if ( uProc->exitStatus() != QProcess::NormalExit || uProc->exitCode() != 0)
    {
      // Read any remaining buffers
      slotReadUpdateOutput();

      // Warn user that this update failed
      if ( lastError.isEmpty() )
         QMessageBox::critical(this, tr("Update Failed!"), tr("Failed to install:") + listUpdates.at(curUpdate).at(0) + " " + tr("An unknown error occured!")); 
      else
         QMessageBox::critical(this, tr("Update Failed!"), tr("Failed to install:") + listUpdates.at(curUpdate).at(0) + " " + lastError); 
    } else {
      // If successfull system update download
      if ( listUpdates.at(curUpdate).at(1) == "SYSUPDATE" )
        QMessageBox::information(this, tr("Update Ready"), tr("Please reboot to start the update to PC-BSD version \"") + listUpdates.at(curUpdate).at(0) + "\". " + tr("This process may take a while, please do NOT interrupt the process.")); 
    }

    listViewUpdates->item(curUpdate)->setIcon(QIcon());
    setWindowTitle(tr("Update Manager"));
  }

  // Start looking for the next update
  for (int z=0; z < listViewUpdates->count(); ++z) {
    if ( listViewUpdates->item(z)->checkState() == Qt::Checked && curUpdate < z ) 
    {
      dPackages = false;
      uPackages = false;
      curUpdate = z;
      curUpdateIndex++;
      progressUpdate->setHidden(false);
      progressUpdate->setRange(0, 0);
      tmp.setNum(curUpdateIndex);
      tmp2.setNum(totUpdate);
      setWindowTitle(tr("Updating:") + " " + listUpdates.at(z).at(0));

      textLabel->setText(tr("Starting Update: %1 (%2 of %3)")
                         .arg(listUpdates.at(z).at(0))
 			 .arg(tmp)
		 	 .arg(tmp2));

      // Get the icon
      listViewUpdates->item(z)->setIcon(QIcon(":images/current-item.png"));

      // Get the install tag
      QString tag;
      if ( listUpdates.at(z).at(1) == "SYSUPDATE" )
        tag = listUpdates.at(z).at(4);
      if ( listUpdates.at(z).at(1) == "PATCH" )
        tag = listUpdates.at(z).at(3);

      // Show tray that we are doing a download
      QFile sysTrig( SYSTRIGGER );
      if ( sysTrig.open( QIODevice::WriteOnly ) ) {
        QTextStream streamTrig( &sysTrig );
        streamTrig << "DOWNLOADING: ";
      }

      // Setup the upgrade process
      uProc = new QProcess();
      QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
      env.insert("PCFETCHGUI", "YES");
      uProc->setProcessEnvironment(env);
      uProc->setProcessChannelMode(QProcess::MergedChannels);

      // Connect the slots
      connect( uProc, SIGNAL(readyReadStandardOutput()), this, SLOT(slotReadUpdateOutput()) );
      connect( uProc, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(slotUpdateLoop()) );

      // If doing FreeBSD Update run freebsd-update cmd
      if ( wDir.isEmpty() ) {
         if ( listUpdates.at(z).at(1) == "FBSDUPDATE" ) {
           uProc->start("freebsd-update", QStringList() << "install"); 
         } else {
           uProc->start("pc-updatemanager", QStringList() << "install" << tag ); 
	 }
      } else {
	 // Doing a warden update in a chroot environment
         if ( listUpdates.at(z).at(1) == "FBSDUPDATE" ) {
           uProc->start("chroot", QStringList() << wDir << "freebsd-update" << "install"); 
         } 
      }
      qDebug() << "Update started";
      return;
    }

  }

  // If we get here, no more updates to do
  slotUpdateFinished();
}

void mainWin::slotReadUpdateOutput()
{
  QString line, cI, tI, tmp;
  bool ok, ok2;
  cI.setNum(curUpdateIndex);
  tI.setNum(totUpdate);


  while (uProc->canReadLine()) {
    line = uProc->readLine().simplified();

    if ( line.indexOf("FETCH:") == 0 ) {
      tmp = line;
      tmp = tmp.remove(0, tmp.lastIndexOf("/") + 1);
      textLabel->setText(tr("Downloading: %1 (Update %2 of %3)")
			 .arg(tmp)
			 .arg(cI)
		    	 .arg(tI));
      continue;
    }
    
    if ( line.indexOf("SIZE:") == 0 ) {
      line.section(" ", 1,1).toInt(&ok);
      line.section(" ", 3,3).toInt(&ok2);
      if ( ok && ok2 ) {
        progressUpdate->setRange(0, line.section(" ", 1,1).toInt(&ok));
        progressUpdate->setValue(line.section(" ", 3,3).toInt(&ok));
      }
      continue;
    }
    if ( line.indexOf("FETCHDONE") == 0 ) {
      progressUpdate->setRange(0, 0);
      textLabel->setText(tr("Updating: %1 (%2 of %3)")
			 .arg(listUpdates.at(curUpdate).at(0))
			 .arg(cI)
			 .arg(tI));
      continue;
    }

    if ( line.indexOf("TOTALSTEPS:") == 0 ) {
      line.section(" ", 1,1).toInt(&ok);
      if ( ok )
        progressUpdate->setRange(0, line.section(" ", 1, 1).toInt(&ok));
      continue;
    }
    if ( line.indexOf("SETSTEPS:") == 0 ) {
      line.section(" ", 1,1).toInt(&ok);
      if ( ok )
        progressUpdate->setValue(line.section(" ", 1, 1).toInt(&ok));
      continue;
    }
    if ( line.indexOf("ERROR:") == 0 ) {
       lastError = line;
       continue; 
    }
    qDebug() << line;
  }
}

void mainWin::slotUpdateFinished()
{
  qDebug() << "Updates all Finished";
  QFile sysTrig( SYSTRIGGER );
  if ( sysTrig.open( QIODevice::WriteOnly ) ) {
    QTextStream streamTrig( &sysTrig );
     streamTrig << "INSTALLFINISHED: ";
  }
  sysTrig.close();

  progressUpdate->setHidden(true);
  slotRescanUpdates();
}

void mainWin::slotInstallClicked()
{
  // Sanity check our install choices
  if (!sanityCheck())
    return;

  // Start the installation
  doUpdates();
}

void mainWin::slotSelectAllClicked()
{
  for (int z=0; z < listViewUpdates->count(); ++z) {
    listViewUpdates->item(z)->setCheckState(checkAll->checkState());	
  }

  slotListClicked();
}

void mainWin::slotRescanUpdates()
{
  if ( doingUpdate )
     return;
  groupUpdates->setEnabled(false);
  listUpdates.clear();
  textLabel->setText(tr("Checking for updates... Please Wait..."));
  slotReadUpdateData();
  slotDisplayUpdates();
  qDebug() << listUpdates;
  //disable the "select all" checkbox if no updates available
  if(listUpdates.isEmpty() ){
    checkAll->setEnabled(false);
  }
  pushInstallUpdates->setEnabled(false); //disable the button until an update is selected
  if ( ! doingUpdate )
    groupUpdates->setEnabled(true);
}

void mainWin::slotDisplayUpdates()
{
  if ( ! doingUpdate )
    tabUpdates->setEnabled(true);
  listViewUpdates->clear();

  // Check if the system has an upgrade available
  if ( QFile::exists("/usr/local/tmp/update-stagedir/doupdate.sh") ) {
    textLabel->setText(tr("A system upgrade is waiting to be installed. Please reboot to begin!"));
    return;
  }


  // Any system updates?
  if ( listUpdates.isEmpty() ) {
    if ( ! pushUpdatePkgs->isEnabled() ) {
      textLabel->setText(tr("Your system is fully updated!"));
      tabUpdates->setTabText(0, tr("System Updates"));
    } else {
      textLabel->setText(tr("Package updates available!"));
    }
    groupUpdates->setTitle("");
    return;
  }

  textLabel->setText(tr("System updates available!"));
  groupUpdates->setTitle(tr("Available Updates"));
  tabUpdates->setCurrentIndex(0);
  tabUpdates->setTabText(0, tr("System Updates (%1)").arg(listUpdates.count()));

  // Start parsing the updates and list whats available
  for (int z=0; z < listUpdates.count(); ++z) {
    if ( listUpdates.at(z).at(1) == "SYSUPDATE" ) {
      QListWidgetItem *item = new QListWidgetItem(tr("System Upgrade: %1 (%2)")
						  .arg(listUpdates.at(z).at(2))
						  .arg(listUpdates.at(z).at(3)));
      item->setToolTip(tr("PC-BSD Version:") + "<br>" + listUpdates.at(z).at(2) + "<hr>" + tr("This update must be installed by itself.") + "<br>" + tr("Creating a backup of your data first is recommended."));
      item->setCheckState(Qt::Unchecked);
      listViewUpdates->addItem(item);
    }
    if ( listUpdates.at(z).at(1) == "PATCH" ) {
      QListWidgetItem *item = new QListWidgetItem(tr("Patch: %1 (%2)")
						  .arg(listUpdates.at(z).at(0))
					  	  .arg(listUpdates.at(z).at(2)));
      item->setCheckState(Qt::Unchecked);
      item->setToolTip(tr("This is a patch for your version of PC-BSD") + "<hr>" + tr("Patch Size:") + " " + listUpdates.at(z).at(4) + "MB<br>");
      listViewUpdates->addItem(item);
    }

    if ( listUpdates.at(z).at(1) == "FBSDUPDATE" ) {
      QString fileNameList;
      for (int p=2; p < listUpdates.at(z).count(); p++)
	fileNameList += listUpdates.at(z).at(p) + "<br>";

      QListWidgetItem *item = new QListWidgetItem(tr("FreeBSD Security Update"));
      item->setCheckState(Qt::Unchecked);
      item->setToolTip(tr("The following files need updating:") + "<hr>" + fileNameList);
      
      listViewUpdates->addItem(item);
    }

    if ( listUpdates.at(z).at(1) == "PACKAGE" ) {
      QString pkgNameList;
      for (int p=2; p < listUpdates.at(z).count(); p=p+5) {
	if ( listUpdates.at(z).count() < p + 4 ) 
	  break;

	pkgNameList += listUpdates.at(z).at((p)) + " " + listUpdates.at(z).at((p+1)) + " -> " + listUpdates.at(z).at((p+2)) + "<br>";
      }
      QListWidgetItem *item = new QListWidgetItem(tr("System Package Updates"));
      item->setCheckState(Qt::Unchecked);
      item->setToolTip(tr("The following package updates are available:") + "<hr>" + pkgNameList);
      
      listViewUpdates->addItem(item);
    }
  }
}

void mainWin::slotRescanPkgsClicked()
{
  // Check for pkg updates
  checkMPKGUpdates();
}

void mainWin::slotReadUpdateData()
{
  // If on the base system, check for PC-BSD updates
  if ( wDir.isEmpty() )
    checkPCUpdates();

  // Check for FreeBSD Updates now
  checkFBSDUpdates();

}

void mainWin::checkPCUpdates() {

  QString line, tmp, name, type, version, date, tag, url, size, sa, rr;
  QStringList up, listPkgs;

  QProcess p;
  p.start(QString("pc-updatemanager"), QStringList() << "check");
  while(p.state() == QProcess::Starting || p.state() == QProcess::Running)
     QCoreApplication::processEvents();

  while (p.canReadLine()) {
    line = p.readLine().simplified();
    if ( line.indexOf("NAME: ") == 0) {
       name = line.replace("NAME: ", "");
       continue;
    }
    if ( line.indexOf("TYPE: ") == 0) {
       type = line.replace("TYPE: ", "");
       continue;
    }

    if ( type == "SYSUPDATE" ) {
      if ( line.indexOf("VERSION: ") == 0) {
         version = line.replace("VERSION: ", "");
         continue;
      }
      if ( line.indexOf("DATE: ") == 0) {
         date = line.replace("DATE: ", "");
         continue;
      }
      if ( line.indexOf("TAG: ") == 0) {
         tag = line.replace("TAG: ", "");
         continue;
      }
      if ( line.indexOf("DETAILS: ") == 0) {
         url = line.replace("DETAILS: ", "");
         continue;
      }

      if ( line.indexOf("To install:") == 0) {
         up.clear();
	 up << name << type << version << date << tag << url;
	 listUpdates.append(up);
         type=""; name="", version="", date="", tag="", url="";
         continue;
      }

    }
    if ( type == "PATCH" ) {
      if ( line.indexOf("DATE: ") == 0) {
         date = line.replace("DATE: ", "");
         continue;
      }
      if ( line.indexOf("TAG: ") == 0) {
         tag = line.replace("TAG: ", "");
         continue;
      }
      if ( line.indexOf("SIZE: ") == 0) {
         size = line.replace("SIZE: ", "");
         continue;
      }
      if ( line.indexOf("STANDALONE: ") == 0) {
         sa = line.replace("STANDALONE: ", "");
         continue;
      }
      if ( line.indexOf("REQUIRESREBOOT: ") == 0) {
         rr = line.replace("REQUIRESREBOOT: ", "");
         continue;
      }
      if ( line.indexOf("DETAILS: ") == 0) {
         url = line.replace("DETAILS: ", "");
         continue;
      }
      if ( line.indexOf("To install:") == 0) {
	 // TODO add this update to list
         up.clear();
	 up << name << type << date << tag << size << sa << rr << url;
	 listUpdates.append(up);
         type=""; name="", date="", tag="", size="", sa="", rr="", url="";
         continue;
      }
    }

  }


}

void mainWin::checkMPKGUpdates() {
  if ( doingUpdate )
     return;

  QString line, tmp, name, pkgname, pkgover, pkgnver;
  QStringList up, listPkgs;
  bool haveUpdates = false;
  int totPkgs=0;
  buttonRescanPkgs->setEnabled(false);
  pushUpdatePkgs->setEnabled(false);
  listViewUpdatesPkgs->clear();
  groupUpdatesPkgs->setTitle(tr("Checking for updates"));

  QProcess p;
  if ( wDir.isEmpty() )
     p.start(QString("pc-updatemanager"), QStringList() << "pkgcheck");
  else
     p.start(QString("chroot"), QStringList() << wDir << "pc-updatemanager" << "pkgcheck");
  while(p.state() == QProcess::Starting || p.state() == QProcess::Running)
     QCoreApplication::processEvents();

  while (p.canReadLine()) {
    line = p.readLine().simplified();
    qDebug() << line;
    if ( line.indexOf("Upgrading") != 0) {
       continue;
    }
    tmp = line;
    pkgname = tmp.section(" ", 1, 1);
    pkgname.replace(":", "");
    pkgover = tmp.section(" ", 2, 2);
    pkgnver = tmp.section(" ", 4, 4);
    QTreeWidgetItem *myItem = new QTreeWidgetItem(QStringList() << pkgname << pkgover << pkgnver);
    listViewUpdatesPkgs->addTopLevelItem(myItem);
    haveUpdates = true;
    totPkgs++;
  }

  buttonRescanPkgs->setEnabled(true);
  pushUpdatePkgs->setEnabled(haveUpdates);
  if ( totPkgs > 0 ) {
    if ( listUpdates.isEmpty() )
      tabUpdates->setCurrentIndex(1);
    tabUpdates->setTabText(1, tr("Package Updates (%1)").arg(totPkgs));
    groupUpdatesPkgs->setTitle(tr("Available updates"));
  } else {
    tabUpdates->setTabText(1, tr("Package Updates"));
    groupUpdatesPkgs->setTitle(tr("No available updates"));
  }
 
  slotDisplayUpdates();
}


void mainWin::checkFBSDUpdates() {
  QString line;
  QStringList up, listPkgs;

  // Now check if there are freebsd-updates to install
  QProcess f;
  if ( wDir.isEmpty() )
     f.start(QString("pc-fbsdupdatecheck"), QStringList());
  else {
     QProcess::execute("cp /usr/local/bin/pc-fbsdupdatecheck " + wDir + "/tmp/.fbupdatechk");
     QProcess::execute("chmod 755 " + wDir + "/tmp/.fbupdatechk");
     f.start(QString("chroot"), QStringList() << wDir << "/tmp/.fbupdatechk" << "fetch" );
  }
  while(f.state() == QProcess::Starting || f.state() == QProcess::Running)
     QCoreApplication::processEvents();

  bool fUp = false;
  
  while (f.canReadLine()) {
    line = f.readLine().simplified();
    qDebug() << line;
    if ( line.indexOf("The following files will be updated ") == 0) {
       fUp = true;
       listPkgs.clear();
       continue;
    }

    if ( fUp )
       listPkgs << line;
  }

  if ( ! wDir.isEmpty() )
     QProcess::execute("rm " + wDir + "/tmp/.fbupdatechk");

  // Are there freebsd updates to install?
  if ( fUp ) {
    up.clear();
    up << "FreeBSD Security Updates" << "FBSDUPDATE";
    up.append(listPkgs);
    listUpdates.append(up);
  }

}

void mainWin::slotSingleInstance() {
   this->hide();
   this->showNormal();
   this->activateWindow();
   this->raise();
}

void mainWin::slotCloseClicked() {
   close();
}

void mainWin::slotUpdatePkgsClicked() {
  // Set our UI elements
  progressUpdate->setHidden(false);
  tabUpdates->setEnabled(false);

  dPackages = false;
  uPackages = false;
  doingUpdate=true;
  curUpdate = 0;

  // Setup the upgrade process
  uProc = new QProcess();
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert("PCFETCHGUI", "YES");
  uProc->setProcessEnvironment(env);
  uProc->setProcessChannelMode(QProcess::MergedChannels);

  // Connect the slots
  connect( uProc, SIGNAL(readyReadStandardOutput()), this, SLOT(slotReadPkgUpdateOutput()) );
  connect( uProc, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(slotUpdatePkgDone()) );

  if ( wDir.isEmpty() )
    uProc->start("pc-updatemanager", QStringList() << "pkgupdate");
  else
    uProc->start("chroot", QStringList() << wDir << "pc-updatemanager" << "pkgupdate");

  textLabel->setText(tr("Starting package updates..."));

  progressUpdate->setRange(0, listViewUpdatesPkgs->topLevelItemCount() );
  progressUpdate->setValue(0);
}

void mainWin::slotReadPkgUpdateOutput() {
   QString line, tmp, cur, tot, fname;

   while (uProc->canReadLine()) {
     line = uProc->readLine().simplified();
     qDebug() << "Normal Line:" << line;
     tmp = line;
     tmp.truncate(50);
     if ( line.indexOf("to be downloaded") != -1 ) {
       textLabel->setText(tr("Downloading packages..."));
       curUpdate = 0;
       progressUpdate->setValue(0);
       continue;
     }
     if ( line.indexOf("Checking integrity") == 0 ) {
       textLabel->setText(line);
       uPackages = true;
       dPackages = false;
       curUpdate = 0;
       progressUpdate->setValue(0);
     }
     if ( line.indexOf("FETCH: ") == 0 ) { 
	progressUpdate->setValue(progressUpdate->value() + 1); 
	tmp = line; 
	tmp = tmp.remove(0, tmp.lastIndexOf("/") + 1); 
	textLabel->setText(tr("Downloading: %1").arg(tmp)); 
        continue;
     } 
     
     if ( line.indexOf("SIZE: ") == 0 ) {
          bool ok, ok2;
     	  line.replace("SIZE: ", "");
          line.replace("DOWNLOADED: ", "");
          line.replace("SPEED: ", "");
          line.section(" ", 0, 0).toInt(&ok);
          line.section(" ", 1, 1).toInt(&ok2);
   
          if ( ok && ok2 ) {
	    QString unit;
            int tot = line.section(" ", 0, 0).toInt(&ok);
            int cur = line.section(" ", 1, 1).toInt(&ok2);
            QString percent = QString::number( (float)(cur * 100)/tot );
            QString speed = line.section(" ", 2, 3);

  	    // Get the MB downloaded / total
	    if ( tot > 2048 ) {
	      unit="MB";
	      tot = tot / 1024;
       	      cur = cur / 1024;
	    } else {
	      unit="KB";
            }

            QString ProgressString=QString("%1" + unit + " of %2" + unit + " at %3").arg(cur).arg(tot).arg(speed);
            progressUpdate->setRange(0, tot);
            progressUpdate->setValue(cur);
         }
     }

     if ( uPackages ) {
       if ( line.indexOf("Upgrading") == 0 ) {
         textLabel->setText(line);
         curUpdate++;
         progressUpdate->setValue(curUpdate);
       }
       continue;
     }

   } // end of while
}

// Function to read output of pipefile
void mainWin::slotReadEventPipe(int fd) {
  QString tmp, fname, cur, tot;
  bool ok, ok2;
  char buff[4028];
  int totread = read(fd, buff, 4020);
  buff[totread]='\0';
  QString line = buff;
  line = line.simplified();
  //qDebug() << "Found line:" << line;
  
  if ( line.indexOf("INFO_FETCH") != -1  && dPackages ) {
     tmp = line;
     fname = tmp.section(":", 4, 4);
     fname.remove(0, fname.lastIndexOf('/') + 1);
     fname  = fname.section('"', 0, 0);
     cur = tmp.section(":", 5, 5);
     cur = cur.remove(',');
     cur = cur.section(" ", 1, 1);
     cur = cur.simplified();
     tot = tmp.section(":", 6, 6);
     tot = tot.simplified();
     tot = tot.remove(',');
     tot = tot.section("}", 0, 0);

     textLabel->setText(tr("Downloading %1").arg(fname));
     tot.toInt(&ok);
     cur.toInt(&ok2);
     if ( ok && ok2 )
     { 
       progressUpdate->setRange(0, tot.toInt(&ok2));
       progressUpdate->setValue(cur.toInt(&ok2));
     }
     
     //qDebug() << "File:" << fname << "cur" << cur << "tot" << tot;
  }
}

void mainWin::slotUpdatePkgDone() {
  progressUpdate->setHidden(true);
  QFile sysTrig( SYSTRIGGER );
  if ( sysTrig.open( QIODevice::WriteOnly ) ) {
    QTextStream streamTrig( &sysTrig );
     streamTrig << "INSTALLFINISHED: ";
  }

  if ( uProc->exitCode() != 0 )
    QMessageBox::warning(this, tr("Failed package update"), tr("The package update failed! If this persists, you may need to manually run: pc-updatemanager pkgupdate"));

  doingUpdate=false;
  checkMPKGUpdates();
}

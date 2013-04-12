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
#include "wizardNewJail.h"
#include "pcbsd-utils.h"
#include <QDebug>
#include <QFileDialog>


void wizardNewJail::programInit()
{
    // Connect our checks
    connect(lineRoot, SIGNAL(textChanged ( const QString & )), this, SLOT(slotCheckComplete() ) );
    connect(lineRoot2, SIGNAL(textChanged ( const QString & )), this, SLOT(slotCheckComplete() ) );
    connect(lineIP, SIGNAL(textChanged ( const QString & )), this, SLOT(slotCheckComplete() ) );
    connect(lineHost, SIGNAL(textChanged ( const QString & )), this, SLOT(slotCheckComplete() ) );
    connect(lineLinuxScript, SIGNAL(textChanged ( const QString & )), this, SLOT(slotCheckComplete() ) );
    connect(pushLinuxScript, SIGNAL(clicked()), this, SLOT(slotSelectLinuxScript()) );
    connect(this, SIGNAL(currentIdChanged(int)), this, SLOT(slotCheckComplete()) );
}

void wizardNewJail::setHostIPUsed(QStringList uH, QStringList uIP)
{
   usedHosts = uH;
   usedIP = uIP;
}

void wizardNewJail::accept()
{
    
    emit create(lineIP->text(), lineHost->text(), radioTraditionalJail->isChecked(),
                lineRoot->text(), checkSystemSource->isChecked(), checkPortsTree->isChecked(),
                checkAutostart->isChecked(), radioLinuxJail->isChecked(), lineLinuxScript->text());
    close();
    
}

void wizardNewJail::slotClose()
{
  close();
}

// Logic checks to see if we are ready to move onto next page
bool wizardNewJail::validatePage()
{
  QColor red = QColor(255, 78, 78);
  QColor white = QColor(255, 255, 255);
  QPalette badPal(red);
  badPal.setColor(QPalette::Window,red);
  badPal.setColor(QPalette::WindowText,red);
  QPalette goodPal(white);
  goodPal.setColor(QPalette::Window,white);
  goodPal.setColor(QPalette::WindowText,white);
  labelMessage->setText(QString());


  switch (currentId()) {
     case Page_IP:
         // Make sure items are not empty
         if ( lineIP->text().isEmpty() ) {
            button(QWizard::NextButton)->setEnabled(false);
            return false;
	 }
         if ( lineHost->text().isEmpty() ) {
            button(QWizard::NextButton)->setEnabled(false);
            return false;
	 }

	 // Check if this IP / Host is already used
         for (int i = 0; i < usedHosts.size(); ++i)
            if ( usedHosts.at(i) == lineHost->text() ) {
               button(QWizard::NextButton)->setEnabled(false);
               lineHost->setPalette(badPal);
               labelMessage->setText(tr("Hostname already used!"));
               return false;
	    }
         for (int i = 0; i < usedIP.size(); ++i)
            if ( usedIP.at(i) == lineIP->text() ) {
               button(QWizard::NextButton)->setEnabled(false);
               lineIP->setPalette(badPal);
               labelMessage->setText(tr("IP already used!"));
               return false;
	    }

         // Check if we have a good IPV4 or IPV6 address
	 if ( ! Utils::validateIPV4(lineIP->text()) && ! Utils::validateIPV6(lineIP->text()) ) {
           button(QWizard::NextButton)->setEnabled(false);
           lineIP->setPalette(badPal);
           labelMessage->setText(tr("Invalid IP address!"));
           return false;
         }

         // Got to the end, must be good!
         lineIP->setPalette(goodPal);
         lineHost->setPalette(goodPal);
         button(QWizard::NextButton)->setEnabled(true);
         return true;
     case Page_Root:
	 if ( lineRoot->text() != lineRoot2->text() ) {
           button(QWizard::NextButton)->setEnabled(false);
	   return false;
         } else {
           button(QWizard::NextButton)->setEnabled(true);
	 }
         return true;
     case Page_Linux:
	  if ( lineLinuxScript->text().isEmpty() ) {
            button(QWizard::NextButton)->setEnabled(false);
	    return false;
	  }
     default:
         button(QWizard::NextButton)->setEnabled(true);
         return true;
  }

  return true;
}

int wizardNewJail::nextId() const
{
  switch (currentId()) {
     case Page_Type:
         if (radioPortsJail->isChecked())
           return Page_Opts;
       break;
       case Page_Root:
	 checkSystemSource->setHidden(false); 
	 checkPortsTree->setHidden(false); 
         if (radioLinuxJail->isChecked()) {
	   labelConfirm->setText(tr("Please take a moment and set any other options for this jail."));
           return Page_Linux;
	 } else {
           return Page_Opts;
	 }
       break;
       case Page_Linux:
	   checkSystemSource->setHidden(true); 
	   checkPortsTree->setHidden(true); 
           return Page_Opts;
       break;
       case Page_Opts:
         return -1;
       break;
     default:
        return currentId() + 1;
  }
  return currentId() + 1;
}

void wizardNewJail::slotCheckComplete()
{
   // Validate this page
   validatePage();
}

void wizardNewJail::slotSelectLinuxScript()
{
   lineLinuxScript->setText(QFileDialog::getOpenFileName(this,
   tr("Select Script"), "/usr/local/share/warden/linux-installs", tr("Linux install files (*)")));
}



/* PCDM Login Manager:
*  Written by Ken Moore (ken@pcbsd.org) 2012/2013
*  Modified by Kris Moore (kris@pcbsd.org) 2013
*  Copyright(c) 2013 by the PC-BSD Project
*  Available under the 3-clause BSD license
*/

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pwd.h>
#include <login_cap.h>
#include <QMessageBox>
#include <QTemporaryFile>

/*
Sub-classed QProcess for starting an XSession Process
*/

#include "pcdm-xprocess.h"

XProcess::XProcess() : QProcess(0) {
  //initialize the variables
  xuser.clear();
  xcmd.clear();
  xhome.clear();
  xpwd.clear();
  xshell.clear();
  pam_started = FALSE;
  pam_session_open = FALSE;
  //Setup the finished signal/slot
  //connect( this, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(slotCleanup()) );
}

XProcess::~XProcess(){
  if( isRunning() ){
    this->terminate();
  }
  this->close();
}

void XProcess::loginToXSession(QString username, QString password, QString desktop, QString lang){
  //Setup the variables
  xuser = username;
  xpwd = password;
  xhome = Backend::getUserHomeDir(xuser);
  xcmd = Backend::getDesktopBinary(desktop);
  xshell = Backend::getUserShell(xuser);
  xde = desktop;
  xlang = lang;
  //Now start the login process
  if( !startXSession() ){
    //Could not continue after session changed significantly - close down the session to restart
    QCoreApplication::exit(-1); //special code to make sure we are just restarting the PCDM session (not the full X session)
  }
}

bool XProcess::isRunning(){
  if(this->state() != QProcess::NotRunning){ return TRUE; }
  else{ return FALSE; }
}

void XProcess::waitForSessionClosed(){
  // CAUTION!! 
  // This function will pause the calling program to wait for the session to end!
  if( isRunning() ){ this->waitForFinished(-1); }

}

/*
 ========== SESSION STARTUP ==========
*/

bool XProcess::startXSession(){
  //Returns TRUE if the session can continue, or FALSE if it needs to be closed down

  //Check that the necessary info to start the session is available
  if( xuser.isEmpty() || xcmd.isEmpty() || xhome.isEmpty() || xde.isEmpty() ){
    emit InvalidLogin();  //Make sure the GUI knows that it was a failure
    return true;
  }
  //Backend::log("Starting up Desktop environment ("+xcmd+") as user ("+xuser+")");
  
  //Check for PAM username/password validity
  if( !pam_checkPW() ){ emit InvalidLogin(); pam_shutdown(); return true; }


  //Save the current user/desktop as the last login
  Backend::saveLoginInfo(xuser,xde);

  // Get the users uid/gid information
  struct passwd *pw;
  int uid;
  char *ok;

  if (!(pw = getpwnam(xuser.toLatin1()))) {
      uid = strtol(xuser.toLatin1(), &ok, 10);
      if (!(pw = getpwuid(uid))) {
          return FALSE;
      }
  }

  //Check/create the user's home-dir before dropping privs
  if(!QFile::exists(xhome)){
    QString hmcmd = "pw usermod "+xuser+" -m";
    QProcess::execute(hmcmd);
  }
  
  // Get the environment before we drop priv
  this->setProcessEnvironment( QProcessEnvironment::systemEnvironment() ); //current environment
  //Emit the last couple logs before dropping privileges
  Backend::log("Starting session:");
  Backend::log(" - Session Log: ~/.pcdm-startup.log");
  //Now allow this user access to the Xserver
  QString xhostcmd = "xhost si:localuser:"+xuser;
  QProcess::execute(xhostcmd);
  //And finally set the login user before dropping priv
  setlogin( xuser.toUtf8() );

  //Startup the PAM session
  if( !pam_startSession() ){ pam_shutdown(); return FALSE; }
  pam_session_open = TRUE; //flag that pam has an open session

  QString cmd;

  // Setup the process environment
  setupSessionEnvironment();

  // Create our startup script
  tFile = new QTemporaryFile();
  if ( ! tFile->open() )
     return FALSE;

  QTextStream tOut(tFile);

  // Configure the DE startup command
  cmd.append("dbus-launch --exit-with-session "+xcmd);

  //  - Setup to run the user's <home-dir>/.xprofile startup script
  QFile::setPermissions(xhome+"/.xprofile", QFile::permissions(xhome+"/.xprofile") | QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther );

  //Need to run a couple commands in sequence: so put them in a script file
  tOut << "#!/bin/sh\n\n";
  tOut << "if [ -e '"+xhome+"/.xprofile' ] ; then\n";
  tOut << "  . "+xhome+"/.xprofile\n";
  tOut << "fi\n";
  tOut << cmd + "\n"; //+ " >" + xhome+ "/.pcdm-startup.log" + " 2>" + xhome + "/.pcdm-startup.log\n";
  tOut << "exit $?"; //Make sure we return the DE return value

  QString tUid, tGid, logFile;
  tUid.setNum(pw->pw_uid);
  tGid.setNum(pw->pw_gid);
  logFile=xhome + "/.pcdm-startup.log";
  cmd = "/usr/local/share/PCDM/pcdm-session "+xuser+" "+tUid+" "+tGid+" "+tFile->fileName()+" "+logFile;
  connect( this, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(slotCleanup()) );
  tFile->setPermissions(QFile::ReadOwner | QFile::WriteOwner |QFile::ReadGroup | QFile::ReadUser | QFile::ReadOther);
  tFile->close();

  Backend::log("Starting session with:\n" + cmd );
  this->start(cmd);
  return TRUE;
}

void XProcess::slotCleanup(){
  Backend::log("Session Finished\n - Return Code: "+ QString::number(this->exitCode()) );
  pam_shutdown(); //make sure that PAM shuts down properly
  //Now remove this user's access to the Xserver
  QString xhostcmd = "xhost -si:localuser:"+xuser;
  system(xhostcmd.toUtf8());
}

void XProcess::setupSessionEnvironment(){
  // Setup any specialized environment variables
  QProcessEnvironment environ = this->processEnvironment();
  // Check the current locale code
  QString langCode = xlang;
  if( langCode.toLower() == "c" ){ langCode = "en_US"; } // default to the US english (PCDM code default), LANG=C causes problems
  if(!environ.value("MM_CHARSET").isEmpty() ){ langCode.append( "."+environ.value("MM_CHARSET") ); }
  else{ langCode.append(".UTF-8"); }
  // USER, HOME, and SHELL are set by the "su" login
  environ.insert("LOGNAME",xuser); //Login name
  environ.insert("USERNAME",xuser); // Username
  environ.insert("USER",xuser); // Username
  environ.insert("PATH",environ.value("PATH")+":"+xhome+"/bin"); // Append the user's home dir to the path
  environ.insert("LANG",langCode); //Set the proper localized language
  environ.insert("LC_ALL",langCode); //additional localization variable for some DE's
  environ.insert("MAIL","/var/mail/"+xuser); //Set the mail variable
  environ.insert("GROUP",xuser); //Set the proper group id
  environ.insert("SHLVL","0"); //Set the proper shell level
  environ.insert("HOME",xhome); //Set the users home directory
  environ.insert("SHELL",xshell); //Set the user's default shell
  environ.insert("PCDM_SESSION", Backend::getNLDesktopName(xde).toUpper() ); //List the desktop environment that is being started
  this->setProcessEnvironment(environ);
  this->setWorkingDirectory(xhome); //set the current directory to the user's home directory
}

//Stand-alone function to check a username/password combination for validity
void XProcess::checkPW(QString user, QString pwd){
  xuser = Backend::getUsernameFromDisplayname(user); 
  xpwd = pwd;
  bool ok = pam_checkPW();
  pam_shutdown();
  xuser.clear();
  xpwd.clear();
  if(ok){ emit ValidLogin(); }
  else{ emit InvalidLogin(); }
}

/*
 ========== PAM FUNCTIONS ==========
*/
static struct pam_conv pamc = { openpam_nullconv, NULL };

bool XProcess::pam_checkPW(){
 //Requires internal "xuser" and "xpwd" variables to be set
	
//Convert the inputs to C character arrays for use in PAM
  QByteArray tmp = xuser.toUtf8();
  char* cUser = tmp.data();
  QByteArray tmp2 = xpwd.toUtf8();
  char* cPassword = tmp2.data();
  //initialize variables
  bool result = FALSE;
  int ret;
  //Initialize PAM
  ret = pam_start("login", cUser, &pamc, &pamh);
  if( ret == PAM_SUCCESS ){
    pam_started = TRUE; //flag that pam is started
    //Place the user-supplied password into the structure 
    ret = pam_set_item(pamh, PAM_AUTHTOK, cPassword);
    //Set the TTY 
    //ret = pam_set_item(pamh, PAM_TTY, "pcdm-terminal");
    //Authenticate with PAM
    ret = pam_authenticate(pamh,0);
    if( ret == PAM_SUCCESS ){
      //Check for valid, unexpired account and verify access restrictions
      ret = pam_acct_mgmt(pamh,0);
      if( ret == PAM_SUCCESS ){ result = TRUE; }
    
    }else{
      pam_logFailure(ret);
    }
  }
  //return verification result
  return result;	
}

bool XProcess::pam_startSession(){
  //This should only be run if pam_checkPW was successful
  int ret = pam_open_session(pamh,0);
  bool ok = FALSE;
  if(ret == PAM_SUCCESS){ ok = TRUE; }
  else{ pam_logFailure(ret); }
  
  return ok;
}

bool XProcess::pam_stopSession(){
  //This should only be run if pam_startSession was successful
  int ret = pam_close_session(pamh,0);
  bool ok = FALSE;
  if(ret == PAM_SUCCESS){ ok = TRUE; }
  else{ pam_logFailure(ret); }
  
  return ok;
}

void XProcess::pam_logFailure(int ret){
  //Interpret a PAM error message and log it
  Backend::log("PAM Error: " + QString::number(ret));
  switch( ret ){
  case PAM_ABORT:
    Backend::log(" - PAM abort error");
    break;
  case PAM_AUTHINFO_UNAVAIL:
    Backend::log(" - Authentication info unavailable");
    break;
  case PAM_AUTH_ERR:
    Backend::log(" - Authentication error");
    break;
  case PAM_BUF_ERR:
    Backend::log(" - Buffer error");
    break;
  case PAM_CONV_ERR:
    Backend::log(" - Conversion error");
    break;
  case PAM_CRED_INSUFFICIENT:
    Backend::log(" - Credentials insufficient");
    break;
  case PAM_MAXTRIES:
    Backend::log(" - Maximum number of tries exceeded");
    break;
  case PAM_PERM_DENIED:
    Backend::log(" - Permission denied");
    break;
  case PAM_SERVICE_ERR:
    Backend::log(" - Service error");
    break;
  case PAM_SYMBOL_ERR:
    Backend::log(" - Symbol error");
    break;
  case PAM_SYSTEM_ERR:
    Backend::log(" - System error");
    break;
  case PAM_USER_UNKNOWN:
    Backend::log(" - Unknown user");
    break;
  default:
    Backend::log(" - Unrecognized authentication error");
  }
	
}

void XProcess::pam_shutdown(){
  if(pam_session_open){
    pam_stopSession();
    pam_session_open = FALSE;
  }
  if(pam_started){
    pam_end(pamh,0);
    pam_started = FALSE;
  }
}

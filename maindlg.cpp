#define _KU_MAIN
#include "maindlg.h"
#include "maindlg.moc"

#include "globals.h"
#include <qtooltip.h>
#include <kmsgbox.h>
#include <ktoolbar.h>
#include <kiconloader.h>
#include <unistd.h>

#ifdef _KU_QUOTA
#include "quota.h"
#include "mnt.h"
#endif

#include "kuser.h"
#include "misc.h"
#include "usernamedlg.h"
#include "propdlg.h"
#include "pwddlg.h"
#include "editGroup.h"

#include <knewpanner.h>

mainDlg::mainDlg(QWidget *parent) :
QWidget(parent)
{
  changed = FALSE;
  prev = 0;

  usort = -1;
  gsort = -1;
}

void mainDlg::init() {
#ifdef _KU_QUOTA
  m = new Mounts();
  q = new Quotas();
#endif

  u = new KUsers();
  g = new KGroups();

  kp = new KNewPanner(this, "panner", KNewPanner::Horizontal);
  kp->setGeometry(10, 80, 380, 416);
  
  lbusers = new KUserView(kp, "lbusers");
  //  lbusers->setGeometry(10,80,380,208);

  lbgroups = new KGroupView(kp, "lbgroups");
  //  lbgroups->setGeometry(10,400,380,208);

  kp->activate(lbusers, lbgroups);
  
  QObject::connect(lbusers, SIGNAL(headerClicked(int)), this, SLOT(setUsersSort(int)));
  QObject::connect(lbusers, SIGNAL(selected(int)), this, SLOT(userSelected(int)));

  QObject::connect(lbgroups, SIGNAL(headerClicked(int)), this, SLOT(setGroupsSort(int)));
  QObject::connect(lbgroups, SIGNAL(selected(int)), this, SLOT(groupSelected(int)));

  reloadUsers(0);
  reloadGroups(0);
}

mainDlg::~mainDlg() {
  delete lbusers;
  delete lbgroups;
  delete kp;
  
  delete u;
  delete g;

#ifdef _KU_QUOTA
  delete q;
  delete m;
#endif
}

void mainDlg::setUsersSort(int col) {
  if (usort == col)
    usort = -1;
  else
    usort = col;

  lbusers->sortBy(usort);

  reloadUsers(lbusers->currentItem());
}

void mainDlg::setGroupsSort(int col) {
  if (gsort == col)
    gsort = -1;
  else
    gsort = col;

  lbgroups->sortBy(gsort);

  reloadGroups(lbgroups->currentItem());
}

void mainDlg::reloadUsers(int id) {
  KUser *ku;

  lbusers->setAutoUpdate(FALSE);
  lbusers->clear();

  for (uint i = 0; i<u->getUsersNumber(); i++) {
    ku = u->getUser(i);
    lbusers->insertItem(ku);
  }

  lbusers->setAutoUpdate(TRUE);
  lbusers->repaint();
  lbusers->setCurrentItem(id);
}

void mainDlg::reloadGroups(int gid) {
  KGroup *kg;

  lbgroups->setAutoUpdate(FALSE);
  lbgroups->clear();

  for (uint i = 0; i<g->getGroupsNumber(); i++) {
    kg = g->getGroup(i);
    lbgroups->insertItem(kg);
  }

  lbgroups->setAutoUpdate(TRUE);
  lbgroups->repaint();
  lbgroups->setCurrentItem(gid);
}

void mainDlg::edit() {
  userSelected(lbusers->currentItem());
}

void mainDlg::del() {
  uint i = 0;
  bool islast = FALSE;

  if (KMsgBox::yesNo(0, _("WARNING"),
                     _("Do you really want to delete user ?"),
                     KMsgBox::STOP,
                     _("Cancel"), _("Delete")) == 2) {

    i = lbusers->currentItem();
    if (i == u->getUsersNumber()-1)
      islast = TRUE;

    unsigned int uid = lbusers->getCurrentUser()->getp_uid();

    u->delUser(lbusers->getCurrentUser());

#ifdef _KU_QUOTA
    if (u->user_lookup(uid) == NULL)
      q->delQuota(uid);
#endif

    prev = -1;

    if (!islast)
      reloadUsers(i);
    else
      reloadUsers(i-1);
    changed = TRUE;
  }
}

void mainDlg::add() {
  usernamedlg *ud;
  propdlg *editUser;

  KUser *tk;
#ifdef _KU_QUOTA
  Quota *tq;
#endif // _KU_QUOTA

  tk = new KUser();

  tk->setp_uid(u->first_free());

#ifdef _KU_QUOTA
  tq = new Quota(tk->getp_uid(), FALSE);
#endif // _KU_QUOTA

  ud = new usernamedlg(tk, this);
  if (ud->exec() == 0)
    return;

  delete ud;

  config->setGroup("template");
  tk->setp_shell(readentry("shell"));
  tk->setp_dir(readentry("homeprefix")+"/"+tk->getp_name());
  tk->setp_gid(readnumentry("gid"));
  tk->setp_fname(readentry("p_fname"));
  tk->setp_office1(readentry("p_office1"));
  tk->setp_office2(readentry("p_office2"));
  tk->setp_address(readentry("p_address"));

#ifdef _KU_SHADOW
  tk->sets_lstchg(today());
  tk->sets_min(readnumentry("s_min"));
  tk->sets_max(readnumentry("s_max"));
  tk->sets_warn(readnumentry("s_warn"));
  tk->sets_inact(readnumentry("s_inact"));
  tk->sets_expire(readnumentry("s_expire"));
  tk->sets_flag(readnumentry("s_flag"));
#endif

#ifdef _KU_QUOTA
  if (is_quota)
    editUser = new propdlg(tk, tq, this, "userin");
  else
    editUser = new propdlg(tk, NULL, this, "userin");
#else
  editUser = new propdlg(tk, this, "userin");
#endif

  if (editUser->exec() != 0) {
    u->addUser(tk);
#ifdef _KU_QUOTA
    q->addQuota(tq);
#endif
    changed = TRUE;
  }
  else {
    delete tk;
#ifdef _KU_QUOTA
    delete tq;
#endif
  }

  reloadUsers(u->getUsersNumber()-1);

  delete editUser;
}

void mainDlg::quit() {
 if (changed == TRUE)
   if (KMsgBox::yesNo(0, _("Data was modified"),
                      _("Would you like to save changes ?"),
  		      KMsgBox::INFORMATION,
		      _("Save"), _("Discard changes")) == 1) {
      u->save();
      g->save();
#ifdef _KU_QUOTA
      if (is_quota)
        q->save();
#endif
    }

  delete this;
  qApp->quit();
}

void mainDlg::about() {
    QString tmp;
    tmp.sprintf(_("KUser version %s\nKDE project\nThis program was created by\nDenis Y. Pershin\ndyp@inetlab.com\nCopyright 1997(c)"), _KU_VERSION);
    KMsgBox::message(0, _("Message"), tmp, KMsgBox::INFORMATION);
}

void mainDlg::setpwd() {
  pwddlg *d;
  
  d = new pwddlg(lbusers->getCurrentUser(), this, "pwddlg");
  d->exec();
  delete d;
}

void mainDlg::help() {
	kapp->invokeHTMLHelp(0,0);
}

void mainDlg::properties() {
  propdlg *editUser;
  KUser *tk;

  tk = new KUser();
#ifdef _KU_QUOTA
  Quota *tq = new Quota(0, FALSE);
#endif

  config->setGroup("template");
  tk->setp_shell(readentry("shell"));
  tk->setp_dir(readentry("homeprefix"));
  tk->setp_gid(readnumentry("gid"));
  tk->setp_fname(readentry("p_fname"));
  tk->setp_office1(readentry("p_office1"));
  tk->setp_office2(readentry("p_office2"));
  tk->setp_address(readentry("p_address"));

#ifdef _KU_SHADOW
  tk->sets_lstchg(readnumentry("s_lstchg"));
  tk->sets_min(readnumentry("s_min"));
  tk->sets_max(readnumentry("s_max"));
  tk->sets_warn(readnumentry("s_warn"));
  tk->sets_inact(readnumentry("s_inact"));
  tk->sets_expire(readnumentry("s_expire"));
  tk->sets_flag(readnumentry("s_flag"));
#endif

#ifdef _KU_QUOTA
  if (is_quota) {
    /*
    tk->quota.at(0)->fsoft = readnumentry("quota.fsoft");
    tk->quota.at(0)->fhard = readnumentry("quota.fhard");
    tk->quota.at(0)->isoft = readnumentry("quota.isoft");
    tk->quota.at(0)->ihard = readnumentry("quota.ihard");
    */
  }
#endif

#ifdef _KU_QUOTA
  editUser = new propdlg(tk, tq, this, "userin");
#else
  editUser = new propdlg(tk, this, "userin");
#endif
  if (editUser->exec() != 0) {
    config->writeEntry("shell", tk->getp_shell());
    config->writeEntry("homeprefix", tk->getp_dir());
    config->writeEntry("gid", tk->getp_gid());
    config->writeEntry("p_fname", tk->getp_fname());
    config->writeEntry("p_office1", tk->getp_office1());
    config->writeEntry("p_office2", tk->getp_office2());
    config->writeEntry("p_address", tk->getp_address());

#ifdef _KU_SHADOW
    config->writeEntry("s_lstchg", tk->gets_lstchg());
    config->writeEntry("s_min", tk->gets_min());
    config->writeEntry("s_max", tk->gets_max());
    config->writeEntry("s_warn", tk->gets_warn());
    config->writeEntry("s_inact", tk->gets_inact());
    config->writeEntry("s_expire", tk->gets_expire());
    config->writeEntry("s_flag", tk->gets_flag());
#endif

#ifdef _KU_QUOTA
    if (is_quota) {
      /*
      config->writeEntry("quota.fsoft", tk->quota.at(0)->fsoft);
      config->writeEntry("quota.fhard", tk->quota.at(0)->fhard);
      config->writeEntry("quota.isoft", tk->quota.at(0)->isoft);
      config->writeEntry("quota.ihard", tk->quota.at(0)->ihard);
      */
    }
#endif
  }

  delete tk;
#ifdef _KU_QUOTA
  delete tq;
#endif
  delete editUser;
}

void mainDlg::groupSelected(int i) {
  editGroup *egdlg;
  KGroup *tmpKG;

  tmpKG = g->getGroup(i);

  if (tmpKG == NULL) {
    printf(_("Null pointer tmpKG in mainDlg::groupSelected(%d)\n"), i);
    return;
  }

  egdlg = new editGroup(tmpKG);

  if (egdlg->exec() != 0)
    changed = TRUE;

  delete egdlg;
}

void mainDlg::userSelected(int i) {
  propdlg *editUser;
  KUser *tmpKU;

  tmpKU =  lbusers->getCurrentUser();
  if (tmpKU == NULL) {
    printf(_("Null pointer tmpKU in mainDlg::userSelected(%d)\n"), i);
    return;
  }

#ifdef _KU_QUOTA
  Quota *tmpQ;

  tmpQ = q->getQuota(tmpKU->getp_uid());
  if (tmpQ == NULL) {
    printf(_("Null pointer tmpQ in mainDlg::selected(%d)\n"), i);
    return;
  }

  editUser = new propdlg(tmpKU, tmpQ, this, "userin");
#else
  editUser = new propdlg(tmpKU, this, "userin");
#endif

  if (editUser->exec() != 0) {
    reloadUsers(lbusers->currentItem());
    changed = TRUE;
  }

  delete editUser;
}

KUsers *mainDlg::getUsers() {
  return (u);
}

KGroups *mainDlg::getGroups() {
  return (g);
}

#ifdef _KU_QUOTA
Mounts *mainDlg::getMounts() {
  return (m);
}

Quotas *mainDlg::getQuotas() {
  return (q);
}
#endif

void mainDlg::resizeEvent (QResizeEvent *rse) {
  QSize sz;

  sz = rse->size();

  kp->setGeometry(10, 10, sz.width()-20, sz.height()-20);
//  lbusers->setGeometry(10, 10, sz.width()-20, sz.height()/2-70);
//  lbgroups->setGeometry(10, sz.height()/2+60, sz.width()-20, sz.height()/2-70);
}


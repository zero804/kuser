#include "globals.h"

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>

#ifdef _KU_SHADOW
#include <shadow.h>
#endif

#include <qstring.h>

#include "kglobal_.h"
#include "kgroup.h"
#include "misc.h"

#ifdef _KU_QUOTA
#include "mnt.h"
#include "quota.h"
#endif

// This is to simplify compilation for Red Hat Linux systems, where
// gid's for regular users' private groups start at 500 <duncan@kde.org>
#ifdef KU_FIRST_USER
#define _KU_FIRST_GID KU_FIRST_USER
#else 
#define _KU_FIRST_GID 1001 
#endif

KGroup::KGroup() : pwd("*") {
  gid = 0;
}
  
KGroup::KGroup(KGroup *copy) {
  name    = copy->name;
  pwd     = copy->pwd;
  gid     = copy->gid;
}

KGroup::~KGroup() {
}

const QString &KGroup::getName() const {
  return name;
}

const QString &KGroup::getPwd() const {
  return pwd;
}

gid_t KGroup::getGID() const {
  return gid;
}

void KGroup::setName(const QString &data) {
  name = data;
}

void KGroup::setPwd(const QString &data) {
  pwd = data;
}

void KGroup::setGID(gid_t data) {
  gid = data;
}

bool KGroup::lookup_user(const QString &name) {
  return (u.find(name) != u.end());
}

void KGroup::addUser(const QString &name) {
  if (!lookup_user(name))
     u.append(name);
}

void KGroup::removeUser(const QString &name) {
  u.remove(name);
}

uint KGroup::count() const {
  return u.count();
}

QString KGroup::user(uint i) {
  return u[i];
}

void KGroup::clear() {
  u.clear();
}

KGroups::KGroups() {
  g_saved = 0;

  mode = 0644;
  uid = 0;
  gid = 0;

  g.setAutoDelete(TRUE);

  if (!load())
    err->display();
}

bool KGroups::load() {
  struct group *p;
  KGroup *tmpKG = 0;
  struct stat st;

  stat(GROUP_FILE, &st);
  mode = st.st_mode;
  uid = st.st_uid;
  gid = st.st_gid;

#ifdef HAVE_FGETGRENT
  FILE *fgrp = fopen(GROUP_FILE, "r");
  QString tmp;
  if (fgrp == 0) {
    err->addMsg(i18n("Error opening %1 for reading").arg(GROUP_FILE));
    return FALSE;
  }

  while ((p = fgetgrent(fgrp)) != NULL) {
#else
  while ((p = getgrent()) != NULL) {
#endif
    tmpKG = new KGroup();
    tmpKG->setGID(p->gr_gid);
    tmpKG->setName(p->gr_name);
    tmpKG->setPwd(p->gr_passwd);

    char *u_name;
    int i = 0;
    while ((u_name = p->gr_mem[i])!=0) {
      tmpKG->addUser(u_name);
      i++;
    }

    g.append(tmpKG);
  }

#ifdef HAVE_FGETGRENT
  fclose(fgrp);
#endif

  return TRUE;
}

bool KGroups::save() {
  FILE *grp;
  QString tmpS;
  QString tmp;

  if (!g_saved) {
    backup(GROUP_FILE);
    g_saved = TRUE;
  }

  umask(0077);

  if ((grp = fopen(GROUP_FILE, "w")) == NULL) {
    err->addMsg(i18n("Error opening %1 for writing").arg(GROUP_FILE));
    return FALSE;
  }

  for (unsigned int i=0; i<g.count(); i++) {
    KGroup *gr = g.at(i);
    tmpS = QString::fromLatin1("%1:%2:%3:")
      .arg(gr->getName())
      .arg(gr->getPwd())
      .arg(gr->getGID());
    for (uint j=0; j<gr->count(); j++) {
       if (j != 0)
	 tmpS += ',';
       tmpS += gr->user(j);
    }
    tmpS += '\n';
    fputs(tmpS.local8Bit(), grp);
  }
  fclose(grp);

  chmod(GROUP_FILE, mode);
  chown(GROUP_FILE, uid, gid);

#ifdef GRMKDB
  if (system(GRMKDB) != 0) {
    err->addMsg(i18n("Unable to build group database"));
    return FALSE;
  }
#endif
  return TRUE;
}

KGroup *KGroups::lookup(const QString &name) {
  for (uint i = 0; i<g.count(); i++)
    if (g.at(i)->getName() == name)
      return g.at(i);
  return NULL;
}

KGroup *KGroups::lookup(gid_t gid) {
  for (uint i = 0; i<g.count(); i++)
    if (g.at(i)->getGID() == gid)
      return g.at(i);
  return NULL;
}

gid_t KGroups::first_free() {
  gid_t t = _KU_FIRST_GID ;

  for (t = _KU_FIRST_GID ; t<65534; t++)
    if (lookup(t) == NULL)
      return t;

  err->addMsg(i18n("You have more than 65534 groups!?!? You have run out of gid space!"));
  return (-1);
}

KGroups::~KGroups() {
  g.clear();
}

KGroup *KGroups::operator[](uint num) {
  return g.at(num);
}

KGroup *KGroups::first() {
  return g.first();
}

KGroup *KGroups::next() {
  return g.next();
}

void KGroups::add(KGroup *ku) {
  g.append(ku);
}

void KGroups::del(KGroup *au) {
  g.remove(au);
}

uint KGroups::count() const {
  return g.count();
}


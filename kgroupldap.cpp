/*
 *  Copyright (c) 2004 Szombathelyi György <gyurco@freemail.hu>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License version 2 as published by the Free Software Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 **/

#include <qstring.h>
#include <qdir.h>

#include <kdebug.h>
#include <kmessagebox.h>

#include "kglobal_.h"
#include "kgroupldap.h"
#include "misc.h"
#include "editDefaults.h"

#include "kgroupldap.moc"

KGroupLDAP::KGroupLDAP( KUserPrefsBase *cfg ) : KGroups( cfg )
{
  mGroups.setAutoDelete(TRUE);

  if ( mCfg->ldapssl() )
    mUrl.setProtocol("ldaps");
  else
    mUrl.setProtocol("ldap");
      
  mUrl.setHost( mCfg->ldaphost() );
  mUrl.setPort( mCfg->ldapport() );
  mUrl.setDn( mCfg->ldapgroupbase() + "," + mCfg->ldapdn() );
  mUrl.setUser( mCfg->ldapuser() );
  mUrl.setPass( mCfg->ldappassword() );
  mUrl.setFilter( mCfg->ldapfilter() );

  if ( mCfg->ldaptls() ) mUrl.setExtension("x-tls","");
  if ( mCfg->ldapsasl() ) {
    mUrl.setExtension( "x-sasl", "" );
    mUrl.setExtension( "x-mech", mCfg->ldapsaslmech() );
  }
    
  mUrl.setScope(KABC::LDAPUrl::One);
  mUrl.setExtension("x-dir","base");
  
  caps = Cap_Passwd;
  if ( mCfg->ldapsam() ) {
    caps |= Cap_Samba;
    domsid = mCfg->samdomsid();
  }
  
  reload();
}

KGroupLDAP::~KGroupLDAP() 
{
  mGroups.clear();
}

QString KGroupLDAP::getRDN( KGroup *group )
{
  switch ( mCfg->ldapgrouprdn() ) {
    case KUserPrefsBase::EnumLdapgrouprdn::cn:
      return "cn=" + group->getName();
    case KUserPrefsBase::EnumLdapgrouprdn::gidNumber:
      return "gidNumber=" + group->getGID();
    default:
      return "";
  }
}

void KGroupLDAP::result( KIO::Job *job )
{
  delete mProg;
  mCancel = false;
  if ( job->error() ) {
    QString errstr = job->errorString();
    if ( !errstr.isEmpty() ) {
      if ( ldif.isEmpty() )
        KMessageBox::error( 0, errstr );
      else
        KMessageBox::detailedError( 0, errstr, QString::fromUtf8( ldif, ldif.size()-1 ) );
    }
    mOk = false;
  } else {
    mOk = true;
  }
}

void KGroupLDAP::data( KIO::Job*, const QByteArray& data )
{
  if ( data.size() ) {
    mParser.setLDIF( data );
  } else {
    QByteArray dummy( 3 );
    dummy[ 0 ] = '\n';
    dummy[ 1 ] = '\n';
    dummy[ 2 ] = '\n';
    mParser.setLDIF( dummy );
  }

  KABC::LDIF::ParseVal ret;
  QString name;
  QByteArray value;
  do {
    ret = mParser.nextItem();
    switch ( ret ) {
      case KABC::LDIF::Item:
        name = mParser.attr().lower();
        value = mParser.val();
        if ( name == QString::fromLatin1("gidnumber") ) 
          mGroup->setGID( QString::fromUtf8( value, value.size() ).toLong() );
        else if ( name == QString::fromLatin1("cn") )
          mGroup->setName(QString::fromUtf8( value, value.size() ));
        else if ( name == QString::fromLatin1("userpassword") )
          mGroup->setPwd(QString::fromUtf8( value, value.size() ));
        else if ( name == QString::fromLatin1("memberuid") )
          mGroup->addUser(QString::fromUtf8( value, value.size() ));
        else if ( name == QString::fromLatin1("sambasid") )
          mGroup->setSID(QString::fromUtf8( value, value.size() ));
        else if ( name == QString::fromLatin1("sambagrouptype") )
          mGroup->setType(QString::fromUtf8( value, value.size() ).toInt());
        else if ( name == QString::fromLatin1("displayname") )
          mGroup->setDisplayName(QString::fromUtf8( value, value.size() ));
        else if ( name == QString::fromLatin1("description") )
          mGroup->setDesc(QString::fromUtf8( value, value.size() ));
        break;
      case KABC::LDIF::EndEntry: {
        KGroup newGroup;
        mGroups.append( new KGroup( mGroup ) );
        mGroup->copy( &newGroup );
        if ( ( mGroups.count() & 7 ) == 7 ) {
          mProg->progressBar()->advance( mAdv );
          if ( mProg->progressBar()->progress() == 0 ) mAdv = 1;
          if ( mProg->progressBar()->progress() == mProg->progressBar()->totalSteps()-1 ) mAdv = -1;
        }
        break;
      }
      default:
        break;
    }
  } while ( ret != KABC::LDIF::MoreData );
}

bool KGroupLDAP::reload() 
{
  kdDebug() << "KGroupLDAP::reload()" << endl;
  mGroup = new KGroup();
  mParser.startParsing();
  
  mProg = new KProgressDialog( 0, "", "", i18n("Loading groups from LDAP"), true );
  mProg->setAutoClose( false );
  mProg->progressBar()->setFormat( "" );
  mProg->progressBar()->setTotalSteps( 100 );
  mAdv = 1;
  mCancel = true;
  ldif = "";
 
  KIO::Job *job = KIO::get( mUrl, true, false );
  connect( job, SIGNAL( data( KIO::Job*, const QByteArray& ) ),
    this, SLOT( data( KIO::Job*, const QByteArray& ) ) );
  connect( job, SIGNAL( result( KIO::Job* ) ),
    this, SLOT( result( KIO::Job* ) ) );
  mProg->exec();
  if ( mCancel ) job->kill();
  
  delete mGroup;
  return( mOk );
}

bool KGroupLDAP::dbcommit() 
{
  mAddSucc.clear();
  mDelSucc.clear();
  mModSucc.clear();
  mAdd.first();
  mDel.first();
  mAddGroup = 0; mDelGroup = 0; mGroup = 0;
  ldif = "";
  
  mProg = new KProgressDialog( 0, "", i18n("LDAP operation"), "", true );
  KIO::Job *job = KIO::put( mUrl, -1, false, false, false );
  connect( job, SIGNAL( dataReq( KIO::Job*, QByteArray& ) ),
    this, SLOT( putData( KIO::Job*, QByteArray& ) ) );
  connect( job, SIGNAL( result( KIO::Job* ) ),
    this, SLOT( result( KIO::Job* ) ) );
  mProg->exec();
  return( mOk );
}

void KGroupLDAP::putData( KIO::Job*, QByteArray& data )
{
  ModIt mit = mMod.begin();
  
  if ( mAddGroup ) {
    mAddSucc.append( mAddGroup );
    mAdd.remove();
    mAddGroup = 0;
  }
  if ( mDelGroup ) {
    mDelSucc.append( mDelGroup );
    mDel.remove();
    mDelGroup = 0;
  }
  if ( mGroup ) {
    mModSucc.insert( mGroup, mit.data() );
    mMod.remove( mit );
    mit = mMod.begin();
  }
    
  if ( (mAddGroup = mAdd.current()) ) {
    addData( mAddGroup );
    data = ldif;
  } else if ( mit != mMod.end() ) {
    mGroup = mit.key();
    modData( &(mit.data()) );
    data = ldif;
  } else if ( (mDelGroup = mDel.current()) ) {
    delData( mDelGroup );
    data = ldif;
  } else
    data.resize(0);
}

void KGroupLDAP::addData( KGroup *group )
{
  ldif = "dn: " + getRDN( group ).utf8() + "," + 
    mUrl.dn().utf8() + "\n" + "objectclass: posixGroup\n";
      
  ldif +=
    KABC::LDIF::assembleLine( "cn", group->getName() ) + "\n" + 
    KABC::LDIF::assembleLine( "gidnumber", QString::number(group->getGID()) ) + "\n" +
    KABC::LDIF::assembleLine( "userpassword", group->getPwd() ) + "\n";
  for ( uint i=0; i < group->count(); i++ ) {
    ldif += KABC::LDIF::assembleLine( "memberuid", group->user(i) ) + "\n";
  }
  if ( getCaps() & Cap_Samba ) {
    ldif += "objectclass: sambagroupmapping\n" +
      KABC::LDIF::assembleLine( "sambasid", group->getSID().getSID() ) + "\n" + 
      KABC::LDIF::assembleLine( "displayname", group->getDisplayName() ) + "\n" + 
      KABC::LDIF::assembleLine( "description", group->getDesc() ) + "\n" + 
      KABC::LDIF::assembleLine( "sambagrouptype", QString::number( group->getType() ) ) + "\n"; 
  }
  ldif += "\n\n";
//  kdDebug() << "ldif: " << ldif << endl;
}

void KGroupLDAP::delData( KGroup *group )
{
  ldif = "dn: " + getRDN( group ).utf8() + "," + 
    mUrl.dn().utf8() + "\n" + "changetype: delete\n\n";
}

void KGroupLDAP::modData( KGroup *group )
{
  ldif = "dn: " + getRDN( group ).utf8() + "," + mUrl.dn().utf8() + "\n" + 
    "changetype: modify\n" + 
    "replace: objectclass\n" +
    "objectclass: posixgroup\n";
  if ( getCaps() & Cap_Samba ) {
    ldif += "objectclass: sambagroupmapping\n";
  }  
  ldif +=
    "-\nreplace: cn\n" + 
    KABC::LDIF::assembleLine( "cn", group->getName() ) + 
    "\n-\nreplace: gidnumber\n" +
    KABC::LDIF::assembleLine( "gidnumber", QString::number(group->getGID()) ) +
    "\n-\nreplace: userpassword\n" +
    KABC::LDIF::assembleLine( "userpassword", group->getPwd() ) + 
    "\n-\nreplace: memberuid\n";
  for ( uint i=0; i < group->count(); i++ ) {
    ldif += KABC::LDIF::assembleLine(QString::fromLatin1("memberuid"),
      group->user(i)) + "\n";
  }
  if ( getCaps() & Cap_Samba ) {
    ldif +=
      "-\nreplace: sambasid\n" + 
      KABC::LDIF::assembleLine( "sambasid", group->getSID().getSID() ) + 
      "\n-\nreplace: displayname\n" + 
      KABC::LDIF::assembleLine( "displayname", group->getDisplayName() ) + 
      "\n-\nreplace: description\n" + 
      KABC::LDIF::assembleLine( "description", group->getDesc() ) +
      "\n-\nreplace: sambagrouptype\n" + 
      KABC::LDIF::assembleLine( "sambagrouptype", QString::number( group->getType() ) ) + "\n"; 
  }
  
  ldif += "-\n\n";
//  kdDebug() << "ldif: " << ldif << endl;
}

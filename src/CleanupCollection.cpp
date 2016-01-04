/*
 *   File name: CleanupCollection.cpp
 *   Summary:	QDirStat classes to reclaim disk space
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include <QMenu>
#include <QSettings>

#include "CleanupCollection.h"
#include "Cleanup.h"
#include "StdCleanup.h"
#include "SettingsHelpers.h"
#include "SelectionModel.h"
#include "Logger.h"
#include "Exception.h"


using namespace QDirStat;


CleanupCollection::CleanupCollection( SelectionModel * selectionModel, QObject * parent ):
    QObject( parent ),
    _selectionModel( selectionModel )
{
    readSettings();

    if ( _cleanupList.isEmpty() )
	addStdCleanups();

    updateActions();

    connect( selectionModel, SIGNAL( selectionChanged() ),
	     this,	     SLOT  ( updateActions()	) );
}


CleanupCollection::~CleanupCollection()
{
    writeSettings();
    clear();
}


void CleanupCollection::add( Cleanup * cleanup )
{
    CHECK_PTR( cleanup );

    int index = indexOf( cleanup->id() );

    if ( index != -1 ) // Replacing an existing ID?
    {
	logDebug() << "Replacing cleanup " << cleanup->id() << endl;
	Cleanup * oldCleanup = _cleanupList.at( index );
	_cleanupList.removeAt( index );
	delete oldCleanup;
    }

    _cleanupList << cleanup;

    connect( cleanup, SIGNAL( triggered() ),
	     this,    SLOT  ( execute()	  ) );
}


void CleanupCollection::remove( Cleanup * cleanup )
{
    int index = indexOf( cleanup );

    if ( index == -1 )
    {
	logError() << "No such cleanup: " << cleanup << endl;
	return;
    }

    _cleanupList.removeAt( index );
}


void CleanupCollection::addStdCleanups()
{
    foreach ( Cleanup * cleanup, StdCleanup::stdCleanups( this ) )
    {
	add( cleanup );
    }
}



Cleanup * CleanupCollection::findById( const QString & id ) const
{
    int index = indexOf( id );

    if ( index == -1 )
	return 0;
    else
	return _cleanupList.at( index );
}


int CleanupCollection::indexOf( const QString & id ) const
{
    for ( int i=0; i < _cleanupList.size(); ++i )
    {
	if ( id == _cleanupList.at(i)->id() )
	    return i;
    }

    logError() << "No Cleanup with ID " << id << " in this collection" << endl;
    return -1;
}


int CleanupCollection::indexOf( Cleanup * cleanup ) const
{
    int index = _cleanupList.indexOf( cleanup );

    if ( index == -1 )
	logError() << "Cleanup " << cleanup << " is not in this collection" << endl;

    return index;
}


Cleanup * CleanupCollection::at( int index ) const
{
    if ( index >= 0 && index < _cleanupList.size() )
	return _cleanupList.at( index );
    else
	return 0;
}


void CleanupCollection::clear()
{
    qDeleteAll( _cleanupList );
    _cleanupList.clear();
}


void CleanupCollection::updateActions()
{
    FileInfoSet sel = _selectionModel->selectedItems();

    bool dirSelected	  = sel.containsDir();
    bool fileSelected	  = sel.containsFile();
    bool dotEntrySelected = sel.containsDotEntry();

    foreach ( Cleanup * cleanup, _cleanupList )
    {
	if ( ! cleanup->active() || sel.isEmpty() )
	    cleanup->setEnabled( false );
	else
	{
	    bool enabled = true;

	    if ( dirSelected && ! cleanup->worksForDir() )
		enabled = false;

	    if ( dotEntrySelected && ! cleanup->worksForDotEntry() )
		enabled = false;

	    if ( fileSelected && ! cleanup->worksForFile() )
		enabled = false;

	    cleanup->setEnabled( enabled );
	}
    }
}


void CleanupCollection::execute()
{
    Cleanup * cleanup = qobject_cast<Cleanup *>( sender() );

    if ( ! cleanup )
    {
	logError() << "Wrong sender type: " << sender()->metaObject()->className() << endl;
	return;
    }

    FileInfoSet sel = _selectionModel->selectedItems();

    foreach ( FileInfo * item, sel )
    {
	if ( cleanup->worksFor( item ) )
	{
	    cleanup->execute( item );
	}
	else
	{
	    logWarning() << "Cleanup " << cleanup
			 << " does not work for " << item << endl;
	}
    }
}


void CleanupCollection::addToMenu( QMenu * menu )
{
    CHECK_PTR( menu );

    foreach ( Cleanup * cleanup, _cleanupList )
    {
	if ( cleanup->active() )
	    menu->addAction( cleanup );
    }
}


void CleanupCollection::moveUp( Cleanup * cleanup )
{
    int oldPos = indexOf( cleanup );

    if ( oldPos > 0 )
    {
	_cleanupList.removeAt( oldPos );
	_cleanupList.insert( oldPos - 1, cleanup );
    }
}


void CleanupCollection::moveDown( Cleanup * cleanup )
{
    int oldPos = indexOf( cleanup );

    if ( oldPos < _cleanupList.size() - 1 )
    {
	_cleanupList.removeAt( oldPos );
	_cleanupList.insert( oldPos + 1, cleanup );
    }
}


void CleanupCollection::moveToTop( Cleanup * cleanup )
{
    int oldPos = indexOf( cleanup );

    if ( oldPos > 0 )
    {
	_cleanupList.removeAt( oldPos );
	_cleanupList.insert( 0, cleanup );
    }
}


void CleanupCollection::moveToBottom( Cleanup * cleanup )
{
    int oldPos = indexOf( cleanup );

    if ( oldPos < _cleanupList.size() - 1 )
    {
	_cleanupList.removeAt( oldPos );
	_cleanupList.insert( _cleanupList.size(), cleanup );
    }
}


void CleanupCollection::readSettings()
{
    QSettings settings;
    settings.beginGroup( "Cleanups" );
    int size = settings.beginReadArray( "Cleanup" );

    if ( size > 0 )
    {
	clear();

	for ( int i=0; i < size; ++i )
	{
	    settings.setArrayIndex( i );

	    QString command  = settings.value( "Command" ).toString();
	    QString id	     = settings.value( "ID"	 ).toString();
	    QString title    = settings.value( "Title"	 ).toString();
	    QString iconName = settings.value( "Icon"	 ).toString();
	    QString hotkey   = settings.value( "Hotkey"	 ).toString();

	    bool active		    = settings.value( "Active"		  , true  ).toBool();
	    bool worksForDir	    = settings.value( "WorksForDir"	  , true  ).toBool();
	    bool worksForFile	    = settings.value( "WorksForFile"	  , true  ).toBool();
	    bool worksForDotEntry   = settings.value( "WorksForDotEntry"  , true  ).toBool();
	    bool recurse	    = settings.value( "Recurse"		  , false ).toBool();
	    bool askForConfirmation = settings.value( "AskForConfirmation", false ).toBool();

	    int refreshPolicy = readEnumEntry( settings, "RefreshPolicy",
					       Cleanup::NoRefresh,
					       Cleanup::refreshPolicyMapping() );

	    if ( ! id.isEmpty()	     &&
		 ! command.isEmpty() &&
		 ! title.isEmpty()     )
	    {
		Cleanup * cleanup = new Cleanup( id, command, title, this );
		CHECK_NEW( cleanup );
		add( cleanup );

		cleanup->setActive	    ( active   );
		cleanup->setWorksForDir	    ( worksForDir );
		cleanup->setWorksForFile    ( worksForFile );
		cleanup->setWorksForDotEntry( worksForDotEntry );
		cleanup->setRecurse	    ( recurse  );
		cleanup->setAskForConfirmation( askForConfirmation );
		cleanup->setRefreshPolicy( static_cast<Cleanup::RefreshPolicy>( refreshPolicy ) );

		if ( ! iconName.isEmpty() )
		    cleanup->setIcon( iconName );

		if ( ! hotkey.isEmpty() )
		    cleanup->setShortcut( hotkey );
	    }
	    else
	    {
		logError() << "Need at least Command, ID, Title for a cleanup" << endl;
	    }
	}
    }

    settings.endArray();
    settings.endGroup();
}


void CleanupCollection::writeSettings()
{
    QSettings settings;
    settings.beginGroup( "Cleanups" );

    if ( _cleanupList.isEmpty() )
	settings.remove( "Cleanup" );
    else
    {
	settings.beginWriteArray( "Cleanup", _cleanupList.size() );

	for ( int i=0; i < _cleanupList.size(); ++i )
	{
	    settings.setArrayIndex( i );
	    Cleanup * cleanup = _cleanupList.at( i );

	    settings.setValue( "Command"	   , cleanup->command()		   );
	    settings.setValue( "ID"		   , cleanup->id()		   );
	    settings.setValue( "Title"		   , cleanup->title()		   );
	    settings.setValue( "Active"		   , cleanup->active()		   );
	    settings.setValue( "WorksForDir"	   , cleanup->worksForDir()	   );
	    settings.setValue( "WorksForFile"	   , cleanup->worksForFile()	   );
	    settings.setValue( "WorksForDotEntry"  , cleanup->worksForDotEntry()   );
	    settings.setValue( "Recurse"	   , cleanup->recurse()		   );
	    settings.setValue( "AskForConfirmation", cleanup->askForConfirmation() );

	    writeEnumEntry( settings, "RefreshPolicy",
			    cleanup->refreshPolicy(),
			    Cleanup::refreshPolicyMapping() );

	    if ( ! cleanup->iconName().isEmpty() )
		 settings.setValue( "Icon", cleanup->iconName() );

	    if ( ! cleanup->shortcut().isEmpty() )
		settings.setValue( "Hotkey" , cleanup->shortcut().toString() );
	}

	settings.endArray();
    }
    settings.endGroup();
}

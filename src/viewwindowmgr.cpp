/**************************************************************************
 *  Kchmviewer - a CHM file viewer with broad language support            *
 *  Copyright (C) 2004-2010 George Yunaev, kchmviewer@ulduzsoft.com       *
 *                                                                        *
 *  This program is free software: you can redistribute it and/or modify  *
 *  it under the terms of the GNU General Public License as published by  *
 *  the Free Software Foundation, either version 3 of the License, or     *
 *  (at your option) any later version.                                   *
 *																	      *
 *  This program is distributed in the hope that it will be useful,       *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *  GNU General Public License for more details.                          *
 *                                                                        *
 *  You should have received a copy of the GNU General Public License     *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 **************************************************************************/

#include "config.h"
#include "mainwindow.h"
#include "viewwindow.h"
#include "viewwindowmgr.h"

#include "viewwindow_qtextbrowser.h"

#if defined (USE_KDE)
	#include "kde/viewwindow_khtmlpart.h"
#endif

#if defined (QT_WEBKIT_LIB)
	#include "viewwindow_qtwebkit.h"
#endif



// A small overriden class to handle a middle click
ViewWindowTabs::ViewWindowTabs( QWidget * parent )
			: QTabWidget( parent )
{
}

ViewWindowTabs::~ViewWindowTabs()
{
}

void ViewWindowTabs::mouseReleaseEvent ( QMouseEvent * event )
{
	if ( event->button() == Qt::MidButton)
	{
		int tab = tabBar()->tabAt( event->pos() );

		if ( tab != -1 )
			emit mouseMiddleClickTab( tab );
	}
}



ViewWindowMgr::ViewWindowMgr( QWidget *parent )
	: QWidget( parent ), Ui::TabbedBrowser()
{
	// UIC
	setupUi( this );
	
	// Create the tab widget
	m_tabWidget = new ViewWindowTabs( this );
	verticalLayout->insertWidget( 0, m_tabWidget, 10 );

	// on current tab changed
	connect( m_tabWidget, SIGNAL( currentChanged(int) ), this, SLOT( onTabChanged(int) ) );
	connect( m_tabWidget, SIGNAL( mouseMiddleClickTab( int ) ), this, SLOT( onCloseWindow(int) ) );

	// Create a close button
	m_closeButton = new QToolButton( this );
	m_closeButton->setCursor( Qt::ArrowCursor );
	m_closeButton->setAutoRaise( true );
	m_closeButton->setIcon( QIcon( ":/images/closetab.png" ) );
	m_closeButton->setToolTip( i18n("Close current page") );
	m_closeButton->setEnabled( false );
	connect( m_closeButton, SIGNAL( clicked() ), this, SLOT( onCloseCurrentWindow() ) );
	
	// Put it there
	m_tabWidget->setCornerWidget( m_closeButton, Qt::TopRightCorner );
	
	// Create a "new tab" button
	QToolButton * newButton = new QToolButton( this );
	newButton->setCursor( Qt::ArrowCursor );
	newButton->setAutoRaise( true );
	newButton->setIcon( QIcon( ":/images/addtab.png" ) );
	newButton->setToolTip( i18n("Add page") );
	connect( newButton, SIGNAL( clicked() ), this, SLOT( openNewTab() ) );
	
	// Put it there
	m_tabWidget->setCornerWidget( newButton, Qt::TopLeftCorner );
	
	// Hide the search frame
	frameFind->setVisible( false );
	labelWrapped->setVisible( false );
	
	// Search Line edit
	connect( editFind,
	         SIGNAL( textEdited ( const QString & ) ),
	         this, 
	         SLOT( editTextEdited( const QString & ) ) );
	
	// Search toolbar buttons
	connect( toolClose, SIGNAL(clicked()), frameFind, SLOT( hide()) );
	connect( toolPrevious, SIGNAL(clicked()), this, SLOT( onFindPrevious()) );
	connect( toolNext, SIGNAL(clicked()), this, SLOT( onFindNext()) );
}

ViewWindowMgr::~ViewWindowMgr( )
{
}
	
void ViewWindowMgr::createMenu( MainWindow *, QMenu * menuWindow, QAction * actionCloseWindow )
{
	m_menuWindow = menuWindow;
	m_actionCloseWindow = actionCloseWindow; 
}

void ViewWindowMgr::invalidate()
{
	closeAllWindows();
	addNewTab( true );
}


ViewWindow * ViewWindowMgr::current()
{
	TabData * tab = findTab( m_tabWidget->currentWidget() );
	
	if ( !tab )
		abort();
	
	return tab->window;
}

ViewWindow * ViewWindowMgr::addNewTab( bool set_active )
{
	ViewWindow * viewvnd;
	
	switch ( pConfig->m_usedBrowser )
	{
		default:
			viewvnd = new ViewWindow_QTextBrowser( m_tabWidget );
			break;

#if defined (USE_KDE)			
		case Config::BROWSER_KHTMLPART:
			viewvnd = new ViewWindow_KHTMLPart( m_tabWidget );
			break;
#endif			
			
#if defined (QT_WEBKIT_LIB)
		case Config::BROWSER_QTWEBKIT:
			viewvnd = new ViewWindow_QtWebKit( m_tabWidget );
			break;
#endif			
	}
	
	editFind->installEventFilter( this );
	
	// Create the tab data structure
	TabData tabdata;
	tabdata.window = viewvnd;
	tabdata.action = new QAction( "window", this ); // temporary name; real name is set in setTabName
	tabdata.widget = viewvnd->getQWidget();
	
	connect( tabdata.action,
			 SIGNAL( triggered() ),
	         this,
	         SLOT( activateWindow() ) );
	
	m_Windows.push_back( tabdata );
	m_tabWidget->addTab( tabdata.widget, "" );
	Q_ASSERT( m_Windows.size() == m_tabWidget->count() );
		
	// Set active if it is the first tab
	if ( set_active || m_Windows.size() == 1 )
		m_tabWidget->setCurrentWidget( tabdata.widget );
	
	// Handle clicking on link in browser window
	connect( viewvnd->getQObject(), 
	         SIGNAL( linkClicked (const QString &, bool &) ), 
	         ::mainWindow, 
	         SLOT( activateLink(const QString &, bool &) ) );
	
	// Set up the accelerator if we have room
	if ( m_Windows.size() < 10 )
		tabdata.action->setShortcut( QKeySequence( i18n("Alt+%1").arg( m_Windows.size() ) ) );
	
	// Add it to the "Windows" menu
	m_menuWindow->addAction( tabdata.action );
	
	return viewvnd;
}

void ViewWindowMgr::closeAllWindows( )
{
	// No it++ - we removing the window by every closeWindow call
	while ( m_Windows.begin() != m_Windows.end() )
		closeWindow( m_Windows.first().widget );
}

void ViewWindowMgr::setTabName( ViewWindow * window )
{
	TabData * tab = findTab( window->getQWidget() );
	
	if ( tab )
	{
		QString title = window->getTitle();
		
		// Trim too long string
		if ( title.length() > 25 )
			title = title.left( 22 ) + "...";
	
		m_tabWidget->setTabText( m_tabWidget->indexOf( window->getQWidget() ), title );
		tab->action->setText( title );
		
		updateCloseButtons();
	}
}

void ViewWindowMgr::onCloseCurrentWindow( )
{
	// Do not allow to close the last window
	if ( m_Windows.size() == 1 )
		return;
			
	TabData * tab = findTab( m_tabWidget->currentWidget() );
	closeWindow( tab->widget );
}


void ViewWindowMgr::onCloseWindow( int num )
{
	// Do not allow to close the last window
	if ( m_Windows.size() == 1 )
		return;

	TabData * tab = findTab( m_tabWidget->widget( num ));

	if ( tab )
		closeWindow( tab->widget );
}

void ViewWindowMgr::closeWindow( QWidget * widget )
{
	WindowsIterator it;
	
	for ( it = m_Windows.begin(); it != m_Windows.end(); ++it )
		if ( it->widget == widget )
			break;
	
	if ( it == m_Windows.end() )
		qFatal( "ViewWindowMgr::closeWindow called with unknown widget!" );

	m_menuWindow->removeAction( it->action );
	
	m_tabWidget->removeTab( m_tabWidget->indexOf( it->widget ) );
	delete it->window;
	delete it->action;
	
	m_Windows.erase( it );
	updateCloseButtons();
	
	// Change the accelerators, as we might have removed the item in the middle
	int count = 1;
	for ( WindowsIterator it = m_Windows.begin(); it != m_Windows.end() && count < 10; ++it, count++ )
		(*it).action->setShortcut( QKeySequence( i18n("Alt+%1").arg( count ) ) );
}


void ViewWindowMgr::restoreSettings( const Settings::viewindow_saved_settings_t & settings )
{
	// Destroy automatically created tab
	closeWindow( m_Windows.first().widget );
	
	for ( int i = 0; i < settings.size(); i++ )
	{
		ViewWindow * window = addNewTab( false );
		window->openUrl( settings[i].url ); // will call setTabName()
		window->setScrollbarPosition( settings[i].scroll_y );
		window->setZoomFactor( settings[i].zoom );
	}
}


void ViewWindowMgr::saveSettings( Settings::viewindow_saved_settings_t & settings )
{
	settings.clear();
	
	for ( int i = 0; i < m_tabWidget->count(); i++ )
	{
		QWidget * p = m_tabWidget->widget( i );
		TabData * tab = findTab( p );
			
		if ( !tab )
			abort();
		
		settings.push_back( Settings::SavedViewWindow( tab->window->getOpenedPage(),
													   tab->window->getScrollbarPosition(),
													   tab->window->getZoomFactor()) );
	}
}

void ViewWindowMgr::updateCloseButtons( )
{
	bool enabled = m_Windows.size() > 1;
	
	m_actionCloseWindow->setEnabled( enabled );
	m_closeButton->setEnabled( enabled );
}

void ViewWindowMgr::onTabChanged( int newtabIndex )
{
	if ( newtabIndex == -1 )
		return;

	TabData * tab = findTab( m_tabWidget->widget( newtabIndex ) );
	
	if ( tab )
	{
		tab->window->updateNavigationToolbar();
		mainWindow->browserChanged( tab->window );
		tab->widget->setFocus();
	}
}


void ViewWindowMgr::openNewTab()
{
	::mainWindow->openPage( current()->getOpenedPage(), 
							MainWindow::OPF_NEW_TAB | MainWindow::OPF_CONTENT_TREE | MainWindow::OPF_ADD2HISTORY );
}

void ViewWindowMgr::activateWindow()
{
	QAction *action = qobject_cast< QAction * >(sender());
	
	for ( WindowsIterator it = m_Windows.begin(); it != m_Windows.end(); ++it )
	{
		if ( it->action != action )
			continue;
		
		QWidget *widget = it->widget;
		m_tabWidget->setCurrentWidget(widget);
		break;
	}
}

ViewWindowMgr::TabData * ViewWindowMgr::findTab(QWidget * widget)
{
	for ( WindowsIterator it = m_Windows.begin(); it != m_Windows.end(); ++it )
		if ( it->widget == widget )
			return (it.operator->());
		
	return 0;
}

void ViewWindowMgr::setCurrentPage(int index)
{
	m_tabWidget->setCurrentIndex( index );
}

int ViewWindowMgr::currentPageIndex() const
{
	return m_tabWidget->currentIndex();
}


void ViewWindowMgr::indicateFindResultStatus( SearchResultStatus status )
{
	QPalette p = editFind->palette();
	
	if ( status == SearchResultNotFound )
		p.setColor( QPalette::Active, QPalette::Base, QColor(255, 102, 102) );
	else
		p.setColor( QPalette::Active, QPalette::Base, Qt::white );
	
	editFind->setPalette( p );
	labelWrapped->setVisible( status == SearchResultFoundWrapped );
}


void ViewWindowMgr::onActivateFind()
{
	frameFind->show();
	labelWrapped->setVisible( false );
	editFind->setFocus( Qt::ShortcutFocusReason );
	editFind->selectAll();
}


void ViewWindowMgr::find()
{
	int flags = 0;
	
	if ( checkCase->isChecked() )
		flags |= ViewWindow::SEARCH_CASESENSITIVE;
	
	if ( checkWholeWords->isChecked() )
		flags |= ViewWindow::SEARCH_WHOLEWORDS;

	current()->find( editFind->text(), flags );
		
	if ( !frameFind->isVisible() )
		frameFind->show();
}


void ViewWindowMgr::editTextEdited(const QString &)
{
	find();
}

void ViewWindowMgr::onFindNext()
{
	current()->onFindNext();
}

void ViewWindowMgr::onFindPrevious()
{
	current()->onFindPrevious();
}

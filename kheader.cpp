#include "qpainter.h"
#include "qdrawutil.h"

#include "kheader.h"

//=======================================================================
// KHeaderItem internal class

class KHeaderItem {
public:
    KHeaderItem( QWidget *parent, int aflags,
				 int size = 0, QString *label = 0,
                 int alignment = Qt::AlignCenter );
	virtual ~KHeaderItem();
    void setSize( int size );
    int size();
	void setText( QString label, int alignment = Qt::AlignCenter );
	QString text();
	void setFlags( int aflags );
	int flags();
	virtual void paint(QPainter *paint, QColorGroup *g, int style, int pos, int width, int height, int sunken);

private:
    int m_size;
	int m_flags;
    QString m_label;
    int m_labelAlignment;
	QWidget *m_parent;
};

KHeaderItem::KHeaderItem( QWidget *parent, int aflags,
						  int size, QString *label, int alignment )
{
	m_size = size;
	if( label != 0 ) m_label = *label;
	m_labelAlignment = alignment;
	m_flags = aflags;
	m_parent = parent;
}

KHeaderItem::~KHeaderItem()
{
}

void KHeaderItem::setSize( int size )
{
	m_size = size;
}

int KHeaderItem::size()
{
	return m_size;
}

void KHeaderItem::setText( QString label, int alignment )
{
	m_label = label;
	m_labelAlignment = alignment;
}

QString KHeaderItem::text() {
	return m_label;
}

void KHeaderItem::setFlags( int aflags ) {
	m_flags = ( m_flags & KHeader::Vertical ) | ( aflags & ~KHeader::Vertical );
}

int KHeaderItem::flags() {
	return m_flags;
}

void KHeaderItem::paint(QPainter *paint, QColorGroup *g, int style, int pos, int width, int height, int state) {
	if( m_flags & KHeader::Vertical )
	{
		if( pos>height || pos+m_size<0 )
			return;
		if(style==Qt::MotifStyle)
			qDrawShadePanel(paint,0,pos,width,m_size,*g,state,1,0);
		else
			qDrawWinPanel(paint,0,pos,width,m_size,*g,state,0);
		if( !m_label.isEmpty() )
			paint->drawText(2,pos+2,width-4,m_size-4,m_labelAlignment,m_label);
	}
	else
	{
		if( pos>width || pos+m_size<0 )
			return;
		if(style==Qt::MotifStyle)
			qDrawShadePanel(paint,pos,0,m_size,height,*g,state,1,0);
		else
			qDrawWinPanel(paint,pos,0,m_size,height,*g,state,0);
		if( !m_label.isEmpty() )
			paint->drawText(pos+2,2,m_size-4,height-4,m_labelAlignment,m_label);
	}
}

//=======================================================================

KHeader::KHeader( QWidget *parent, const char *name, int numHeaders, int aflags )
	: QFrame( parent, name )
{
	labels.resize( 0 );
	m_offset = 0;
	m_selected = -1;
	m_temp_sel = -1;
	m_flags = aflags;
	divider = -1;
	m_resizing = FALSE;
	if( numHeaders != 0 )
        setNumHeaders( numHeaders );
	if( aflags & Resizable )
	{
		installEventFilter(this);
		setMouseTracking(TRUE);
		m_defCursor=cursor();
	}
}


KHeader::~KHeader()
{
	for( uint i=0 ; i<labels.size() ; i++ )
		delete labels[ i ];
}


void KHeader::setGeometry( int x, int y, int w, int h )
{
	if( m_flags & Vertical )
		QFrame::setGeometry( x, y, w, h );
	else
		QFrame::setGeometry( x, y, w, fontMetrics().height() + 4 );
}


void KHeader::resize( int w, int h )
{
	if( m_flags & Vertical )
		QFrame::resize( w, h );
	else
		QFrame::resize( w, fontMetrics().height() + 4 );
}


void KHeader::fontChange( const QFont &oldfont )
{
	if( !(m_flags & Vertical) )
		resize( width(), fontMetrics().height() + 4 );
}

bool KHeader::eventFilter( QObject *obj, QEvent *ev )
{
	if( ev->type()==QEvent::MouseButtonPress && divider!=-1 ) {
		m_resizing=TRUE;
		return TRUE;
	}

	if( ev->type()==QEvent::MouseMove ) {
		QMouseEvent *mev = (QMouseEvent*)ev;
		if(m_resizing) {
			if( m_flags & Vertical ) {
				adjustHeaderSize( divider, mev->pos().y()-divstart );
				divstart = mev->pos().y();
			}
			else {
				adjustHeaderSize( divider, mev->pos().x()-divstart );
				divstart = mev->pos().x();
			}
			return TRUE;
		}
		else if( mev->button()==NoButton ) {
			// search labels to see if we are in range of a divider
			int pos = m_offset;
			int cur = m_flags&Vertical ? mev->y()+3 : mev->x()+3;
			divider = -1;
			for( uint i=0 ; i<labels.size() ; i++ ) {
				pos += labels[i]->size();
				if( cur>=pos && cur<pos+6 ) {
					setCursor(m_flags&Vertical ? sizeVerCursor : sizeHorCursor);
					divider = i;
					divstart = pos;
				}
			}
			if(divider==-1) setCursor(m_defCursor);
			return TRUE;
		}
	}

	if( ev->type()==QEvent::MouseButtonRelease && m_resizing ) {
		emit sizeChanged( divider, labels[divider]->size() );
		m_resizing = FALSE;
		return TRUE;
	}
	return FALSE;
}

int KHeader::numHeaders()
{
	return labels.size();
}


void KHeader::setNumHeaders( int numHeaders )
{
	ASSERT( numHeaders >= 0 );
	if( numHeaders < (int)labels.size() )
	{
		for( uint i=numHeaders ; i<labels.size() ; i++ )
		{
			delete labels[i];
		}
		labels.resize( numHeaders );
	}
	else if( numHeaders > (int)labels.size() )
	{
		int oldCount = labels.size();
		labels.resize( numHeaders );
		
		for( int i=oldCount ; i < numHeaders ; i++ )
		{
			labels[i] = new KHeaderItem( this, m_flags );
		}
	}
}


void KHeader::setHeaderFlags( int header, int aflags )
{
	ASSERT( header >= 0 );
	ASSERT( header < (int)labels.size() );

	labels[header]->setFlags( aflags );
}


void KHeader::setHeaderSize( int header, int size )
{
	ASSERT( header >= 0 );
	ASSERT( header < (int)labels.size() );

	labels[header]->setSize( size );
	repaint();
	emit sizeChanged( header, size );
}


void KHeader::adjustHeaderSize( int start, int delta )
{
	int pos = labels[start]->size()+delta;
	if( pos < 0 ) pos = 0;
	labels[start]->setSize( pos );
// There must be a better way!
// calculate the rectangle?
	repaint();
	emit sizeChanging( start, pos );
}


void KHeader::setOrigin( int pos )
{
	m_offset = -pos;
	repaint();
}


void KHeader::setHeaderLabel( int header, const char *text, int align )
{
	labels[header]->setText( text, align );
}


void KHeader::paintEvent( QPaintEvent *pev )
{
	QColorGroup g = colorGroup();

	QPainter paint;
	paint.begin( this );

	int pos = m_offset;
	for( int i=0 ; i<(int)labels.size(); i++ )
	{
		labels[i]->paint( &paint, &g, style(), pos, width(), height(), i==m_selected);
		pos += labels[i]->size();
	}

	paint.end();
}

void KHeader::mousePressEvent( QMouseEvent *mev )
{
	int cur;
	if( m_flags & Vertical )
		cur = mev->y();
	else
		cur = mev->x();
	int pos = m_offset;
	for( uint i=0 ; i<labels.size() ; i++ )
	{
		pos += labels[i]->size();
		if( pos >= cur )
		{
			if( !(labels[i]->flags() & Buttons) )
				return;
			m_selected = i;
			break;
		}
	}
	repaint();
}

void KHeader::mouseReleaseEvent( QMouseEvent *mev )
{
	if( m_selected == -1 )
	{
		m_temp_sel = -1;
		return;
	}
	emit selected(m_selected);
	m_selected = -1;
	m_temp_sel = -1;
	repaint();
}

void KHeader::enterEvent( QEvent *ev )
{
	m_selected = m_temp_sel;
	if( m_selected != -1 )
		repaint();
}

void KHeader::leaveEvent( QEvent *ev )
{
	m_temp_sel = m_selected;
	m_selected = -1;
	if( m_temp_sel != -1 )
		repaint();
}

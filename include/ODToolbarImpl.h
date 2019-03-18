#ifndef __ODToolbarImpl__
#define __ODToolbarImpl__

/**
@file
Subclass of MyFrameODToolbar, which is generated by wxFormBuilder.
*/

#include "ODToolbarDef.h"
#include "ocpn_draw_pi.h"

enum {
    ID_DISPLAY_NEVER = 0,
    ID_DISPLAY_WHILST_DRAWING,
    ID_DISPLAY_ALWAYS,
    
    ID_DISPLAY_DEF_LAST
};

/** Implementing MyFrameODToolbar */
class ODToolbarImpl : public ODToolbarDialog
{
public:
	/** Constructor */
    ODToolbarImpl( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint &pos, const wxSize &size, long style );
    ~ODToolbarImpl();
    void OnClose( wxCloseEvent& event );
    void OnToolButtonClick( wxCommandEvent& event );
    void OnKeyDown( wxKeyEvent& event );
    void SetToolbarTool( int iTool );
    void UpdateIcons( void );
    void SetToolbarToolEnableOnly( int iTool );
    void SetToolbarToolEnableAll( void );
    void SetColourScheme( PI_ColorScheme cs );
    
    wxToolBarToolBase *m_toolBoundary;
    wxToolBarToolBase *m_toolODPoint;
    wxToolBarToolBase *m_toolTextPoint;
    wxToolBarToolBase *m_toolEBL;
    wxToolBarToolBase *m_toolDR;
    wxToolBarToolBase *m_toolGZ;
    wxToolBarToolBase *m_toolPIL;
    int    m_Mode;
    
private:
    void AddTools( void );
    void SetToolbarToolToggle( int iTool );
    void SetToolbarToolBitmap( int iTool );
    
    wxSize m_toolbarSize;
    PI_ColorScheme m_ColourScheme;
    
};

#endif // __ODToolbarImpl__

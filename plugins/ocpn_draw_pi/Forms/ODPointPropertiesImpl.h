#ifndef __ODPointPropertiesImpl__
#define __ODPointPropertiesImpl__

/**
@file
Subclass of ODPointPropertiesDialog, which is generated by wxFormBuilder.
*/

#include "ODPointPropertiesDialog.h"

/** Implementing ODPointPropertiesDialog */
class ODPointPropertiesImpl : public ODPointPropertiesDialog
{
protected:
	// Handlers for ODPointPropertiesDialog events.
	void OnRightClick( wxMouseEvent& event );
	void OnPositionCtlUpdated( wxCommandEvent& event );
	void OnRightClick( wxMouseEvent& event );
	void OnArrivalRadiusChange( wxCommandEvent& event );
	void OnShowRangeRingsSelect( wxCommandEvent& event );
	void OnRangeRingsStepChange( wxCommandEvent& event );
	void OnDescChangedBasic( wxCommandEvent& event );
	void OnExtDescriptionClick( wxCommandEvent& event );
	void OnPointPropertiesOKClick( wxCommandEvent& event );
	void OnPointPropertiesCancelClick( wxCommandEvent& event );
	
public:
	/** Constructor */
	ODPointPropertiesImpl( wxWindow* parent );
};

#endif // __ODPointPropertiesImpl__

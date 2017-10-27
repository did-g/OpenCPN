/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  Chart Scale Plugin
 * Author:   Sean D'Epagnier
 *
 ***************************************************************************
 *   Copyright (C) 2015 by Sean D'Epagnier
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************
 */

class ChartScaleDialog : public ChartScaleDialogBase
{
public:
    ChartScaleDialog( wxWindow *parent, chartscale_pi &_chartscale_pi, wxPoint position, int size,
                      int transparency, long style, long orientation, bool lastbutton );

    void SetVP( PlugIn_ViewPort &vp );
    void OnScale( wxScrollEvent& event );
    void OnScaleClicked( wxMouseEvent& event );
    void OnLast( wxCommandEvent& event );
        
private:
    wxSlider *m_sScale;
    chartscale_pi &m_chartscale_pi;
    PlugIn_ViewPort m_vp;
    double m_lastppm;
};

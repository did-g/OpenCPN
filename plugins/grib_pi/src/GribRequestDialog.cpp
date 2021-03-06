/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  GRIB Object
 * Author:   David Register
 *
 ***************************************************************************
 *   Copyright (C) 2010 by David S. Register   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.         *
 ***************************************************************************
 *
 */

#include "wx/wx.h"
#include <wx/url.h>
#include <wx/wfstream.h>
#include <wx/sstream.h>

#include "email.h"

#include "grib_pi.h"

#include "TexFont.h"

#define RESOLUTIONS 4

/* ref
    http://www.nco.ncep.noaa.gov/pmb/products/nam/
*/
enum Provider { SAILDOCS, ZYGRIB, NOAA, METEO_F, OPENGRIBS};  //grib providers

enum Model : int { GFS=0, GFS_ENS, HRRR, NAM, COAMPS, RTOFS, OSCAR, ICON, ARxxx };      //forecast models
const static wxString s1[] = {_T("GFS"), _T("GFS Ensemble"), _T("HRRR"), _T("NAM Car/CA"), _T("COAMPS"), _T("RTOFS"), 
        _T("OSCAR"), _T("Icon DWD"), _T("Meteo France")};

enum Field   {PRMSL  = (1 <<  0), // (presssure at sea level)
              WIND   = (1 <<  1), // (10 meters above surface)
              GUST   = (1 <<  2), //  (at 10 meters)
              AIRTMP = (1 <<  3), //  (temperature 2 meters above surface)
              SFCTMP = (1 <<  4), //  (temp at surface)
              RH     = (1 <<  5), //  (Relative Humidity 2m above surface)
              LFTX   = (1 <<  6), //  (LiFTed indeX)
              CAPE   = (1 <<  7), //  (Clear Air Potential Energy)
              RAIN   = (1 <<  8), //  (Precip rate, mm/hr)
              APCP   = (1 <<  9), //  (Accumulated precip)
              HGT300 = (1 << 10), //  (300mb height)
              TMP300 = (1 << 11), //  (temperature at 300mb level)
              WIND300= (1 << 12), //  (Wind velocity at 300mb level)
              HGT500 = (1 << 13), //  (500mb height)
              TMP500 = (1 << 14), //  (temperature at 500mb level)
              WIND500= (1 << 15), //  (Wind velocity at 500mb level)
              ABSV   = (1 << 16), //  (Absolute vorticity at 500mb)
              HGT700 = (1 << 17), //  (700mb height)
              TMP700 = (1 << 18), //  (temperature at 700mb level)
              WIND700= (1 << 19), //  (Wind velocity at 700mb level)
              HGT850 = (1 << 20), //  (850mb height)
              TMP850 = (1 << 21), //  (temperature at 850mb level)
              WIND850= (1 << 22), //  (Wind velocity at 850mb level)
              CLOUDS = (1 << 23), //  (Total cloud cover)
              WAVES  = (1 << 24), //  can be added to include sign wave height from the WW3 model.
              SEATMP = (1 << 25), //  (sea temp at surface)
              CURRENT= (1 << 26), //  (current)
              REFC   = (1 << 27), //  (Composite reflectivity)
              MSLET  = (1 << 28), //  (Mean Sea Level Pressure (ETA Model Reduction))
};

#include <set>

enum Resolution {r0_01, r0_025, r0_08, r0_10, r0_15, r0_25, r0_33, r0_5, r1_0, r2_0};

static const std::set<int> T16 = { 2, 3 , 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
static const std::set<int> T8  = { 1, 2, 3, 4, 5, 6, 7, 8 };
static const std::set<int> T5  = { 1, 2, 3, 4, 5 };
static const std::set<int> T4  = { 1, 2, 3, 4 };
static const std::set<int> T1  = { 1 };

static const std::set<int> I3_12 = { 3, 6, 12 };
static const std::set<int> I3_24 = { 3, 6, 12, 24 };
static const std::set<int> I1 = { 1 };
static const std::set<int> I3 = { 3 };

class ModelDef{
public:
    int Parameters;
    int Default;
    const std::map<Resolution, const std::set<int>> Intervals;
    const std::set<int> TimeRanges;

    bool  Has( Field a) { return (Parameters & (a)) != 0; }

} ;

typedef std::map<Model, ModelDef> ModelDefs;
typedef std::map<Provider, ModelDefs> Providers;

static const Providers P = { 
    // 2018-05-25 GFS=0, HRRR, COAMPS, RTOFS, OSCAR, ARxxx
    {SAILDOCS,{ {GFS, {/* .Parameters = */ PRMSL|WIND|GUST|AIRTMP|CAPE|RAIN|APCP|HGT500|TMP500|WIND500|CLOUDS|WAVES,
                       /* .Default = */  PRMSL | WIND ,
                       /* .Intervals  = */ {{r0_25, I3_12 },
                                       {r0_5, I3_12 },
                                       {r1_0, I3_12 } 
                                       },
                       /* .TimeRanges = */ T16
                       }
                },
                {HRRR, {/* .Parameters = */ PRMSL|WIND|GUST|AIRTMP|CAPE|RAIN,
                        /* .Default = */  WIND,
                        /* .Intervals = */ {{r0_025, I1 }, {r0_25, I1 }, {r0_5, I1} },
                        /* .TimeRanges = */ T1,
                       }
                },
                {COAMPS, {/* .Parameters = */ PRMSL|WIND,
                          /* .Default = */ PRMSL | WIND,
                          /* .Intervals = */ {{r0_15, { 3, 6} },},
                          /* .TimeRanges = */ { 2, 3 , 4 }
                          }
                },
                {RTOFS, {/* .Parameters = */ SEATMP|CURRENT,
                          /* .Default = */  CURRENT,
                         /* .Intervals =  */ {{r0_08, { 24} } },
                         /* .TimeRanges = */ { 2, 3 , 4, 5, 6, 7, 8 }
                        }
                },
                {OSCAR, {/* .Parameters = */ CURRENT,
                          /* .Default = */  CURRENT,
                         /* .Intervals = */ {{r0_33, { 3, 6} }},
                         /* .TimeRanges =*/  T5,
                       } 
                }
            },
    },
    // ===================
    {ZYGRIB, {{GFS, {/* .Parameters = */ PRMSL|WIND|GUST|AIRTMP|CAPE|RAIN|APCP|HGT500|TMP500|WIND500|
                            HGT300|TMP300|WIND300|HGT700|TMP700|WIND700|HGT850|TMP850|WIND850|CLOUDS|WAVES,
                     /* .Default = */  PRMSL | WIND,
                     /* .Intervals = */ {{r0_25, I3_12 },
                                   {r0_5, I3_12 },
                                   {r1_0, I3_12 },
                                   {r2_0, I3_24 } },
                     /* .TimeRanges = */ T16
                   },
              }},
    },
    // ===================
    {NOAA, {{GFS, {/* .Parameters = */ PRMSL|MSLET|WIND|GUST|AIRTMP|CAPE|RAIN|APCP|HGT500|TMP500|WIND500|CLOUDS,
                   /* .Default = */  WIND,
                   /* .Intervals = */ {{r0_25, I1 },
                                 {r0_5,  I3 },
                                 {r1_0,  I3 },
                                },
                   /* .TimeRanges = */ T16
                  },
            },
            {GFS_ENS, {/* .Parameters = */ PRMSL|WIND|AIRTMP|APCP|HGT500|TMP500|WIND500|CLOUDS,
                       /* .Default = */  PRMSL | WIND ,
                       /* .Intervals  = */ {{r0_5, I3_12 },
                                       {r1_0, { 6 } } 
                                       },
                       /* .TimeRanges = */ T16
                       }
            },
            {HRRR, {/* .Parameters = */ PRMSL|WIND|GUST|AIRTMP|CAPE|/*RAIN|*/APCP|REFC|CLOUDS,
                     /* .Default = */  WIND,
                    /* .Intervals =  */ {{r0_025, { 0, 1 } } },
                    /* .TimeRanges = */ T1,
                   },
            },
            {NAM, {/* .Parameters = */ PRMSL|WIND|AIRTMP|CAPE|APCP|SEATMP|REFC|CLOUDS,
                    // no Gust in inventory ! 
                    /* .Default = */  WIND, 
                    /* .Intervals =  */ {{r0_10, I3 }},
                    /* .TimeRanges = */ { 1, 2, 3 , 4 }
                   },
            },
        },
    },
    // ===================
    {OPENGRIBS, {{ICON, {/* .Parameters = */ PRMSL|WIND|GUST|AIRTMP|CAPE|RAIN|APCP|HGT500|TMP500|WIND500|
                                 HGT300|TMP300|WIND300|HGT700|TMP700|WIND700|HGT850|TMP850|WIND850|CLOUDS,
                   /* .Default = */  WIND,
                   /* .Intervals = */ {{r0_25, I3 },
                                },
                   /* .TimeRanges = */ T8
                  },
            },
            {ARxxx, {/* .Parameters = */ PRMSL|WIND|GUST|AIRTMP|CAPE|RAIN|APCP|HGT500|TMP500|WIND500|
                            HGT300|TMP300|WIND300|HGT700|TMP700|WIND700|HGT850|TMP850|WIND850|CLOUDS,
                       /* .Default = */  PRMSL | WIND ,
                       /* .Intervals  = */ {{r0_5, I3 },
                                       },
                       /* .TimeRanges = */ T4
                       }
            },
        },
    },
    // ===================
    {METEO_F, {{ARxxx, {/* .Parameters = */ PRMSL|WIND|GUST|AIRTMP|CAPE|RAIN|APCP|CLOUDS|WAVES|CURRENT,
                        /* .Default = */  WIND,
                        /* .Intervals = */ {{r0_01, I1,},
                                   {r0_025, I1 },
                                   {r0_10, I1} },
                     /* .TimeRanges = */ T5
                   },
              }},
    },
};

static const std::map<Resolution, const wxString> ResString = {
        {r0_01,  _T("0.01")},
        {r0_025, _T("0.025")},
        {r0_08,  _T("0.08")},
        {r0_10,  _T("0.10")},
        {r0_15,  _T("0.15")},
        {r0_25,  _T("0.25")},
        {r0_33,  _T("0.33")},
        {r0_5,   _T("0.5")},
        {r1_0,   _T("1.0")},
        {r2_0,   _T("2.0")}
};


static wxString toMailFormat ( int NEflag, int a )                 //convert position to mail necessary format
{
    char c = NEflag == 1 ? a < 0 ? 'S' : 'N' : a < 0 ? 'W' : 'E';
    wxString s;
    s.Printf ( _T ( "%01d%c" ), abs(a), c );
    return s;
}

extern int m_SavedZoneSelMode;
extern int m_ZoneSelMode;

//----------------------------------------------------------------------------------------------------------
//          GRIB Request Implementation
//----------------------------------------------------------------------------------------------------------
GribRequestSetting::GribRequestSetting(GRIBUICtrlBar &parent )
    : GribRequestSettingBase(&parent),
      m_parent(parent)
{
    m_Vp = 0;
    m_bconnected = false;
    InitRequestConfig();
}

GribRequestSetting::~GribRequestSetting( )
{
    Disconnect(wxEVT_DOWNLOAD_EVENT, (wxObjectEventFunction)(wxEventFunction)&GribRequestSetting::onDLEvent);
    delete m_Vp;
}

void GribRequestSetting::InitRequestConfig()
{
    wxFileConfig *pConf = GetOCPNConfigObject();

    if(pConf) {
        pConf->SetPath ( _T( "/PlugIns/GRIB" ) );
        wxString l;
        int m;
        pConf->Read ( _T( "MailRequestConfig" ), &m_RequestConfigBase, _T( "000220XX..............." ) );
        pConf->Read ( _T( "MailSenderAddress" ), &l, _T("") );
        m_pSenderAddress->ChangeValue( l );
        pConf->Read ( _T( "MailRequestAddresses" ), &m_MailToAddresses, _T("query@saildocs.com;gribauto@zygrib.org") );
        pConf->Read ( _T( "ZyGribLogin" ), &l, _T("") );
        m_pLogin->ChangeValue( l );
        pConf->Read ( _T( "ZyGribCode" ), &l, _T("") );
        m_pCode->ChangeValue( l );
        pConf->Read ( _T( "SendMailMethod" ), &m_SendMethod, 0 );
        pConf->Read ( _T( "MovingGribSpeed" ), &m, 0 );
        m_sMovingSpeed->SetValue( m );
        pConf->Read ( _T( "MovingGribCourse" ), &m, 0 );
        m_sMovingCourse->SetValue( m );
        m_cManualZoneSel->SetValue( m_SavedZoneSelMode != AUTO_SELECTION );      //has been read in GriUbICtrlBar dialog implementation or updated previously
        m_cUseSavedZone->SetValue( m_SavedZoneSelMode == SAVED_SELECTION );
        fgZoneCoordinatesSizer->ShowItems( m_SavedZoneSelMode != AUTO_SELECTION );
        m_cUseSavedZone->Show( m_SavedZoneSelMode != AUTO_SELECTION );
        if( m_cManualZoneSel->GetValue() ) {
            pConf->Read ( _T( "RequestZoneMaxLat" ), &m, 0 );
            m_spMaxLat->SetValue( m );
            pConf->Read ( _T( "RequestZoneMinLat" ), &m, 0 );
            m_spMinLat->SetValue( m );
            pConf->Read ( _T( "RequestZoneMaxLon" ), &m, 0 );
            m_spMaxLon->SetValue( m );
            pConf->Read ( _T( "RequestZoneMinLon" ), &m, 0 );
            m_spMinLon->SetValue( m );

            SetCoordinatesText();
        }
    //if GriDataConfig has been corrupted , take the standard one to fix a crash
    if( m_RequestConfigBase.Len() != wxString (_T( "000220XX..............." ) ).Len() )
        m_RequestConfigBase = _T( "000220XX..............." );
    }
    //populate model
    m_pModel->Clear();
    m_RevertMapModel.clear();
    for( unsigned int i= 0;  i< (sizeof(s1) / sizeof(wxString) );i++) {
        m_pModel->Append( s1[i] );
        m_RevertMapModel.push_back( (Model)i);
    }

    //populate mail to, waves model choices
    wxString s2[] = {_T("Saildocs"),_T("zyGrib"),_T("NOAA (web)"),_T("Meteo France(OCPN)"), _T("OpenGribs")};
    m_pMailTo->Clear();
    for( unsigned int i= 0;  i<(sizeof(s2) / sizeof(wxString));i++)
        m_pMailTo->Append( s2[i] );

    wxString s3[] = {_T("WW3-GLOBAL"),_T("WW3-MEDIT")};
    m_pWModel->Clear();
    for( unsigned int i= 0;  i<(sizeof(s3) / sizeof(wxString));i++)
        m_pWModel->Append( s3[i] );

    m_rButtonYes->SetLabel(_("Send"));
    m_rButtonApply->SetLabel(_("Save"));
    m_tResUnit->SetLabel(wxString::Format( _T("\u00B0")));
    m_sCourseUnit->SetLabel(wxString::Format( _T("\u00B0")));

    //Set wxSpinCtrl sizing
    int w,h;
    GetTextExtent( _T("-360"), &w, &h, 0, 0, OCPNGetFont(_("Dialog"), 10)); // optimal text control size
    w += 30;
    h += 4;
    m_sMovingSpeed->SetMinSize( wxSize(w, h) );
    m_sMovingCourse->SetMinSize( wxSize(w, h) );
    m_spMaxLat->SetMinSize( wxSize(w, h) );
    m_spMinLat->SetMinSize( wxSize(w, h) );
    m_spMaxLon->SetMinSize( wxSize(w, h) );
    m_spMinLon->SetMinSize( wxSize(w, h) );

    //add tooltips
    m_pSenderAddress->SetToolTip(_("Address used to send request eMail. (Mandatory for LINUX)"));
    m_pLogin->SetToolTip(_("This is your zyGrib's forum access Login"));
    m_pCode->SetToolTip(_("Get this Code in zyGrib's forum ( This is not your password! )"));
    m_sMovingSpeed->SetToolTip(_("Enter your forescasted Speed (in Knots)"));
    m_sMovingCourse->SetToolTip(_("Enter your forecasted Course"));

    long i,j,k;
    ( (wxString) m_RequestConfigBase.GetChar(0) ).ToLong( &i );             //MailTo
    m_pMailTo->SetSelection(i);
    ( (wxString) m_RequestConfigBase.GetChar(1) ).ToLong( &i );             //Model
    m_pModel->SetSelection(i);
    m_cMovingGribEnabled->SetValue(m_RequestConfigBase.GetChar(16) == 'X' );//Moving Grib
    ( (wxString) m_RequestConfigBase.GetChar(2) ).ToLong( &i );             //Resolution
    ( (wxString) m_RequestConfigBase.GetChar(3) ).ToLong( &j );             //interval
    ( (wxString) m_RequestConfigBase.GetChar(4) ).ToLong( &k, 16 );         //Time Range
    k--;                                         // range max = 2 to 16 stored in hexa from 1 to f

#ifdef __WXMSW__                                 //show / hide sender elemants as necessary
    m_pSenderSizer->ShowItems(false);
#else
    if(m_SendMethod == 0 )
        m_pSenderSizer->ShowItems(false);
    else
        m_pSenderSizer->ShowItems(true);                //possibility to use "sendmail" method with Linux
#endif

    m_tMouseEventTimer.Connect(wxEVT_TIMER, wxTimerEventHandler( GribRequestSetting::OnMouseEventTimer ), NULL, this);

    m_RenderZoneOverlay = 0;

    ApplyRequestConfig( i, j ,k);

    ( (wxString) m_RequestConfigBase.GetChar(5) ).ToLong( &j );             //Waves model
    m_pWModel->SetSelection( j );

    DimeWindow( this );                                                     //aplly global colours scheme

    m_AllowSend = true;
    m_MailImage->SetValue( WriteMail() +"\n\n");
}

void GribRequestSetting::OnClose( wxCloseEvent& event )
{
    m_RenderZoneOverlay = 0;                                    //eventually stop graphical zone display
    RequestRefresh( m_parent.pParent );

    //allow to be back to old value if changes have not been saved
    m_ZoneSelMode = m_SavedZoneSelMode;
    m_parent.SetRequestBitmap( m_ZoneSelMode );                                           //set appopriate bitmap

    this->Hide();
}

void GribRequestSetting::SetRequestDialogSize()
{
    int y;
    /*first let's size the mail display space*/
    GetTextExtent( _T("abc"), NULL, &y, 0, 0, OCPNGetFont(_("Dialog"), 10) );
    m_MailImage->SetMinSize( wxSize( -1, ( (y * m_MailImage->GetNumberOfLines()) + 10 ) ) );

    /*then as default sizing do not work with wxScolledWindow let's compute it*/
    wxSize scroll = m_fgScrollSizer->Fit(m_sScrolledDialog);                                   // the area size to be scrolled

#ifdef __WXGTK__
    SetMinSize( wxSize( 0, 0 ) );
#endif
    int w = GetOCPNCanvasWindow()->GetClientSize().x;           // the display size
    int h = GetOCPNCanvasWindow()->GetClientSize().y;
    int dMargin = 80;                                      //set a margin
    h -= ( m_rButton->GetSize().GetY() + dMargin );         //height available for the scrolled window
    w -= dMargin;                                           //width available for the scrolled window
    m_sScrolledDialog->SetMinSize( wxSize( wxMin( w, scroll.x ), wxMin( h, scroll.y ) ) );		//set scrolled area size with margin

	Layout();
    Fit();
#ifdef __WXGTK__
    wxSize sd = GetSize();
    if( sd.y == GetClientSize().y ) sd.y += 30;
    SetSize( wxSize( sd.x, sd.y ) );
    SetMinSize( wxSize( sd.x, sd.y ) );
#endif
    Refresh();
}

void GribRequestSetting::SetVpSize(PlugIn_ViewPort *vp)
{
    double lonmax= vp->lon_max;
    double lonmin= vp->lon_min;
    if( ( fabs(vp->lat_max ) < 90. ) && ( fabs( lonmax ) < 360. ) ) {
        if( lonmax < -180. ) lonmax += 360.;
        if( lonmax > 180. ) lonmax -= 360.;
    }
    if( ( fabs( vp->lat_min ) < 90. ) && ( fabs( lonmin ) < 360. ) ) {
        if( lonmin < -180. ) lonmin += 360.;
        if( lonmin > 180. ) lonmin -= 360.;
    }

    m_spMaxLat->SetValue( (int) ceil( vp->lat_max) );
    m_spMinLon->SetValue( (int) floor(lonmin) );
    m_spMinLat->SetValue( (int) floor(vp->lat_min) );
    m_spMaxLon->SetValue( (int) ceil(lonmax) );

    SetCoordinatesText();
    m_MailImage->SetValue( WriteMail() +"\n\n");
}

bool GribRequestSetting::MouseEventHook( wxMouseEvent &event )
{
    if( m_ZoneSelMode == AUTO_SELECTION || m_ZoneSelMode == SAVED_SELECTION || m_ZoneSelMode == START_SELECTION ) return false;

    if( event.Moving()) return false;                           //maintain status bar and tracking dialog updated

    if( event.LeftDown() ) {
        m_parent.pParent->SetFocus();
        m_ZoneSelMode = DRAW_SELECTION;                         //restart a new drawing
        m_parent.SetRequestBitmap( m_ZoneSelMode );
        if( this->IsShown() ) this->Hide();                     //eventually hide diaog in case of mode change
        m_RenderZoneOverlay = 0;                                //eventually hide previous drawing
    }

    if( event.LeftUp () && m_RenderZoneOverlay == 2 ) {
        m_ZoneSelMode = COMPLETE_SELECTION;                     //ask to complete selection
        m_parent.SetRequestBitmap( m_ZoneSelMode );
        SetCoordinatesText();
        m_MailImage->SetValue( WriteMail() +"\n\n");
        m_RenderZoneOverlay = 1;
    }

    if( event.Dragging() ) {
        if( m_RenderZoneOverlay < 2 ) {
            m_StartPoint = event.GetPosition();                                    //starting selection point
            m_RenderZoneOverlay = 2;
        }
		m_IsMaxLong = m_StartPoint.x > event.GetPosition().x? true: false;         //find if startpoint is max longitude 
        GetCanvasLLPix( m_Vp, event.GetPosition(), &m_Lat, &m_Lon);                //extend selection
        if( !m_tMouseEventTimer.IsRunning() ) m_tMouseEventTimer.Start( 20, wxTIMER_ONE_SHOT );
    }
    return true;
}

void GribRequestSetting::OnMouseEventTimer( wxTimerEvent & event)
{
    //compute zone starting point lon/lat for zone drawing
    double lat,lon;
    GetCanvasLLPix( m_Vp, m_StartPoint, &lat, &lon);

    //compute rounded coordinates
    if( lat > m_Lat) {
        m_spMaxLat->SetValue( (int) ceil(lat) );
        m_spMinLat->SetValue( (int) floor(m_Lat) );
    }
    else {
        m_spMaxLat->SetValue( (int) ceil(m_Lat) );
        m_spMinLat->SetValue( (int) floor(lat) );
    }
	if(m_IsMaxLong) {
        m_spMaxLon->SetValue( (int) ceil(lon) );
        m_spMinLon->SetValue( (int) floor(m_Lon) );
    }
    else {
        m_spMaxLon->SetValue( (int) ceil(m_Lon) );
        m_spMinLon->SetValue( (int) floor(lon) );
    }

    RequestRefresh( m_parent.pParent );
}

void GribRequestSetting::SetCoordinatesText()
{
    m_stMaxLatNS->SetLabel( m_spMaxLat->GetValue() < 0 ? _("S")  : _("N") );
    m_stMinLonEW->SetLabel( m_spMinLon->GetValue() < 0 ? _("W")  : _("E") );
    m_stMaxLonEW->SetLabel( m_spMaxLon->GetValue() < 0 ? _("W")  : _("E") );
    m_stMinLatNS->SetLabel( m_spMinLat->GetValue() < 0 ? _("S")  : _("N") );
}

void GribRequestSetting::StopGraphicalZoneSelection()
{
    m_RenderZoneOverlay = 0;                                                //eventually stop graphical zone display

    RequestRefresh( m_parent.pParent );
}

void GribRequestSetting::OnVpChange(PlugIn_ViewPort *vp)
{
    delete m_Vp;
    m_Vp = new PlugIn_ViewPort(*vp);

    if(!m_AllowSend) return;
    if( m_cManualZoneSel->GetValue() ) return;

    SetVpSize(vp);
}

Model GribRequestSetting::getModel() const
{
    // XXX or is wxNOT_FOUND possible?
    Model model = GFS;
    if (m_pModel->GetCurrentSelection() >= 0) {
        assert(m_pModel->GetCurrentSelection() < (int)m_RevertMapModel.size());
        model = m_RevertMapModel[m_pModel->GetCurrentSelection()];
    }
    return model;
}

const ModelDef GribRequestSetting::getModelDef() const
{
    // XXX or is wxNOT_FOUND possible?
    Model model = getModel();

    Provider provider = SAILDOCS;
    if ( m_pMailTo->GetCurrentSelection() > 0) provider = (Provider)m_pMailTo->GetCurrentSelection();

    const auto itP = P.find(provider);
    assert (itP != P.end());

    auto m = itP->second; // map of model definitions
    auto itM = m.find(model);
    // selected model is not in this provider's list, get another one
    if (itM == m.end()) itM = m.find(m.begin()->first);
    assert (itM != m.end());
    return itM->second;
}

void GribRequestSetting::ApplyRequestConfig( unsigned rs, unsigned it, unsigned tr )
{
    Model model = getModel();

    Provider provider = SAILDOCS;
    if ( m_pMailTo->GetCurrentSelection() > 0) provider = (Provider)m_pMailTo->GetCurrentSelection();

    IsZYGRIB =  provider == ZYGRIB;
    bool IsSAILDOCS = provider == SAILDOCS;

    const auto itP = P.find(provider);
    assert (itP != P.end());
    auto m = itP->second; // map of model definitions

    //populate model list
    m_pModel->Clear();
    m_RevertMapModel.clear();
    unsigned int imax = sizeof(s1) / sizeof(wxString);
    int midx = imax;
    for( unsigned int i= 0, j = 0;  i< imax; i++) {
        if (m.find((Model)i) == m.end())
            continue;
        m_pModel->Append( s1[i] );
        m_RevertMapModel.push_back((Model)i);
        if ((Model)i == model)
            midx = j;
        j++;
    }
    m_pModel->SetSelection(wxMin(midx, m_pModel->GetCount()-1));

    auto itM = m.find(model);
    // selected model is not in this provider's list, get another one
    if (itM == m.end()) itM = m.find(m.begin()->first);
    assert (itM != m.end());
    if (m.size() == 1) {
        m_pModel->SetSelection(0);
        m_pModel->Enable(false);
    } 
    else {
        m_pModel->Enable(true);
    }
    model = m_RevertMapModel[m_pModel->GetCurrentSelection()];
    itM = m.find(model);
    IsGFS = model == GFS;

    //populate resolution choice
    m_pResolution->Clear();
    ModelDef d = itM->second;
    for (const auto r: d.Intervals) {
        assert(ResString.find( r.first) != ResString.end());
        m_pResolution->Append(ResString.find( r.first)->second);
    }
    if (d.Intervals.size() == 1) {
        m_pResolution->SetSelection(0);
        m_pResolution->Enable(false);
    }
    else {
        m_pResolution->Enable(true);
        m_pResolution->SetSelection(wxMin(rs, m_pResolution->GetCount()-1));
    }
    rs =  m_pResolution->GetCurrentSelection();
    //populate time interval choice
    m_pInterval->Clear();
    for( auto r: d.Intervals ) {
        if (rs == 0) {
            for ( auto i : r.second) {
                if (i == 0)
                    m_pInterval->Append( _T("1/4"));
                else 
                    m_pInterval->Append( wxString::Format(_T("%d"), i));
            }
            break;
        }
        rs--;
    }
    m_pInterval->SetSelection(wxMin(it, m_pInterval->GetCount()-1));
    m_pInterval->Enable(m_pInterval->GetCount() > 1);

    //populate time range choice
    m_pTimeRange->Clear();
    for( auto i: d.TimeRanges ) {
        m_pTimeRange->Append( wxString::Format(_T("%d"), i));
    }
    m_pTimeRange->Enable(m_pTimeRange->GetCount()-1);
    m_pTimeRange->SetSelection(wxMin(tr, m_pTimeRange->GetCount()-1));

    m_pWind->SetValue( (d.Default & WIND) || (m_RequestConfigBase.GetChar(6) == 'X' && d.Has(WIND)) );
    m_pWind->Enable( d.Has(WIND) && !(d.Default & WIND));

    m_pPress->SetValue( (d.Default & PRMSL) || (m_RequestConfigBase.GetChar(7) == 'X' && d.Has(PRMSL)) );
    m_pPress->Enable( d.Has(PRMSL) && !(d.Default & PRMSL));

    m_pWaves->SetValue( m_RequestConfigBase.GetChar(8) == 'X' && d.Has(WAVES));
    m_pWaves->Enable( d.Has(WAVES) && m_pTimeRange->GetCurrentSelection() < 7 );      //gfs & time range less than 8 days

    m_pRainfall->SetValue( m_RequestConfigBase.GetChar(9) == 'X' && (d.Has(RAIN) || d.Has(APCP)) );
    m_pRainfall->Enable( d.Has(RAIN) || d.Has(APCP) );

    m_pCloudCover->SetValue( m_RequestConfigBase.GetChar(10) == 'X' && d.Has(CLOUDS) );
    m_pCloudCover->Enable(  d.Has(CLOUDS) );

    m_pAirTemp->SetValue( m_RequestConfigBase.GetChar(11) == 'X' && d.Has(AIRTMP) );
    m_pAirTemp->Enable(  d.Has(AIRTMP) );

    m_pSeaTemp->SetValue( m_RequestConfigBase.GetChar(12) == 'X' && d.Has(SEATMP) );
    m_pSeaTemp->Enable(  d.Has(SEATMP) );

    m_pCurrent->SetValue( (d.Default & CURRENT) || ( m_RequestConfigBase.GetChar(13) == 'X' && d.Has(CURRENT)) );
    m_pCurrent->Enable( d.Has(CURRENT) && !(d.Default & CURRENT));

    m_pWindGust->SetValue( m_RequestConfigBase.GetChar(14) == 'X' && d.Has(GUST) );
    m_pWindGust->Enable(  d.Has(GUST) );

    m_pCAPE->SetValue( m_RequestConfigBase.GetChar(15) == 'X' && d.Has(CAPE) );
    m_pCAPE->Enable(  d.Has(CAPE) );

    m_pReflectivity->SetValue( m_RequestConfigBase.GetChar(22) == 'X' && d.Has(REFC) );
    m_pReflectivity->Enable(  d.Has(REFC) );

    bool HaveAlt = d.Has(HGT300) || d.Has(HGT500) || d.Has(HGT700)|| d.Has(HGT850);
    m_pAltitudeData->SetValue( HaveAlt ? m_RequestConfigBase.GetChar(17) == 'X' : false );       //altitude 
    m_pAltitudeData->Enable( HaveAlt );

    m_p850hpa->SetValue( d.Has(HGT850) ? m_RequestConfigBase.GetChar(18) == 'X' : false );
    m_p850hpa->Enable( d.Has(HGT850)  );

    m_p700hpa->SetValue( d.Has(HGT700) ? m_RequestConfigBase.GetChar(19) == 'X' : false );
    m_p700hpa->Enable( d.Has(HGT700)  );

    m_p500hpa->SetValue( d.Has(HGT500) ? m_RequestConfigBase.GetChar(20) == 'X' : false );
    m_p500hpa->Enable( d.Has(HGT500) );

    m_p300hpa->SetValue( d.Has(HGT300) ? m_RequestConfigBase.GetChar(21) == 'X' : false  );
    m_p300hpa->Enable( d.Has(HGT300) );


    //show parameters only if necessary
    m_cMovingGribEnabled->Show(IsSAILDOCS);                             //show/hide Moving settings
    m_fgMovingParams->ShowItems( m_cMovingGribEnabled->IsChecked() && m_cMovingGribEnabled->IsShown() );

    m_fgLog->ShowItems(IsZYGRIB);                                           //show/hide zigrib login

    m_pWModel->Show(IsZYGRIB && m_pWaves->IsChecked());                     //show/hide waves model

    m_fgAltitudeData->ShowItems(m_pAltitudeData->IsChecked());              //show/hide altitude params
}

void GribRequestSetting::OnTopChange(wxCommandEvent &event)
{
    int it = m_pInterval->GetCurrentSelection();

    ApplyRequestConfig( m_pResolution->GetCurrentSelection(), it, m_pTimeRange->GetCurrentSelection() );

    m_cMovingGribEnabled->Show( m_pMailTo->GetCurrentSelection() == SAILDOCS );

    if(m_AllowSend) m_MailImage->SetValue( WriteMail() +"\n\n");

    SetRequestDialogSize();
}

void GribRequestSetting::OnResolutionChange(wxCommandEvent &event)
{
    int it = m_pInterval->GetCurrentSelection();

    ApplyRequestConfig( m_pResolution->GetCurrentSelection(), it, m_pTimeRange->GetCurrentSelection() );

    if(m_AllowSend) m_MailImage->SetValue( WriteMail() +"\n\n");

    SetRequestDialogSize();
}

void GribRequestSetting::OnZoneSelectionModeChange( wxCommandEvent& event )
{
    StopGraphicalZoneSelection();                       //eventually stop graphical zone display

    if( !m_ZoneSelMode )
        SetVpSize( m_Vp );                              //recompute zone

    if( event.GetId() == MANSELECT ) {
        //set temporarily zone selection mode if manual selection set, put it directly in "drawing" position
        //else put it in "auto selection position
        m_ZoneSelMode = m_cManualZoneSel->GetValue() ? DRAW_SELECTION : AUTO_SELECTION;
        m_cUseSavedZone->SetValue( false );
    } else if(event.GetId() == SAVEDZONE ) {
        //set temporarily zone selection mode if saved selection set, put it directly in "no selection" position
        //else put it directly in "drawing" position
        m_ZoneSelMode = m_cUseSavedZone->GetValue()? SAVED_SELECTION : DRAW_SELECTION;
    }
    m_parent.SetRequestBitmap( m_ZoneSelMode );               //set appopriate bitmap
    fgZoneCoordinatesSizer->ShowItems( m_ZoneSelMode != AUTO_SELECTION ); //show coordinate if necessary
    m_cUseSavedZone->Show( m_ZoneSelMode != AUTO_SELECTION );
    if(m_AllowSend) m_MailImage->SetValue( WriteMail() +"\n\n");

    SetRequestDialogSize();
}

bool GribRequestSetting::DoRenderZoneOverlay()
{
    wxPoint p;
    GetCanvasPixLL( m_Vp, &p, m_Lat, m_Lon);

    int x = (m_StartPoint.x < p.x) ? m_StartPoint.x : p.x;
    int y = (m_StartPoint.y < p.y) ? m_StartPoint.y : p.y;

    int zw = fabs( (double ) p.x - m_StartPoint.x );
    int zh = fabs( (double ) p.y - m_StartPoint.y );

    wxPoint center;
    center.x = x + (zw / 2);
    center.y = y + (zh / 2);

    wxFont *font = OCPNGetFont(_("Dialog"), 10);
    wxColour pen_color, back_color;
    GetGlobalColor( _T ( "DASHR" ), &pen_color );
    GetGlobalColor( _T ( "YELO1" ), &back_color );

    int label_offsetx = 5, label_offsety = 1;

    double size;
    EstimateFileSize( &size );

    wxString label( _("Coord. ") );
    label.Append( toMailFormat(1, m_spMaxLat->GetValue()) + _T(" "));
    label.Append( toMailFormat(0, m_spMinLon->GetValue()) + _T(" "));
    label.Append( toMailFormat(1, m_spMinLat->GetValue()) + _T(" "));
    label.Append( toMailFormat(0, m_spMaxLon->GetValue()) + _T("\n"));
    label.Append( _T("Estim. Size ") ).Append((wxString::Format( _T("%1.2f " ) , size ) + _("MB") ) );

    if( m_pdc ) {
        wxPen pen(pen_color);
        pen.SetWidth(3);
        m_pdc->SetPen( pen );
        m_pdc->SetBrush( *wxTRANSPARENT_BRUSH);
        m_pdc->DrawRectangle(x, y, zw, zh);



        int w, h, sl;
#ifdef __WXMAC__
        wxScreenDC sdc;
        sdc.GetMultiLineTextExtent(label, &w, &h, &sl, font);
#else
        m_pdc->GetMultiLineTextExtent(label, &w, &h, &sl, font);
#endif
        w += 2*label_offsetx, h += 2*label_offsety;
        x = center.x - (w / 2);
        y = center.y - (h / 2);

        wxBitmap bm(w, h);
        wxMemoryDC mdc(bm);
        mdc.Clear();

        mdc.SetFont( *font );
        mdc.SetBrush(back_color);
        mdc.SetPen(*wxTRANSPARENT_PEN);
        mdc.SetTextForeground(wxColor( 0, 0, 0 ));
        mdc.DrawRectangle(0, 0, w, h);
        mdc.DrawLabel( label, wxRect( label_offsetx, label_offsety, w, h ) );

        wxImage im = bm.ConvertToImage();
        im.InitAlpha();
        w = im.GetWidth(), h = im.GetHeight();
        for( int j = 0; j < h; j++ )
			for( int i = 0; i < w; i++ )
				im.SetAlpha( i, j, 155 );

        m_pdc->DrawBitmap(im, x, y, true);

    } else {

#ifdef ocpnUSE_GL
    TexFont m_TexFontlabel;
    m_TexFontlabel.Build(*font);

    glColor3ub(pen_color.Red(), pen_color.Green(), pen_color.Blue() );

    glPushAttrib(GL_COLOR_BUFFER_BIT | GL_LINE_BIT | GL_ENABLE_BIT |
                     GL_POLYGON_BIT | GL_HINT_BIT );

   glEnable( GL_LINE_SMOOTH );
   glEnable( GL_BLEND );
   glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
   glHint( GL_LINE_SMOOTH_HINT, GL_NICEST );
   glLineWidth( 3.f );

   glBegin( GL_LINES );
   glVertex2d( x, y );
   glVertex2d( x+zw, y );
   glVertex2d( x+zw, y );
   glVertex2d( x+zw, y+zh );
   glVertex2d( x+zw, y+zh );
   glVertex2d( x, y+zh );
   glVertex2d( x, y+zh );
   glVertex2d( x, y );
   glEnd();

   int w, h;
   glColor4ub(back_color.Red(), back_color.Green(), back_color.Blue(), 155 );
   m_TexFontlabel.GetTextExtent(label, &w, &h );

    w += 2*label_offsetx, h += 2*label_offsety;
    x = center.x - (w / 2);
    y = center.y - (h / 2);

   /* draw text background */
   glBegin(GL_QUADS);
   glVertex2i(x,   y);
   glVertex2i(x+w, y);
   glVertex2i(x+w, y+h);
   glVertex2i(x,   y+h);
   glEnd();

   /* draw text */
   glColor3ub( 0, 0, 0 );

   glEnable(GL_TEXTURE_2D);
   m_TexFontlabel.RenderString(label, x + label_offsetx, y + label_offsety);
   glDisable(GL_TEXTURE_2D);

   glDisable( GL_BLEND );

   glPopAttrib();
   
#endif
    }
    return true;
}

bool GribRequestSetting::RenderGlZoneOverlay()
{
    if( m_RenderZoneOverlay == 0 ) return false;
    m_pdc = NULL;                  // inform lower layers that this is OpenGL render
    return DoRenderZoneOverlay();
}

bool GribRequestSetting::RenderZoneOverlay( wxDC &dc )
{
    if( m_RenderZoneOverlay == 0 ) return false;
    m_pdc = &dc;
    return DoRenderZoneOverlay();
}

void GribRequestSetting::OnMovingClick( wxCommandEvent& event )
{
    m_fgMovingParams->ShowItems( m_cMovingGribEnabled->IsChecked() && m_cMovingGribEnabled->IsShown() );

    if(m_AllowSend) m_MailImage->SetValue( WriteMail() +"\n\n");
    SetRequestDialogSize();

    this->Refresh();
}

void GribRequestSetting::OnCoordinatesChange( wxSpinEvent& event )
{
    SetCoordinatesText();

    StopGraphicalZoneSelection();                           //eventually stop graphical zone display

    if( !m_AllowSend ) return;

    m_MailImage->SetValue( WriteMail() +"\n\n");
}



void GribRequestSetting::OnAnyChange(wxCommandEvent &event)
{
    m_fgAltitudeData->ShowItems(m_pAltitudeData->IsChecked());

    m_pWModel->Show( IsZYGRIB && m_pWaves->IsChecked());

    if(m_AllowSend) m_MailImage->SetValue( WriteMail() +"\n\n");

    SetRequestDialogSize();
}

void GribRequestSetting::OnTimeRangeChange(wxCommandEvent &event)
{
    auto d = getModelDef();

    if( d.Has(WAVES)) {               //gfs
        if( m_pTimeRange->GetCurrentSelection() > 6 ) {         //time range more than 8 days
            m_pWaves->Enable(false);
            if (m_pWaves->IsChecked()) {
                OCPNMessageBox_PlugIn(this, _("You request a forecast for more than 8 days horizon.\nThis is conflicting with Wave data which will be removed from your request.\nDon't forget that beyond the first 8 days, the resolution will be only 2.5\u00B0x2.5\u00B0\nand the time intervall 12 hours."),
                    _("Warning!") );
            }
            m_pWaves->SetValue(0);
        } else
            m_pWaves->Enable(true);
    }
    else
        m_pWaves->Enable(false);
    m_pWModel->Show( IsZYGRIB && m_pWaves->IsChecked());

    if(m_AllowSend) m_MailImage->SetValue( WriteMail() +"\n\n");

    SetRequestDialogSize();
}

void GribRequestSetting::OnSaveMail( wxCommandEvent& event )
{
    m_RequestConfigBase.SetChar( 0, (char) ( m_pMailTo->GetCurrentSelection() + '0' ) );            //recipient
    m_cMovingGribEnabled->IsChecked() ? m_RequestConfigBase.SetChar( 16, 'X' )                      //moving grib
        : m_RequestConfigBase.SetChar( 16, '.' );

    m_RequestConfigBase.SetChar( 1, (char) ( m_pModel->GetCurrentSelection() + '0' ) );         //model idx

    m_RequestConfigBase.SetChar( 2, (char) ( m_pResolution->GetCurrentSelection() + '0' ) );    //resolution

    int it = m_pInterval->GetCurrentSelection();

    m_RequestConfigBase.SetChar( 3, (char) ( it + '0' ) );
    auto d = getModelDef();

    wxString range;
    range.Printf(_T("%x"), m_pTimeRange->GetCurrentSelection() + 1 );                  //range max = 2 to 16 stored in hexa 1 to f
    m_RequestConfigBase.SetChar( 4, range.GetChar( 0 ) );

    if( IsZYGRIB && m_pWModel->IsShown() )
        m_RequestConfigBase.SetChar( 5, (char) ( m_pWModel->GetCurrentSelection() + '0' ) );        //waves model

    if (d.Has(WIND)) {
        char c = m_pWind->IsChecked() ? 'X':'.';      	// wind
        m_RequestConfigBase.SetChar( 6, c );
    }
    if (d.Has(PRMSL)) {
        char c = m_pPress->IsChecked() ? 'X':'.';     	// pressure 
        m_RequestConfigBase.SetChar( 7, c );
    }
    if (d.Has(WAVES)) {
        char c = m_pWaves->IsChecked() ? 'X':'.';     	// Waves
        m_RequestConfigBase.SetChar( 8, c );
    }
    if (d.Has(RAIN) || d.Has(APCP)) {
        char c = m_pRainfall->IsChecked() ? 'X':'.'; 	 // rainfall
        m_RequestConfigBase.SetChar( 9, c );
    }
    if (d.Has(CLOUDS)) {
        char c = m_pCloudCover->IsChecked() ? 'X':'.'; 	// clouds
        m_RequestConfigBase.SetChar( 10, c );
    }
    if (d.Has(AIRTMP)) {
        char c = m_pAirTemp->IsChecked() ? 'X':'.'; 	// air temp
        m_RequestConfigBase.SetChar( 11, c );
    }
    if (d.Has(SEATMP)) {
        char c = m_pSeaTemp->IsChecked() ? 'X':'.'; 	// sea temp
        m_RequestConfigBase.SetChar( 12, c );
    }
    if (d.Has(CURRENT)) {
        char c = m_pCurrent->IsChecked() ? 'X':'.';     // current
        m_RequestConfigBase.SetChar( 13, c );
    }
    if (d.Has(GUST)) {
        char c = m_pWindGust->IsChecked() ? 'X':'.';  	// Gust
        m_RequestConfigBase.SetChar( 14, c );
    }
    if (d.Has(CAPE)) {
        char c = m_pCAPE->IsChecked() ? 'X':'.';  	// GAPE
        m_RequestConfigBase.SetChar( 15, c );
    }
    if (d.Has(REFC)) {
        char c = m_pReflectivity->IsChecked() ? 'X':'.';// Reflectivity
        m_RequestConfigBase.SetChar( 22, c );
    }

    if (d.Has(HGT500)) {
        m_pAltitudeData->IsChecked() ?  m_RequestConfigBase.SetChar( 17, 'X' )                      //altitude data
        : m_RequestConfigBase.SetChar( 17, '.' );
        m_p500hpa->IsChecked() ?  m_RequestConfigBase.SetChar( 20, 'X' )
        : m_RequestConfigBase.SetChar( 20, '.' );
    }
    if (d.Has(HGT850)) {
        m_p850hpa->IsChecked() ?  m_RequestConfigBase.SetChar( 18, 'X' )
            : m_RequestConfigBase.SetChar( 18, '.' );
        m_p700hpa->IsChecked() ?  m_RequestConfigBase.SetChar( 19, 'X' )
            : m_RequestConfigBase.SetChar( 19, '.' );
        m_p300hpa->IsChecked() ?  m_RequestConfigBase.SetChar( 21, 'X' )
            : m_RequestConfigBase.SetChar( 21, '.' );
    }

    wxFileConfig *pConf = GetOCPNConfigObject();
    if(pConf) {
        pConf->SetPath ( _T( "/PlugIns/GRIB" ) );

        pConf->Write ( _T ( "MailRequestConfig" ), m_RequestConfigBase );
        pConf->Write ( _T( "MailSenderAddress" ), m_pSenderAddress->GetValue() );
        pConf->Write ( _T( "MailRequestAddresses" ), m_MailToAddresses );
        pConf->Write ( _T( "ZyGribLogin" ), m_pLogin->GetValue() );
        pConf->Write ( _T( "ZyGribCode" ), m_pCode->GetValue() );
        pConf->Write ( _T( "SendMailMethod" ), m_SendMethod );
        pConf->Write ( _T( "MovingGribSpeed" ), m_sMovingSpeed->GetValue() );
        pConf->Write ( _T( "MovingGribCourse" ), m_sMovingCourse->GetValue() );

        m_SavedZoneSelMode = m_cUseSavedZone->GetValue()? SAVED_SELECTION: m_cManualZoneSel->GetValue()? START_SELECTION: AUTO_SELECTION;
        pConf->Write ( _T( "ManualRequestZoneSizing" ), m_SavedZoneSelMode );

        pConf->Write ( _T( "RequestZoneMaxLat" ), m_spMaxLat->GetValue() );
        pConf->Write ( _T( "RequestZoneMinLat" ), m_spMinLat->GetValue() );
        pConf->Write ( _T( "RequestZoneMaxLon" ), m_spMaxLon->GetValue() );
        pConf->Write ( _T( "RequestZoneMinLon" ), m_spMinLon->GetValue() );

    }

    wxCloseEvent evt;
    OnClose ( evt );
}

/*
ensemble

https://nomads.ncep.noaa.gov/cgi-bin/filter_gens_0p50.pl?
   file=geavg.t06z.pgrb2a.0p50.f000&
   lev_10_m_above_ground=on&
   lev_2_m_above_ground=on&
   lev_500_mb=on&
   lev_entire_atmosphere=on&
   lev_entire_atmosphere_%5C%28considered_as_a_single_layer%5C%29=on&
   lev_mean_sea_level=on&
   lev_surface=on&var_APCP=on&var_CAPE=on&var_HGT=on&var_PRMSL=on&var_TCDC=on&var_TMP=on&var_UGRD=on&var_VGRD=on&
   subregion=&leftlon=0&rightlon=-90&toplat=60&bottomlat=0&
   dir=%2Fgefs.20180623%2F06%2Fpgrb2ap5

https://nomads.ncep.noaa.gov/cgi-bin/filter_gens.pl?
   file=gec00.t12z.pgrb2f00&
   lev_2_m_above_ground=on&lev_entire_atmosphere=on&lev_mean_sea_level=on&lev_surface=on&var_APCP=on&var_CAPE=on&var_PRATE=on&var_PRMSL=on&var_UGRD=on&var_VGRD=on&
   subregion=&leftlon=0&rightlon=360&toplat=90&bottomlat=-90&
   dir=%2Fgefs.20180630%2F12%2Fpgrb2
   
GFS
"https://nomads.ncep.noaa.gov/cgi-bin/filter_gfs_0p25_1hr.pl?
   file=gfs.t18z.pgrb2.0p25.f000&
   lev_500_mb=on&var_HGT=on&
   lev_entire_atmosphere=on&var_TCDC=on&
   lev_surface=on&var_CAPE=on&var_GUST=on&var_PRATE=on&
   lev_2_m_above_ground=on&var_TMP=on&
   lev_10_m_above_ground=on&var_UGRD=on&var_VGRD=on&
   lev_mean_sea_level=on&var_PRMSL=on&
   subregion=&leftlon=1&rightlon=-5&toplat=52&bottomlat=44&dir=%2Fgfs.2018052018

https://nomads.ncep.noaa.gov/cgi-bin/filter_gfs_0p50.pl?
   file=gfs.t00z.pgrb2full.0p50.f003&
   lev_10_m_above_ground=on&var_UGRD=on&var_VGRD=on&subregion=&leftlon=0&rightlon=360&toplat=90&bottomlat=-90&dir=%2Fgfs.2018052300

https://nomads.ncep.noaa.gov/cgi-bin/filter_gfs_1p00.pl?
   file=gfs.t06z.pgrb2.1p00.f003&
   lev_10_m_above_ground=on&var_UGRD=on&var_VGRD=on&subregion=&leftlon=0&rightlon=360&toplat=90&bottomlat=-90&dir=%2Fgfs.2018062306


https://nomads.ncep.noaa.gov/cgi-bin/filter_hrrr_2d.pl?
    file=hrrr.t00z.wrfsfcf01.grib2&
    lev_10_m_above_ground=on&lev_2_m_above_ground=on&var_TMP=on&var_UGRD=on&var_VGRD=on&
    subregion=&leftlon=0&rightlon=360&toplat=90&bottomlat=-90&dir=%2Fhrrr.20180527

http://nomads.ncep.noaa.gov/cgi-bin/filter_hrrr_sub.pl?
    file=hrrr.t22z.wrfsubhf06.grib2&
    lev_10_m_above_ground=on&lev_2_m_above_ground=on&lev_surface=on&
    var_GUST=on&var_PRATE=on&var_PRES=on&var_TMP=on&var_UGRD=on&var_VGRD=on&
    subregion=&leftlon=0&rightlon=360&toplat=90&bottomlat=-90&dir=%2Fhrrr.20180530

http://nomads.ncep.noaa.gov/cgi-bin/filter_nam_crb.pl?
    file=nam.t00z.afwaca03.tm00.grib2&
    lev_10_m_above_ground=on&lev_500_mb=on&lev_mean_sea_level=on&var_HGT=on&
    var_PRMSL=on&var_UGRD=on&var_VGRD=on&var_WTMP=on&subregion=&leftlon=0&rightlon=360&toplat=90&bottomlat=-90&
    dir=%2Fnam.20180603

*/
wxString GribRequestSetting::WriteMail()
{
    if  (m_pModel->GetCurrentSelection() < 0)
        return wxEmptyString;
    if  (m_pModel->GetCurrentSelection() >= (int)m_RevertMapModel.size())
        return wxEmptyString;

    int model = m_RevertMapModel[m_pModel->GetCurrentSelection()];
    int resolution = m_pResolution->GetCurrentSelection();
    int provider = m_pMailTo->GetCurrentSelection();

    //define size limits for zyGrib
    int limit = IsZYGRIB ? 2 :( provider == METEO_F) ?30:0; //new limit  2 mb
    auto d = getModelDef();

    m_MailError_Nb = 0;
    //some useful strings
    // {_T("0.25"), _T("0.5"), _T("1.0"), _T("2.0")},
    static const wxString m_o[] = { _T("icon_p25_"), _T("arpege_p50_") };
    static const wxString m[] = { _T("gfs"), _T("gens"), _T("hrrr"), _T("nam")};
    static const wxString M[] = {  _T("arome01?"), _T("arome?"), _T("arpege?")};

    static const wxString s[] = { _T(","), _T(" "), _T("&"), _T(""), _T(";")};        //separators

    static const wxString p[][18] = {
        //parameters Saildocs
        {_T("APCP"), _T("TCDC"), _T("AIRTMP"), _T("HTSGW,WVPER,WVDIR"),
            _T("SEATMP"), _T("GUST"), _T("CAPE"), wxEmptyString, wxEmptyString, _T("WIND500,HGT500"), wxEmptyString,
            _T("WIND"), _T("PRESS"), _T("PRESS"), wxEmptyString, _T("CUR"), wxEmptyString,
        }, 
        //parameters zigrib
        {_T("PRECIP"), _T("CLOUD"), _T("TEMP"), _T("WVSIG WVWIND"), wxEmptyString, _T("GUST"),
            _T("CAPE"), _T("A850"), _T("A700"), _T("A500"), _T("A300"),_T("WIND"), _T("PRESS")
            , wxEmptyString, wxEmptyString, wxEmptyString, wxEmptyString, wxEmptyString,
        } ,
        //parameters NOAA
        // lev_entire_atmosphere_%5C%28considered_as_a_single_layer%5C%29=on
        // lev_entire_atmosphere=on
        {   _T("var_PRATE=on"), _T("var_TCDC=on"), 
            _T("lev_2_m_above_ground=on&var_TMP=on"),
            wxEmptyString, _T("var_WTMP"), _T("var_GUST=on"),
            _T("var_CAPE=on"),wxEmptyString, wxEmptyString, 
            /* 9 */ _T("lev_500_mb=on&var_HGT=on&"), wxEmptyString,
            _T("lev_10_m_above_ground=on&var_UGRD=on&var_VGRD=on&lev_surface=on"),
            _T("lev_mean_sea_level=on&var_PRMSL=on"),
            _T("var_PRES=on"), _T("var_REFC=on"), wxEmptyString, 
            _T("var_APCP=on"), _T("lev_mean_sea_level=on&var_MSLET=on")
        },
        // Meteo France
        {_T("r"), _T("n"), _T("t"),
        _T("v"), wxEmptyString, _T("g"),
        _T("a"), wxEmptyString, wxEmptyString, 
        /* 9 */ wxEmptyString, wxEmptyString,
        _T("w"), _T("p"), wxEmptyString, wxEmptyString, _T("m"), wxEmptyString, wxEmptyString,
        },
        // OpenGribs
        {_T("R"), _T("C"), _T("T"),
        wxEmptyString, wxEmptyString,_T("G"),
        _T("a"), _T("8"), _T("7"), 
        /* 9 */ _T("5"), _T("3"),
        _T("W"), _T("P"), wxEmptyString, wxEmptyString, wxEmptyString, wxEmptyString,
        }
    };

    wxString r_topmess,r_parameters,r_zone;
    //write the top part of the mail
    switch( provider ) {
    case OPENGRIBS:
        r_zone.Printf ( _T ("&la1=%d&la2=%d&lo1=%d&lo2=%d"),
           m_spMinLat->GetValue(), m_spMaxLat->GetValue(), m_spMinLon->GetValue(), m_spMaxLon->GetValue());
        r_topmess = _T("http://grbsrv.opengribs.org/getmygribs.php?model=") +m_o[m_pModel->GetCurrentSelection()];
        r_topmess.Append( r_zone );
        r_topmess.Append(_T("&intv=") +m_pInterval->GetStringSelection());
        r_topmess.append(_T("&days=") +m_pTimeRange->GetStringSelection());
        // no wave
        r_topmess.Append(_T("&cyc=last&wmdl=none&wpar=&par="));
        break;

    case METEO_F:
        r_zone.Printf ( _T ("x=%d&X=%d&y=%d&Y=%d"),
           m_spMinLon->GetValue(), m_spMaxLon->GetValue(), m_spMinLat->GetValue(), m_spMaxLat->GetValue());
        r_topmess = _T("http://grib.opencpn.tk/grib/") +M[resolution];
        r_topmess.Append( r_zone  + _T("&r="));
        break;
    case NOAA:                                                                         //NOAA http download
        r_zone.Printf ( _T ( "subregion=&leftlon=%d&rightlon=%d&toplat=%d&bottomlat=%d" ), 
            m_spMinLon->GetValue(), m_spMaxLon->GetValue(), m_spMaxLat->GetValue(), m_spMinLat->GetValue()
            );
        r_topmess = _T("https://nomads.ncep.noaa.gov/cgi-bin/filter_") + m[model] ;
        if (model == GFS) {
            static const wxString r[] = { _T(".0p25"), _T("full.0p50"), _T(".1p00") };
            static const wxString f[] = { _T("0p25_1hr.pl"), _T("0p50.pl"), _T("1p00.pl") };

            r_topmess.Append( _T("_") + f[resolution] );
            r_topmess.Append(_T("?file=")  + m[model] + _T(".t%02dz.pgrb2") + r[resolution] + _T(".f%03d"));
            r_topmess.Append(_T("&dir=%%2F") + m[model] + _T(".%d%02d%02d%02d&"));
        }
        else if (model == GFS_ENS) {
            static const wxString f[] = { _T("_0p50.pl"), _T(".pl") };
            static const wxString h[] = { _T("geavg"), _T("gec00") };
            static const wxString r[] = { _T("a.0p50.f%03d"), _T("f%02d") };
            static const wxString t[] = { _T("ap5"), _T("") };

            r_topmess.Append( f[resolution] );
            r_topmess.Append(_T("?file=")  + h[resolution] + _T(".t%02dz.pgrb2") + r[resolution]);
            r_topmess.Append(_T("&dir=%%2Fgefs.%d%02d%02d%%2F%02d%%2Fpgrb2") + t[resolution] + _T("&"));
        }
        else if (model == NAM) {
            r_topmess.Append( _T("_crb.pl") );
            r_topmess.Append(_T("?file=")  + m[model] + _T(".t%02dz.afwaca%02d.tm00.grib2"));
            r_topmess.Append(_T("&dir=%%2F") + m[model] + _T(".%d%02d%02d&"));
        }
        else {
            bool hourly = m_pInterval->GetStringSelection().Length() == 1;
            r_topmess.Append(hourly?_T("_2d.pl") : _T("_sub.pl"));
            r_topmess.Append(_T("?file=")  + m[model] + _T(".t%02dz.wrf") + (hourly?_T("sfc"):_T("subh")));
            r_topmess.Append(_T("f%02d.grib2"));
            r_topmess.Append(_T("&dir=%%2F") + m[model] + _T(".%d%02d%02d%%2Fconus&"));
        }
        r_topmess.Append( r_zone  + _T("&"));
        break;
    case SAILDOCS:                                                                         //Saildocs
        r_zone = toMailFormat(1, m_spMaxLat->GetValue() ) + _T(",") + toMailFormat(1, m_spMinLat->GetValue() ) + _T(",")
            + toMailFormat(2, m_spMinLon->GetValue() ) + _T(",") + toMailFormat(2, m_spMaxLon->GetValue() );
        r_topmess = wxT("send ");
        r_topmess.Append(m_pModel->GetStringSelection() + _T(":"));
        r_topmess.Append( r_zone  + _T("|"));
        r_topmess.Append(m_pResolution->GetStringSelection()).Append(_T(","))
            .Append(m_pResolution->GetStringSelection()).Append(_T("|"));
        double v;
        m_pInterval->GetStringSelection().ToDouble(&v);
        r_topmess.Append(wxString::Format(_T("0,%d,%d"), (int) v, (int) v*2));
        m_pTimeRange->GetStringSelection().ToDouble(&v);
        r_topmess.Append(wxString::Format(_T("..%d"), (int) v*24) + _T("|=\n"));
        break;
	case ZYGRIB:                                                                         //Zygrib
		double maxlon = (m_spMinLon->GetValue() > m_spMaxLon->GetValue() && m_spMaxLon->GetValue() < 0)?
			m_spMaxLon->GetValue() + 360 : m_spMaxLon->GetValue();
		r_zone = toMailFormat(1, m_spMinLat->GetValue() ) + toMailFormat(2, m_spMinLon->GetValue() ) + _T(" ")
			+ toMailFormat(1, m_spMaxLat->GetValue() ) + toMailFormat(2, maxlon );
        r_topmess = wxT("login : ");
        r_topmess.Append(m_pLogin->GetValue() + _T("\n"));
        r_topmess.Append(wxT("code :"));
        r_topmess.Append(m_pCode->GetValue() + _T("\n"));
        r_topmess.Append(wxT("area : "));
        r_topmess.append(r_zone + _T("\n"));
        r_topmess.Append(wxT("resol : "));
        r_topmess.append(m_pResolution->GetStringSelection() + _T("\n"));
        r_topmess.Append(wxT("days : "));
        r_topmess.append(m_pTimeRange->GetStringSelection() + _T("\n"));
        r_topmess.Append(wxT("hours : "));
        r_topmess.append(m_pInterval->GetStringSelection() + _T("\n"));
        if(m_pWaves->IsChecked()) {
            r_topmess.Append(wxT("waves : "));
            r_topmess.append(m_pWModel->GetStringSelection() + _T("\n"));
        }
        r_topmess.Append(wxT("meteo : "));
        r_topmess.append(m_pModel->GetStringSelection() + _T("\n"));
        if ( m_pLogin->GetValue().IsEmpty() || m_pCode->GetValue().IsEmpty() ) m_MailError_Nb = 6;
        break;
    }
    //write the parameters part of the mail
    bool notfirst = false;
    bool ea = false;
    if ( m_pWind->IsChecked()) {
        r_parameters = p[provider][11];
        notfirst = true;
    }
    if ( m_pPress->IsChecked()) {
        if (notfirst) r_parameters.Append( s[provider]);
        // HRRR is PRES surface not sealevel
        if (d.Has(MSLET)) {
            r_parameters.Append( p[provider][17] );
        }
        else {
            r_parameters.Append( p[provider][(model == HRRR)?13:12] );
        }
        notfirst = true;
    }
    if ( m_pRainfall->IsChecked()) {
        if (!(provider == NOAA && model == HRRR && m_pInterval->GetStringSelection().Length() != 1)) {
            // NOAA HRRR sub hourly has TPRATE (aka RAIN) but I don't understand it
            if (notfirst) r_parameters.Append( s[provider]);
            r_parameters.Append( p[provider][d.Has(RAIN)?0:16]);
            notfirst = true;
        }
    }
    if ( m_pCloudCover->IsChecked()) {
        if (!(provider == NOAA && model == HRRR && m_pInterval->GetStringSelection().Length() != 1)) {
            // not available in NOAA HRRR sub hourly
            if (notfirst) r_parameters.Append( s[provider]);
            if (provider == NOAA) {
                ea = true;
                r_parameters.Append((model != NAM)?_T("lev_entire_atmosphere=on&")
                        :_T("lev_entire_atmosphere_%%5C%%28considered_as_a_single_layer%%5C%%29=on&")
                    );
            }
            r_parameters.Append( p[provider][1]);
            notfirst = true;
        }
    }
    if ( m_pAirTemp->IsChecked()) {
        if (notfirst) r_parameters.Append( s[provider]);
        r_parameters.Append( p[provider][2]);
        notfirst = true;
    }
    if ( m_pWaves->IsChecked() ) {
        if (notfirst) r_parameters.Append( s[provider]);
        r_parameters.Append( p[provider][3]);
        notfirst = true;
    }
    if ( m_pSeaTemp->IsChecked()) {
        if (notfirst) r_parameters.Append( s[provider]);
        r_parameters.Append( p[provider][4]);
        notfirst = true;
    }
    if ( m_pWindGust->IsChecked()) {
        if (notfirst) r_parameters.Append( s[provider]);
        r_parameters.Append( p[provider][5]);
        notfirst = true;
    }
    if ( m_pCAPE->IsChecked()) {
        if (!(provider == NOAA && model == HRRR && m_pInterval->GetStringSelection().Length() != 1)) {
            if (notfirst) r_parameters.Append( s[provider]);
            r_parameters.Append( p[provider][6]);
            notfirst = true;
        }
    }
    if ( m_pReflectivity->IsChecked()) {
        if (notfirst) r_parameters.Append( s[provider]);
        if (provider == NOAA && !ea) {
            r_parameters.Append((model != NAM)?_T("lev_entire_atmosphere=on&")
                :_T("lev_entire_atmosphere_%%5C%%28considered_as_a_single_layer%%5C%%29=on&")
            );
        }
        r_parameters.Append( p[provider][14]);
        notfirst = true;
    }
    if ( m_pCurrent->IsChecked()) {
        if (notfirst) r_parameters.Append( s[provider]);
        r_parameters.Append( p[provider][15]);
        notfirst = true;
    }

    if( m_pAltitudeData->IsChecked() ){
        if( m_p850hpa->IsChecked() )
            r_parameters.Append( s[provider] + p[provider][7] );
        if( m_p700hpa->IsChecked() )
            r_parameters.Append( s[provider] + p[provider][8] );
        if( m_p500hpa->IsChecked() )
            r_parameters.Append( s[provider] + p[provider][9] );
        if( m_p300hpa->IsChecked() )
            r_parameters.Append( s[provider] + p[provider][10] );
    }

    if( provider == SAILDOCS && m_cMovingGribEnabled->IsChecked())            //moving grib
        r_parameters.Append(wxString::Format(_T("|%d,%d"),m_sMovingSpeed->GetValue(),m_sMovingCourse->GetValue()));

    // line lenth limitation
    int j = 0;
    char c =  provider == SAILDOCS ? ',' : ' ';
    for( size_t i = 0; i < r_parameters.Len(); i++ ) {
        if(r_parameters.GetChar( i ) == '|' ) j--;                        //do not split Saildocs "moving" values
        if(r_parameters.GetChar( i ) == c ) j++;
        if( j > 6 ) {                                                       //no more than 6 parameters on the same line
            r_parameters.insert( i + 1 , provider == SAILDOCS ? _T("=\n") : _T("\n"));
            break;
        }
    }

    double size;
    m_MailError_Nb += EstimateFileSize( &size );

    m_tFileSize->SetLabel(wxString::Format( _T("%1.2f " ) , size ) + _("MB") );

    if( limit > 0 ) {
        m_tLimit->SetLabel(wxString( _T("( ") ) + _("Max") + wxString::Format(_T(" %d "), limit) + _("MB") + _T(" )") );
        if(size > limit) m_MailError_Nb += 2;
    } else
        m_tLimit->SetLabel(wxEmptyString);

    return wxString( r_topmess + r_parameters );
}

int GribRequestSetting::EstimateFileSize( double *size )
{
    if (!size)
        return 0; // Wrong parameter
    *size = 0.;

    //too small zone ? ( mini 2 * resolutions )
    double reso,time;
    m_pResolution->GetStringSelection().ToDouble(&reso);
    m_pTimeRange->GetStringSelection().ToDouble(&time);

    double maxlon = m_spMaxLon->GetValue(), minlon = m_spMinLon->GetValue();
    double maxlat = m_spMaxLat->GetValue(), minlat = m_spMinLat->GetValue();
    if( maxlat - minlat < 0 )
        return 3;                               // maxlat must be > minlat
    double wlon = (maxlon > minlon ? 0 : 360) + maxlon - minlon;
    if (wlon > 180 || ( maxlat - minlat > 180 ))
        return 4;                           //ovoid too big area

    if ( fabs(wlon) < 2*reso || maxlat - minlat < 2*reso  )
        return 5;                           //ovoid too small area

    int npts = (int) (  ceil(((double)(maxlat - minlat )/reso))
                      * ceil(((double)(wlon )/reso)) );

    int model = m_RevertMapModel[m_pModel->GetCurrentSelection()];
    int provider = m_pMailTo->GetCurrentSelection();
    if( model == COAMPS )                                           //limited area for COAMPS
        npts = wxMin(npts, (int) (  ceil(40.0/reso) * ceil(40.0/reso) ) );

    // Nombre de GribRecords
    double inter;
    m_pInterval->GetStringSelection().ToDouble(&inter);
    // XXX Hack for 1/4
    if( model == HRRR && m_pInterval->GetStringSelection().Length() != 1)
        inter /= 4.;

    int nbrec = (int) (time*24/inter)+1;
    int nbPress = (m_pPress->IsChecked()) ?  nbrec   : 0;
    int nbWind  = (m_pWind->IsChecked()) ?  2*nbrec : 0;
    int nbwave  = (m_pWaves->IsChecked()) ?  2*nbrec : 0;
    int nbRain  = (m_pRainfall->IsChecked()) ?  nbrec-1 : 0;
    int nbCloud = (m_pCloudCover->IsChecked()) ?  nbrec-1 : 0;
    int nbTemp  = (m_pAirTemp->IsChecked())    ?  nbrec   : 0;
    int nbSTemp  = (m_pSeaTemp->IsChecked())    ?  nbrec   : 0;
    int nbGUSTsfc  = (m_pWindGust->IsChecked()) ?  nbrec : 0;
    int nbCurrent  = (m_pCurrent->IsChecked()) ?  nbrec : 0;
    int nbCape  = (m_pCAPE->IsChecked()) ?  nbrec : 0;
    int nbAltitude  = IsZYGRIB ? 5 * nbrec : 3 * nbrec;             //five data types are included in each ZyGrib altitude request and only
                                                                    // three in sSaildocs's
    int head = 84;
    double estime = 0.0;
    double nbits;

    nbits = 13;
    estime += nbWind*(head+(nbits*npts)/8+2 );
    estime += nbCurrent*(head+(nbits*npts)/8+2 );

    nbits = 11;
    estime += nbTemp*(head+(nbits*npts)/8+2 );
    estime += nbSTemp*(head+(nbits*npts)/8+2 );

    nbits = 4;
    estime += nbRain*(head+(nbits*npts)/8+2 );

    nbits = 15;
    estime += nbPress*(head+(nbits*npts)/8+2 );

    nbits = 4;
    estime += nbCloud*(head+(nbits*npts)/8+2 );

    nbits = 7;
    estime += nbGUSTsfc*(head+(nbits*npts)/8+2 );

    nbits = 5;
    estime += nbCape*(head+(nbits*npts)/8+2 );

    nbits = 6;
    estime += nbwave*(head+(nbits*npts)/8+2 );

    if( m_pAltitudeData->IsChecked() ) {
        int nbalt = 0;
        if( m_p850hpa->IsChecked() ) nbalt ++;
        if( m_p700hpa->IsChecked() ) nbalt ++;
        if( m_p500hpa->IsChecked() ) nbalt ++;
        if( m_p300hpa->IsChecked() ) nbalt ++;

        nbits = 12;
        estime += nbAltitude*nbalt*(head+(nbits*npts)/8+2 );
    }

    *size = estime / (1024.*1024.);

    return 0;
}

static wxString FormatBytes(double bytes)
{
    return wxString::Format( _T("%.1fMB"), bytes / 1024 / 1024 );
}

void GribRequestSetting::onDLEvent(OCPN_downloadEvent &ev)
{
//    wxString msg;
//    msg.Printf(_T("onDLEvent  %d %d"),ev.getDLEventCondition(), ev.getDLEventStatus()); 
//    wxLogMessage(msg);
    
    switch(ev.getDLEventCondition()){
        case OCPN_DL_EVENT_TYPE_END:
            m_bTransferComplete = true;
            m_bTransferSuccess = (ev.getDLEventStatus() == OCPN_DL_NO_ERROR) ? true : false;
            break;
            
        case OCPN_DL_EVENT_TYPE_PROGRESS:
            m_totalsize = FormatBytes( ev.getTotal() );
            if (ev.getTransferred() != 0)
                m_transferredsize = ev.getTransferred();
            break;

        default:
            break;
    }
}

void GribRequestSetting::OnSendMaiL( wxCommandEvent& event  )
{
    StopGraphicalZoneSelection();                    //eventually stop graphical zone display

    if(!m_AllowSend) {
        m_rButtonCancel->Show();
        m_rButtonApply->Show();
        m_rButtonYes->SetLabel(_("Send"));

        m_MailImage->SetForegroundColour(wxColor( 0, 0, 0 ));                   //permit to send a (new) message
        m_AllowSend = true;

        m_MailImage->SetValue( WriteMail() +"\n\n");
        SetRequestDialogSize();

        return;
    }

    const wxString error[] = { _T("\n\n"), _("Before sending an email to Zygrib you have to enter your Login and Code.\nPlease visit www.zygrib.org/ and follow instructions..."),
        _("Too big file! limit is 2MB!"), _("Error! Max Lat lower than Min Lat or Max Lon lower than Min Lon!"),
        _("Too large area! Each side must be less than 180\u00B0!"), _("Too small area for this resolution!") };

    ::wxBeginBusyCursor();

    m_MailImage->SetForegroundColour(wxColor( 255, 0, 0 ));
    m_AllowSend = false;

    if( m_MailError_Nb ) {
        if( m_MailError_Nb > 7 ) {
            m_MailImage->SetValue( error[1] + error[0] + error[m_MailError_Nb - 6] );
        } else {
            if( m_MailError_Nb == 6 ) m_MailError_Nb = 1;
            m_MailImage->SetValue( error[m_MailError_Nb] );
        }

        m_rButtonCancel->Hide();
        m_rButtonApply->Hide();
        m_rButtonYes->SetLabel(_("Continue..."));
        m_rButton->Layout();
        SetRequestDialogSize();

        ::wxEndBusyCursor();

        return;
    }

    if (m_pMailTo->GetCurrentSelection() == NOAA || m_pMailTo->GetCurrentSelection() == METEO_F 
        || m_pMailTo->GetCurrentSelection() == OPENGRIBS)
    {
        if(!m_bconnected){
            Connect(wxEVT_DOWNLOAD_EVENT, (wxObjectEventFunction)(wxEventFunction)&GribRequestSetting::onDLEvent);
            m_bconnected = true;
        }

        wxString q(WriteMail());
        wxDateTime start = wxDateTime::Now().ToGMT();
        int req;
        if (m_RevertMapModel[m_pModel->GetCurrentSelection()] == HRRR) {
            req = start.Subtract(wxTimeSpan( 1, 30 )).GetHour();
        }
        else {
            // available 5 hours after at 00z 06z 12z and 18z
            req = (start.Subtract(wxTimeSpan( 5, 0 )).GetHour()/6)*6;
        }

        double v;
        int it;
        m_pInterval->GetStringSelection().ToDouble(&v);
        it = (int)v;
        bool one = false;
        int d = start.GetHour() - req;
        // round and if not crossing day start at n-1
        if (it > 1) d = (d/it)*it;
        if (d > it) d -= it;

        int cnt;
        m_pTimeRange->GetStringSelection().ToDouble(&v);
        cnt = (int)v *24;
        unsigned int to_download = (unsigned int)cnt/it;

        m_transferredsize = 0.;
        wxFileName downloaded_p;
        int idx = -1;
        m_downloading = 0;
        wxString output_path = wxFileName::CreateTempFileName( _T("O-") +
                m_pModel->GetStringSelection() + _T("-") ) ;
        unlink(output_path);
        output_path += _T(".grb");
        Model model = m_RevertMapModel[m_pModel->GetCurrentSelection()];

        wxFFileOutputStream *output = new wxFFileOutputStream(output_path );
        if (m_pMailTo->GetCurrentSelection() == METEO_F) {
            it = 1;
            cnt = 1;
            to_download = 1;
        }
        else if (m_pMailTo->GetCurrentSelection() == OPENGRIBS) {
            it = 1;
            cnt = 2;
            to_download = 2;
        }
        if ( model == HRRR) {
            cnt = 19;
            to_download = 19;
        }
        // =================== loop
        if (output->IsOk()) while (it > 0 && cnt > 0) {
            m_bTransferComplete  = false;
            m_bTransferSuccess = true;
            m_cancelled = false;
            wxString  buf;
            buf.Printf(q, req, d, start.GetYear(), start.GetMonth() +1, start.GetDay() , req);
            printf("%s\n\n",(const char*)buf.mb_str());

            wxURI url(buf);
            long handle;
            wxString file_path = wxFileName::CreateTempFileName( _T("") );
            unlink(file_path);
            file_path += _T(".grb");
            m_downloading++;
            if (OCPN_downloadFileBackground( url.BuildURI(), file_path, this, &handle) == OCPN_DL_FAILED) {
                printf("failed...\n");            
                break;
            }
            if (idx >= 0) {
                {
                    wxFFileInputStream i(downloaded_p.GetFullPath());
                    if (i.IsOk()) output->Write(i);
                }

                one = true;
                unlink(downloaded_p.GetFullPath());
                idx = -1;
            }
            while( !m_bTransferComplete && m_bTransferSuccess  && !m_cancelled ) {
                /*m_stCatalogInfo->SetLabel*/m_MailImage->SetValue( wxString::Format( _("Downloading grib %u of %u, (%s)"),
                                                         m_downloading, to_download, 
                                                         FormatBytes(m_transferredsize).c_str()/*, m_totalsize.c_str()*/ ) +"\n\n");
                wxYield();
                wxMilliSleep(30);
            }
        
            if( m_bTransferSuccess && !m_cancelled ) {
                downloaded_p = file_path;
                if (m_pMailTo->GetCurrentSelection() == OPENGRIBS && cnt > 1) {
                   bool ok = false;
                   wxFFileInputStream jsonText(downloaded_p.GetFullPath());
                   if ( jsonText.IsOk()) {
                       wxJSONValue  value;
                       wxJSONReader reader;
                       int numErrors = reader.Parse( jsonText, &value );
                       // {"status":true,"message":{"url":"..","size":9574626,"sha1":"cce61612a865fbbd17df46db0b7298672710ce25"}}
                       if ( numErrors == 0 && value.HasMember( "status" ) && value["status"].AsBool())  {
                           wxString u;
                           if (value["message"]["url"].AsString(u)) {
                               q = u;
                               ok = true;
                           }
                       }
                   }
                   if (!ok) {
                       idx = -1;
                       break;
                   }
                }
                else {
                    idx = 1;
                }
            } 
            else {
                idx = -1;
                m_MailImage->SetValue( wxString::Format( _("Error or cancel after %u downloads"), m_downloading));
                OCPN_cancelDownloadFileBackground( handle );
                if( wxFileExists( file_path ) ) wxRemoveFile( file_path );
                if (!one && model == HRRR && req > 0) {
                    printf("Maybe not uploaded yet, try %02d run\n", req);
                    // too greedy? retry with previous run
                    req--;
                    model = GFS;
                    continue;
                }
                break;
            }
            switch (model) {
            case GFS_ENS:
                if (d == 192) it = wxMax(it, 6);
                break;
            case GFS:
                if (d == 120) it = wxMax(it, 3);
                else if (d == 240) it = wxMax(it, 12);
                break;
            default:
                break;
            }
            d += it;
            cnt -= it;
        }
        // ==== done
        if (idx >= 0) {
            {
                wxFFileInputStream i(downloaded_p.GetFullPath());
                if (i.IsOk()) output->Write(i);
            }
            one = true;
            unlink(downloaded_p.GetFullPath());
        }
        // close 
        delete output;
        m_MailImage->SetForegroundColour(wxColor( 0, 0, 0 ));
        if (one) {
            m_MailImage->SetValue( wxString::Format( _("Ok downloading %u gribs"), m_downloading));
            m_parent.OpenFile(output_path);
            m_parent.SetDialogsStyleSizePosition( true );
        }

        m_AllowSend = true;

        ::wxEndBusyCursor();
        return;
    }

    wxMailMessage *message = new wxMailMessage(
    wxT("gribauto"),                                                                            //requested subject
    (m_pMailTo->GetCurrentSelection() == SAILDOCS) ? m_MailToAddresses.BeforeFirst(_T(';'))     //to request address
        : m_MailToAddresses.AfterFirst(_T(';')),
    WriteMail(),                                                                                 //message image
    m_pSenderAddress->GetValue()
    );
    wxEmail mail ;
    if(mail.Send( *message, m_SendMethod)) {
#ifdef __WXMSW__
        m_MailImage->SetValue(
            _("Your request is ready. An email is prepared in your email environment. \nYou have just to verify and send it...\nSave or Cancel to finish...or Continue...") );
#else
        if(m_SendMethod == 0 ) {
            m_MailImage->SetValue(
            _("Your request is ready. An email is prepared in your email environment. \nYou have just to verify and send it...\nSave or Cancel to finish...or Continue...") );
        } else {
        m_MailImage->SetValue(
            _("Your request was sent \n(if your system has an MTA configured and is able to send email).\nSave or Cancel to finish...or Continue...") );
        }
#endif
    } else {
        m_MailImage->SetValue(
            _("Request can't be sent. Please verify your email systeme parameters.\nYou should also have a look at your log file.\nSave or Cancel to finish..."));
        m_rButtonYes->Hide();
    }
    m_rButtonYes->SetLabel(_("Continue..."));
    m_rButton->Layout();
    SetRequestDialogSize();
    delete message;
    ::wxEndBusyCursor();
}

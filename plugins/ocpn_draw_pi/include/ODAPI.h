/******************************************************************************
 * $Id: ODAPI.h,v 1.0 2016/06/17 01:54:37 jongough Exp $
 *
 * Project:  OpenCPN
 * Purpose:  OpenCPN General Drawing Plugin API
 * Author:   Jon Gough
 *
 ***************************************************************************
 *   Copyright (C) 2010 by David S. Register                               *
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
 **************************************************************************/

#ifndef _ODAPI_H_
#define _ODAPI_H_

#ifndef DECL_EXP
#ifdef __WXMSW__
#  define DECL_EXP     __declspec(dllexport)
#else
# ifdef __GNUC__
# define DECL_EXP       __attribute__((visibility("default")))
# endif
#endif
#endif

#include    <list>

enum {
    EXCLUSION_BOUNDARY = 0,
    INCLUSION_BOUNDARY,
    NEITHER_BOUNDARY
};

enum {
    TEXTPOINT_DISPLAY_TEXT_SHOW_ALWAYS = 0,
    TEXTPOINT_DISPLAY_TEXT_SHOW_ON_ROLLOVER,
    TEXTPOINT_DISPLAY_TEXT_SHOW_NEVER
};

enum {
    TEXT_TOP = 0,
    TEXT_CENTRE_TOP,
    TEXT_BOTTOM,
    TEXT_CENTRE_BOTTOM,
    TEXT_CENTRE,
    TEXT_RIGHT,
    TEXT_LEFT,
    
    TEXT_POSTION_LAST
};

struct FindPointInAnyBoundary_t {
    double  dLat;
    double  dLon;
    wxString    sBoundaryType;
    wxString    sBoundaryState;
    wxString    sName;
    wxString    sDescription;
    wxString    sGUID;
    wxString    sBoundaryObjectType;
};

struct FindClosestBoundaryLineCrossing_t {
    double      dStartLat;
    double      dStartLon;
    double      dEndLat;
    double      dEndLon;
    double      dCrossingLat;
    double      dCrossingLon;
    double      dCrossingDistance;
    wxString    sBoundaryType;
    wxString    sBoundaryState;
    wxString    sName;
    wxString    sDescription;
    wxString    sGUID;
    wxString    sBoundaryObjectType;
};

struct CreateBoundaryPoint_t {
    wxString    name;
    wxString    iconname;
    double      lat;
    double      lon;
    int         type;
    bool        visible;
    bool        ringsvisible;
    int         ringsnumber;
    double      ringssteps;
    int         ringsunits;
    bool        defaultRingColour;
    wxColour    ringscolour;
    wxString    GUID;
};

struct CreateBoundary_t {
    wxString    name;
    int         type;
    bool        pathIsActive;
    bool        visible;
    bool        defaultLineColour;
    wxColour    lineColour;
    bool        defaultFillColour;
    wxColour    fillColour;
    wxString    GUID;
    std::list   <CreateBoundaryPoint_t *> BoundaryPointsList;
};

struct CreateTextPoint_t {
    wxString    name;
    wxString    iconname;
    double      lat;
    double      lon;
    wxString    description;
    wxString    TextToDisplay;
    bool        Visible;
    int         TextPosition;
    wxColour    TextColour;
    wxColour    BackgroundColour;
    int         BackgroundTransparancy;
    wxFont      TextFont;
    int         TextPointDisplayTextWhen;
    bool        ringsvisible;
    int         ringsnumber;
    double      ringssteps;
    int         ringsunits;
    bool        defaultRingColour;
    wxColour    ringscolour;
    wxString    GUID;
};

class ODAPI
{
public:
    ODAPI();
    ~ODAPI();
    static bool OD_FindPathByGUID( wxString sGUID, wxString *sName, wxString *sDescription );
    static bool OD_FindPointInAnyBoundary( FindPointInAnyBoundary_t *pFPIAB );
    static bool OD_FindClosestBoundaryLineCrossing( FindClosestBoundaryLineCrossing_t *pFCBLC );
    static bool OD_FindFirstBoundaryLineCrossing( FindClosestBoundaryLineCrossing_t *pFCBLC );
    static bool OD_CreateBoundary( CreateBoundary_t *pCB );
    static bool OD_CreateBoundaryPoint( CreateBoundaryPoint_t *pCBP );
    static bool OD_CreateTextPoint( CreateTextPoint_t *pCTP );
    
protected:
    
private:
    
};

typedef bool (*OD_FindPathByGUID) (wxString, wxString *, wxString *);
typedef bool (*OD_FindPointInAnyBoundary) (FindPointInAnyBoundary_t *);
typedef bool (*OD_FindClosestBoundaryLineCrossing) (FindClosestBoundaryLineCrossing_t *);
typedef bool (*OD_FindFirstBoundaryLineCrossing) (FindClosestBoundaryLineCrossing_t *);
typedef bool (*OD_CreateBoundary) (CreateBoundary_t *);
typedef bool (*OD_CreateBoundaryPoint) (CreateBoundaryPoint_t *);
typedef bool (*OD_CreateTextPoint) (CreateTextPoint_t *);


#endif //_ODAPI_H_

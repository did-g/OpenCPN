/**************************************************************************
*
*
***************************************************************************
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
#ifndef __CHARTOBJ_H__
#define __CHARTOBJ_H__

#ifdef USE_S57

#include "chartdb.h"
#include "s57chart.h"
#include "Quilt.h"

class ChartObj 
{
public:
  ChartObj();
  ~ChartObj();
  ListOfS57ObjRegion *GetHazards(ViewPort &vp);
  ListOfS57ObjRegion *GetSafeWaterAreas(ViewPort &vp);

private:
  int GetExtendedStackCount( ) 
  {
        return m_extended_stack_array.GetCount();
  }

  void EmptyCandidateArray( );
  bool BuildExtendedChartStackAndCandidateArray();
  ChartStack *m_stack;

  ViewPort  m_vp;

  PatchList m_PatchList;
  ArrayOfSortedQuiltCandidates *m_pcandidate_array;
  ArrayOfInts m_extended_stack_array;
  LLRegion m_covered_region;
  
};

#endif

#endif

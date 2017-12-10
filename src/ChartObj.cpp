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

#include "ChartObj.h"
#include "Quilt.h"

extern ChartDB *ChartData;
extern int g_GroupIndex;
extern ArrayOfInts g_quilt_noshow_index_array;

static int ExactCompareScales( int *i1, int *i2 )
{
    if( !ChartData ) return 0;

    const ChartTableEntry &cte1 = ChartData->GetChartTableEntry( *i1 );
    const ChartTableEntry &cte2 = ChartData->GetChartTableEntry( *i2 );

    if(  cte1.GetScale() == cte2.GetScale() ) {  // same scales, so sort on dbIndex
        float lat1, lat2;
        lat1 = cte1.GetLatMax();
        lat2 = cte2.GetLatMax();
        if (roundf(lat1*100.) == roundf(lat2*100.)) {
            float lon1, lon2;
            lon1 = cte1.GetLonMin();
            lon2 = cte2.GetLonMin();
            if (lon1 == lon2) {
                return *i1 -*i2;
            }
            else 
                return (lon1 < lon2)?-1:1;
        }
        else 
            return (lat1 < lat2)?1:-1;
    }
    else
        return cte1.GetScale() - cte2.GetScale();
}


static int CompareCandidateScales( QuiltCandidate *qc1, QuiltCandidate *qc2 )
{
    if( !ChartData ) return 0;
    return ExactCompareScales(&qc1->dbIndex, &qc2->dbIndex);
}

ChartObj::ChartObj() : m_stack(0) 
{
  m_pcandidate_array = new ArrayOfSortedQuiltCandidates( CompareCandidateScales );
}


ChartObj::~ChartObj()
{
  delete m_stack;
  m_PatchList.DeleteContents( true );
  m_PatchList.Clear();

  EmptyCandidateArray();
  delete m_pcandidate_array;

  m_extended_stack_array.Clear();
}

void ChartObj::EmptyCandidateArray( )
{
    for( unsigned int i = 0; i < m_pcandidate_array->GetCount(); i++ ) {
        delete m_pcandidate_array->Item( i );
    }

    m_pcandidate_array->Clear();

}

bool ChartObj::BuildExtendedChartStackAndCandidateArray()
{
    EmptyCandidateArray();
    m_extended_stack_array.Clear();

    int reference_scale = 1;
    const int reference_type = CHART_TYPE_S57;
    const int reference_family = CHART_FAMILY_VECTOR;
    const int quilt_proj = m_vp.m_projection_type;

    bool b_need_resort = false;

    ViewPort vp_local = m_vp;

    if( !m_stack ) {
        m_stack = new ChartStack;
        ChartData->BuildChartStack( m_stack, vp_local.clat, vp_local.clon );
    }

    int n_charts = m_stack->nEntry;

    //    Walk the current ChartStack...
    //    Building the quilt candidate array
    for( int ics = 0; ics < n_charts; ics++ ) {
        int i = m_stack->GetDBIndex( ics );
        if (i < 0)
            continue;
        m_extended_stack_array.Add( i );

        //  If the reference chart is cm93, we need not add any charts to the candidate array from the vp center.
        //  All required charts will be added (by family) as we consider the entire database (group) and full screen later
        if(reference_type == CHART_TYPE_CM93COMP)
               continue;
            
        const ChartTableEntry &cte = ChartData->GetChartTableEntry( i );

        // only charts of the proper projection and type may be quilted....
        // Also, only unskewed charts if so directed
        // and we avoid adding CM93 Composite until later
         
        // If any PlugIn charts are involved, we make the inclusion test on chart family, instead of chart type.
        if( (cte.GetChartType() == CHART_TYPE_PLUGIN ) || (reference_type == CHART_TYPE_PLUGIN )){
            if( reference_family != cte.GetChartFamily() ){
                continue;
            }
        }
        else{
            if( reference_type != cte.GetChartType() ){
                continue;
            }
        }
        
        if( cte.GetChartType() == CHART_TYPE_CM93COMP ) continue;

        double skew_norm = cte.GetChartSkew();
        if( skew_norm > 180. ) skew_norm -= 360.;
            
        if( cte.GetChartProjectionType() == quilt_proj ) {
            QuiltCandidate *qcnew = new QuiltCandidate;
            qcnew->dbIndex = i;
            qcnew->SetScale (cte.GetScale() );
            m_pcandidate_array->Add( qcnew );               // auto-sorted on scale
        }
    }
    //    Search the entire database, potentially adding all charts
    //    which intersect the ViewPort in any way
    //    .AND. other requirements.
    //    Again, skipping cm93 for now
    int n_all_charts = ChartData->GetChartTableEntries();

    LLBBox viewbox = vp_local.GetBBox();
    int sure_index = -1;
    int sure_index_scale = 0;

    for( int i = 0; i < n_all_charts; i++ ) {
      //    We can eliminate some charts immediately
      //    Try to make these tests in some sensible order....

      if( ( g_GroupIndex > 0 ) && ( !ChartData->IsChartInGroup( i, g_GroupIndex ) ) ) continue;
            
      const ChartTableEntry &cte = ChartData->GetChartTableEntry( i );

      if( cte.GetChartType() == CHART_TYPE_CM93COMP ) continue;

      if( reference_family != cte.GetChartFamily() )
          continue;
            
      const LLBBox &chart_box = cte.GetBBox();
      if( ( viewbox.IntersectOut( chart_box ) ) ) continue;

      const int candidate_chart_scale = cte.GetScale();

      bool b_add = true;

      LLRegion cell_region = Quilt::GetChartQuiltRegion( cte, vp_local );

      // this is false if the chart has no actual overlap on screen
      // or lots of NoCovr regions.  US3EC04.000 is a good example
      // i.e the full bboxes overlap, but the actual vp intersect is null.
      if( ! cell_region.Empty() ) {
        // Check to see if this chart is already in the stack array
        // by virtue of being under the Viewport center point....
        bool b_exists = false;
        for( unsigned int ir = 0; ir < m_extended_stack_array.GetCount(); ir++ ) {
          if( i == m_extended_stack_array.Item( ir ) ) {
            b_exists = true;
            break;
          }
        }

        if( !b_exists ) {
           //      Check to be sure that this chart has not already been added
           //    i.e. charts that have exactly the same file name and nearly the same mod time
           //    These charts can be in the database due to having the exact same chart in different directories,
           //    as may be desired for some grouping schemes
           bool b_noadd = false;
           ChartTableEntry *pn = ChartData->GetpChartTableEntry( i );
           for ( unsigned int id = 0; id < m_extended_stack_array.GetCount() ; id++ ) 
           {
              if ( m_extended_stack_array.Item( id ) != -1 ) {
                 ChartTableEntry *pm = ChartData->GetpChartTableEntry( m_extended_stack_array.Item( id ) );
                 if ( pm->GetFileTime() && pn->GetFileTime()) {
                    if ( labs(pm->GetFileTime() - pn->GetFileTime()) < 60 ) {           // simple test
                      if( pn->GetpFileName()->IsSameAs( *( pm->GetpFileName() ) ) )
                        b_noadd = true;
                    }
                 }
              }
           }

           if (!b_noadd) 
           {
              m_extended_stack_array.Add( i );

              QuiltCandidate *qcnew = new QuiltCandidate;
              qcnew->dbIndex = i;
              qcnew->SetScale ( candidate_chart_scale ); //ChartData->GetDBChartScale( i );

              m_pcandidate_array->Add( qcnew );               // auto-sorted on scale

              b_need_resort = true;
           }
        }
      }
    }               // for all charts

    // Re sort the extended stack array on scale
    if( b_need_resort && m_extended_stack_array.GetCount() > 1 ) {
        m_extended_stack_array.Sort(ExactCompareScales);
    }
    return true;
}

// -------------------
ListOfS57Obj *ChartObj::GetHazards(ViewPort &vp)
{
  if ( !ChartData )
    return 0;

  if(ChartData->IsBusy())             // This prevent recursion on chart loads that Yeild()
    return 0;

  m_vp = vp;
  delete m_stack;
  m_stack = new ChartStack;
  if (BuildExtendedChartStackAndCandidateArray() == false)
    return 0;

  bool b_has_overlays = false;
  //  If this is an S57 quilt, we need to know if there are overlays in it
  for( unsigned int ir = 0; ir < m_pcandidate_array->GetCount(); ir++ ) {
      QuiltCandidate *pqc = m_pcandidate_array->Item( ir );
      const ChartTableEntry &cte = ChartData->GetChartTableEntry( pqc->dbIndex );

      if (s57chart::IsCellOverlayType(cte.GetpFullPath() )){
         b_has_overlays = true;
         break;
      }
  }

  //    Using Region logic, and starting from the largest scale chart
  //    figuratively "draw" charts until the ViewPort window is completely quilted over
  //    Add only those charts whose scale is smaller than the "reference scale"
  const LLRegion cvp_region = m_vp.GetLLRegion(wxRect(0, 0, m_vp.pix_width, m_vp.pix_height));
  LLRegion vp_region = cvp_region;
  //    "Draw" the reference chart first, since it is special in that it controls the fine vpscale setting
  QuiltCandidate *pqc_ref = m_pcandidate_array->Item( 0 );
#if 0
  for( unsigned int ir = 0; ir < m_pcandidate_array->GetCount(); ir++ )       // find ref chart entry
  {
      QuiltCandidate *pqc = m_pcandidate_array->Item( ir );
      const ChartTableEntry &cte = ChartData->GetChartTableEntry( pqc->dbIndex );
      // printf("chart %s %d\n",cte.GetpFullPath(), cte.GetScale());
  }
#endif
  const ChartTableEntry &cte_ref = ChartData->GetChartTableEntry( pqc_ref->dbIndex );

  LLRegion vpu_region( cvp_region );

  //LLRegion chart_region = pqc_ref->GetCandidateRegion();
  LLRegion chart_region = pqc_ref->GetCandidateRegion();
        
  if ( !chart_region.Empty() ){
      vpu_region.Intersect( chart_region );

      if( vpu_region.Empty() )
                pqc_ref->b_include = false;   // skip this chart, no true overlap
      else {
                pqc_ref->b_include = true;
                vp_region.Subtract( chart_region );          // adding this chart
      }
  }
  else
      pqc_ref->b_include = false;   // skip this chart, empty region

   //    Now the rest of the candidates
   if( !vp_region.Empty() ) {
        for( unsigned int ir = 0; ir < m_pcandidate_array->GetCount(); ir++ ) {
            QuiltCandidate *pqc = m_pcandidate_array->Item( ir );

            const ChartTableEntry &cte = ChartData->GetChartTableEntry( pqc->dbIndex );

            //  Skip overlays on this pass, so that they do not subtract from quilt and thus displace
            //  a geographical cell with the same extents.
            //  Overlays will be picked up in the next pass, if any are found
            if(s57chart::IsCellOverlayType(cte.GetpFullPath() )){
                continue;
            }

            //  If this chart appears in the no-show array, then simply include it, but
            //  don't subtract its region when determining the smaller scale charts to include.....
            bool b_in_noshow = false;
            for( unsigned int ins = 0; ins < g_quilt_noshow_index_array.GetCount(); ins++ ) {
                    if( g_quilt_noshow_index_array.Item( ins ) == pqc->dbIndex ) // chart is in the noshow list
                    {
                        b_in_noshow = true;
                        break;
                    }
            }

            if( !b_in_noshow ) {
                //    Check intersection
                LLRegion vpu_region( cvp_region );

                //LLRegion chart_region = pqc->GetCandidateRegion( );  //quilt_region;
                LLRegion chart_region = pqc->GetCandidateRegion();
                    
                if( !chart_region.Empty() ) {
                      vpu_region.Intersect( chart_region );

                      if( vpu_region.Empty() )
                          pqc->b_include = false; // skip this chart, no true overlap
                      else {
                          pqc->b_include = true;
                          vp_region.Subtract( chart_region );          // adding this chart
                      }
                } else
                      pqc->b_include = false;   // skip this chart, empty region
            } else {
                pqc->b_include = true;
            }

            if( vp_region.Empty() )                   // normal stop condition, quilt is full
                break;
        }
    }
    //  For S57 quilts, walk the list again to identify overlay cells found previously,
    //  and make sure they are always included and not eclipsed
    if( b_has_overlays  ) {
        for( unsigned int ir = 0; ir < m_pcandidate_array->GetCount(); ir++ ) {
            QuiltCandidate *pqc = m_pcandidate_array->Item( ir );

            const ChartTableEntry &cte = ChartData->GetChartTableEntry( pqc->dbIndex );
            
            bool b_in_noshow = false;
            for( unsigned int ins = 0; ins < g_quilt_noshow_index_array.GetCount(); ins++ ) {
                    if( g_quilt_noshow_index_array.Item( ins ) == pqc->dbIndex ) // chart is in the noshow list
                    {
                        b_in_noshow = true;
                        break;
                    }
            }

            if( !b_in_noshow ) {
                    //    Check intersection
                    LLRegion vpu_region( cvp_region );

                    //LLRegion chart_region = pqc->GetCandidateRegion( );
                    LLRegion chart_region = pqc->GetCandidateRegion();
                    
                    if( !chart_region.Empty() )
                        vpu_region.Intersect( chart_region );

                    if( vpu_region.Empty() )
                        pqc->b_include = false; // skip this chart, no true overlap
                    else {
                        bool b_overlay = s57chart::IsCellOverlayType(cte.GetpFullPath() );
                        if( b_overlay )
                            pqc->b_include = true;
                    }
                    
                    //  If the reference chart happens to be an overlay (e.g. 3UABUOYS.000),
                    //  we dont want it to eclipse any smaller scale standard useage charts.
                    const ChartTableEntry &cte_ref = ChartData->GetChartTableEntry( pqc_ref->dbIndex );
                    if(s57chart::IsCellOverlayType(cte_ref.GetpFullPath() )){
                        pqc->b_include = true;
                    }
            }
        }
    }
    //    Finally, build a list of "patches" for the quilt.
    //    Smallest scale first, as this will be the natural drawing order

    m_PatchList.DeleteContents( true );
    m_PatchList.Clear();

    if( m_pcandidate_array->GetCount() ) {
        for( int i = m_pcandidate_array->GetCount() - 1; i >= 0; i-- ) {
            QuiltCandidate *pqc = m_pcandidate_array->Item( i );

            //    cm93 add has been deferred until here
            //    so that it would not displace possible raster or ENCs of larger scale
            const ChartTableEntry &m = ChartData->GetChartTableEntry( pqc->dbIndex );

            if( m.GetChartType() == CHART_TYPE_CM93COMP ) pqc->b_include = false; // force acceptance of this chart in quilt
            // would not be in candidate array if not elected

            if( pqc->b_include ) {
                QuiltPatch *pqp = new QuiltPatch;
                pqp->dbIndex = pqc->dbIndex;
                pqp->ProjType = m.GetChartProjectionType();
                // this is the region used for drawing, don't reduce it
                // it's visible
                pqp->quilt_region = pqc->GetCandidateRegion();
                //pqp->quilt_region = pqc->GetReducedCandidateRegion(factor);
                
                pqp->b_Valid = true;

                m_PatchList.Append( pqp );
            }
        }
    }

    //    Walk the PatchList, marking any entries which appear in the noshow array
    for( unsigned int i = 0; i < m_PatchList.GetCount(); i++ ) {
        wxPatchListNode *pcinode = m_PatchList.Item( i );
        QuiltPatch *piqp = pcinode->GetData();
        for( unsigned int ins = 0; ins < g_quilt_noshow_index_array.GetCount(); ins++ ) {
            if( g_quilt_noshow_index_array.Item( ins ) == piqp->dbIndex ) // chart is in the noshow list
            {
                piqp->b_Valid = false;
                break;
            }
        }
    }

    //    Generate the final render regions for the patches, one by one

    m_covered_region.Clear();
    
    //  If the reference chart is cm93, we need to render it first.
    bool b_skipCM93 = true;
    for( int i = m_PatchList.GetCount()-1; i >=0; i-- ) {
        wxPatchListNode *pcinode = m_PatchList.Item( i );
        QuiltPatch *piqp = pcinode->GetData();
        if( !piqp->b_Valid )                         // skip invalid entries
            continue;

        const ChartTableEntry &cte = ChartData->GetChartTableEntry( piqp->dbIndex );
        
        if(b_skipCM93){
            if(cte.GetChartType() == CHART_TYPE_CM93COMP)
                continue;
        }
            
        //    Start with the chart's full region coverage.
        piqp->ActiveRegion = piqp->quilt_region;
        
        // this operation becomes expensive with lots of charts
        if(!b_has_overlays && m_PatchList.GetCount() < 75)
            piqp->ActiveRegion.Subtract(m_covered_region);

        piqp->ActiveRegion.Intersect(cvp_region);

        //    Could happen that a larger scale chart covers completely a smaller scale chart
        if( piqp->ActiveRegion.Empty() && (piqp->dbIndex != pqc_ref->dbIndex))
            piqp->b_eclipsed = true;

        //    Maintain the present full quilt coverage region
        piqp->b_overlay = false;
        if(cte.GetChartFamily() == CHART_FAMILY_VECTOR){
            piqp->b_overlay = s57chart::IsCellOverlayType(cte.GetpFullPath());
        }
                
        if(!piqp->b_overlay)    
            m_covered_region.Union( piqp->quilt_region );
    }

    unsigned int il = 0;
    while( il < m_PatchList.GetCount() ) {
        wxPatchListNode *pcinode = m_PatchList.Item( il );
        QuiltPatch *piqp = pcinode->GetData();
        if( piqp->b_eclipsed ) {
            //    Make sure that this chart appears in the eclipsed list...
            //    This can happen when....
            m_PatchList.DeleteNode( pcinode );
            il = 0;           // restart the list walk
        }

        else
            il++;
    }

    ListOfS57Obj *pobj_list = new ListOfS57Obj;
    pobj_list->Clear();

    // XXX TODO void Quilt::ComputeRenderRegion( ViewPort &vp, OCPNRegion &chart_region )

    LLRegion r;
    ChartBase *pret = 0;
    
    for( unsigned int i = m_PatchList.GetCount(); i > 0; i-- ) {
        wxPatchListNode *pcinode = m_PatchList.Item( i -1);
        QuiltPatch *piqp = pcinode->GetData();
        if (!piqp->b_Valid)
            continue;
        const ChartTableEntry &cte = ChartData->GetChartTableEntry( piqp->dbIndex );
        LLRegion r = piqp->ActiveRegion;
        int c = pobj_list->GetCount();
        printf("%d chart %s %d \n",i, cte.GetpFullPath(), cte.GetScale());
        //if( ChartData->IsChartInCache( piqp->dbIndex ) ){
        pret = ChartData->OpenChartFromDB( piqp->dbIndex, FULL_INIT );
        s57chart *s57 = dynamic_cast<s57chart*>( pret );
        printf("\tsearching");	
        pobj_list = s57->GetHazards(r, pobj_list);
        printf(" find %d \n", pobj_list->GetCount() -c);
    }
    return pobj_list;
}

ListOfS57Obj *ChartObj::GetSafeWaterAreas(ViewPort &vp)
{
  return 0;
}

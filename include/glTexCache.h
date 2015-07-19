/******************************************************************************
 *
 * Project:  OpenCPN
 *
 ***************************************************************************
 *   Copyright (C) 2013 by David S. Register                               *
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
 */

#ifndef __GLTEXTCACHE_H__
#define __GLTEXTCACHE_H__

#include <wx/glcanvas.h>
#include <wx/ffile.h>
#include <wx/timer.h>
#include <stdint.h>

#include "ocpn_types.h"
#include "glTextureDescriptor.h"

#define COMPRESSED_CACHE_MAGIC 0xf010  // change this when the format changes

#define COMPRESSED_BUFFER_OK            0
#define COMPRESSED_BUFFER_PENDING       1
#define MAP_BUFFER_OK                   4

#define FACTORY_TIMER                   10000

void HalfScaleChartBits( int width, int height, unsigned char *source, unsigned char *target );
bool CompressUsingGPU( glTextureDescriptor *ptd, GLuint raster_format, int level, bool b_post_comp);

struct CompressedCacheHeader
{
    uint32_t magic;
    uint32_t format;
    uint32_t chartdate;
    uint32_t m_nentries;
    uint32_t catalog_offset;    
};

typedef struct _CatalogEntry_key
{
    int         mip_level;
    ColorScheme tcolorscheme;
    int         x;
    int         y;
} CatalogEntryKey;

typedef struct _CatalogEntry_value
{
    int         texture_offset;
    uint32_t    compressed_size;
} CatalogEntryValue;

class CatalogEntry
{
public:
    CatalogEntry();
    ~CatalogEntry();
    CatalogEntry(int level, int x0, int y0, ColorScheme colorscheme);
    int GetSerialSize();
    void Serialize(unsigned char *);
    void DeSerialize(unsigned char *);
    CatalogEntryKey k;
    CatalogEntryValue v;
    
};

WX_DEFINE_ARRAY(CatalogEntry*, ArrayOfCatalogEntries);

class glTexFactory : public wxEvtHandler
{
public:
    glTexFactory(ChartBase *chart, GLuint raster_format);
    ~glTexFactory();

    bool PrepareTexture( int base_level, const wxRect &rect, ColorScheme color_scheme, bool b_throttle_thread = true );
    int GetTextureLevel( glTextureDescriptor *ptd, const wxRect &rect, int level,  ColorScheme color_scheme );
    bool UpdateCacheLevel( const wxRect &rect, int level, ColorScheme color_scheme, bool bCat = true );
    bool IsCompressedArrayComplete( int base_level, const wxRect &rect);
    bool IsCompressedArrayComplete( int base_level, glTextureDescriptor *ptd);
    bool IsLevelInCache( int level, const wxRect &rect, ColorScheme color_scheme );
    void DoImmediateFullCompress(const wxRect &rect);
    wxString GetChartPath(){ return m_ChartPath; }
    void DeleteTexture(const wxRect &rect);
    void DeleteAllTextures( void );
    void DeleteSomeTextures( long target );
    void DeleteSomeTextures( );
    void DeleteAllDescriptors( void );
    bool BackgroundCompressionAsJob() const;
    void PurgeBackgroundCompressionPool();
    void OnTimer(wxTimerEvent &event);
    void SetLRUTime(int lru) { m_LRUtime = lru; }
    int	 GetLRUTime() { return m_LRUtime; }
    void FreeSome( long target );
    
    glTextureDescriptor *GetpTD( wxRect & rect );
    GLuint GetRasterFormat() { return m_raster_format; }
    
    bool CompressUnCacheEntry();
    
private:
    bool LoadCatalog(void);
    bool LoadHeader(void);
    bool WriteCatalogAndHeader();
    
    bool UpdateCache(unsigned char *data, int data_size, glTextureDescriptor *ptd, int level,
                                   ColorScheme color_scheme, bool bCat = true);
    bool UpdateCachePrecomp(unsigned char *data, int data_size, glTextureDescriptor *ptd, int level,
                                          ColorScheme color_scheme, bool bCat = true);
    
    void DeleteSingleTexture( glTextureDescriptor *ptd );

    CatalogEntryValue *GetCacheEntryValue(int level, int x, int y, ColorScheme color_scheme);
    bool AddCacheEntryValue(const CatalogEntry &p);
    int  ArrayIndex(int x, int y) const { return ((y / m_tex_dim) * m_stride) + (x / m_tex_dim); } 
    void  ArrayXY(wxRect *r, int index) const;

    int         n_catalog_entries;
    //ArrayOfCatalogEntries       m_catalog;
    CatalogEntryValue *m_cache[N_COLOR_SCHEMES][5];
    wxString    m_ChartPath;
    GLuint      m_raster_format;
    wxString    m_CompressedCacheFilePath;
    
    int         m_catalog_offset;
    bool        m_hdrOK;
    bool        m_catalogOK;
    bool        m_catalogCorrupted;
    wxFFile     *m_fs;
    uint32_t    m_chart_date_binary;
    
    int         m_stride;
    int         m_ntex;
    int         m_tex_dim;
    int         m_size_X;
    int         m_size_Y;
    int         m_nx_tex;
    int         m_ny_tex;
    
    int		m_index;
    ColorScheme m_colorscheme;
    bool	m_pending;
    wxTimer     m_timer;
    size_t      m_ticks;
    int		m_LRUtime;
    
    glTextureDescriptor  **m_td_array;
    
    DECLARE_EVENT_TABLE()
    
};

const wxEventType wxEVT_OCPN_COMPRESSIONTHREAD = wxNewEventType();

class JobTicket;

WX_DECLARE_LIST(JobTicket, JobList);

class OCPN_CompressionThreadEvent: public wxEvent
{
public:
    OCPN_CompressionThreadEvent( wxEventType commandType = wxEVT_NULL, int id = 0 );
    ~OCPN_CompressionThreadEvent( );
    
    // accessors
    void SetTicket( JobTicket *ticket ){m_ticket = ticket;}
    JobTicket *GetTicket(void){ return m_ticket; }
    
    // required for sending with wxPostEvent()
    wxEvent *Clone() const;
    
private:
    JobTicket  * m_ticket;
};

//      CompressionWorkerPool Definition
class CompressionWorkerPool : public wxEvtHandler
{
public:
    CompressionWorkerPool();
    ~CompressionWorkerPool();
    
    bool ScheduleJob( glTexFactory *client, const wxRect &rect, int level_min,
                      bool b_throttle_thread, bool b_immediate, bool b_postZip);
    void OnEvtThread( OCPN_CompressionThreadEvent & event );
    int GetRunningJobCount(){ return m_njobs_running; };
    bool AsJob( wxString const &chart_path ) const;
    void PurgeJobList( wxString chart_path = wxEmptyString );
    
    unsigned int m_raster_format;

    int GetStartedJobCount(){ return m_jobs_started; };
    int GetAbortedJobCount(){ return m_jobs_aborted; };
    int GetRefusedJobCount(){ return m_jobs_refused; };
    int GetPurgedJobCount(){ return m_jobs_purged; };
    
private:
    
    bool DoJob( JobTicket *pticket );
    bool DoThreadJob(JobTicket* pticket);
    bool StartTopJob();
    
    JobList             running_list;
    JobList             todo_list;
    int                 m_njobs_running;
    int                 m_max_jobs;
    
    int		m_jobs_started;
    int		m_jobs_purged;
    int		m_jobs_aborted;
    int		m_jobs_refused;
    
};

#endif

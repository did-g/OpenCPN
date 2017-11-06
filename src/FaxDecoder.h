/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  weather fax Plugin
 * Author:   Sean D'Epagnier
 *
 ***************************************************************************
 *   Copyright (C) 2015 by Sean D'Epagnier                                 *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.         *
 ***************************************************************************
 */

#include "wx/wxprec.h"

#ifndef  WX_PRECOMP
  #include "wx/wx.h"
#endif //precompiled headers

#include <audiofile.h>

#ifdef OCPN_USE_PORTAUDIO
    #include <portaudio.h>
#endif

struct FaxDecoderCaptureSettings
{    
    FaxDecoderCaptureSettings() : type(NONE), offset(0) {}

    enum Type {NONE, AUDIO, FILE, RTLSDR} type;

    // if type == "audio"
    int audio_deviceindex;
    long audio_samplerate;
    
    // if type == "file"
    wxString filename;
    long offset;

    // if type == "rtlsdr"
    int rtlsdr_deviceindex;
    int rtlsdr_errorppm;
    int rtlsdr_upconverter_mhz;
};

class FaxDecoder
{
public:
    enum Header {IMAGE, START, STOP};

    struct firfilter {
        enum Bandwidth {NARROW, MIDDLE, WIDE};
        firfilter() {}
    firfilter(enum Bandwidth b) : bandwidth(b), current(0)
        { for(int i=0; i<17; i++) buffer[i] = 0; }
        enum Bandwidth bandwidth;
        double buffer[17];
        int current;
    };

    FaxDecoder(wxWindow &parent, FaxDecoderCaptureSettings &CaptureSettings)
        : m_imgdata(NULL), m_imagewidth(-1),
        datadouble(NULL), m_CaptureSettings(CaptureSettings),
        m_parent(parent), dsp(0), aFile(0),
#ifdef OCPN_USE_PORTAUDIO
        pa_stream(NULL),
#endif
        sample(NULL), data(NULL), phasingPos(NULL) {}
    ~FaxDecoder() { FreeImage(); CleanUpBuffers(); }

    bool Configure(int imagewidth, int BitsPerPixel, int carrier,
                   int deviation, enum firfilter::Bandwidth bandwidth,
                   double minus_saturation_threshold,
                   bool bSkipHeaderDetection, bool bIncludeHeadersInImages, bool reset);
    static int AudioDeviceCount();

    bool DecodeFaxFromFilename();
    bool DecodeFaxFromDSP();
    bool DecodeFaxFromPortAudio();
    bool DecodeFaxFromRTLSDR();

    void InitializeImage();
    void FreeImage();

    void CloseInput();

    void SetupBuffers();
    void CleanUpBuffers();

    bool DecodeFax(); /* thread main function */
    Header State(bool &phasing) { phasing = phasingLinesLeft>0; return lasttype; }

    bool m_bEndDecoding; /* flag to end decoding thread */
    AFframecount m_stop_audio_offset; // position in stream when stop sequence was found

    wxMutex m_DecoderMutex, m_DecoderStopMutex, m_DecoderReloadMutex;

    wxUint8 *m_imgdata;
    int m_imageline;
    int m_blocksize;
    int m_imagewidth;
    double m_minus_saturation_threshold;
    double *datadouble;

    int m_SampleRate;

    FaxDecoderCaptureSettings m_CaptureSettings;

private:
    wxWindow &m_parent;

    int m_SampleSize;

    int dsp;

    AFfilehandle aFile;
    AFfileoffset size;

#ifdef OCPN_USE_PORTAUDIO
    PaStream *pa_stream;
#endif

    bool Error(wxString error);
    double FourierTransformSub(wxUint8* buffer, int buffer_len, int freq);
    Header DetectLineType(wxUint8* buffer, int buffer_len);
    void DemodulateData(int n);
    void DecodeImageLine(wxUint8* buffer, int buffer_len, wxUint8 *image);
    int FaxPhasingLinePosition(wxUint8 *image, int imagewidth);

    /* fax settings */
    int m_BitsPerPixel;
    double m_carrier, m_deviation;
    struct firfilter firfilters[2];
    bool m_bSkipHeaderDetection;
    bool m_bIncludeHeadersInImages;
    int m_imagecolors, m_faxcolors;
    int m_lpm;
    bool m_bFM;
    int m_StartFrequency, m_StopFrequency;
    int m_StartLength, m_StopLength;
    int m_phasingLines;

    /* internal state machine */
    wxInt16 *sample;
    wxUint8 *data;
    
    int height, imgpos;

    Header lasttype;
    int typecount;

    bool gotstart;

    int *phasingPos;
    int phasingLinesLeft, phasingSkipData, phasingSkippedData;
};

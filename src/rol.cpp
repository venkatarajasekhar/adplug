/*
 * Adplug - Replayer for many OPL2/OPL3 audio file formats.
 * Copyright (C) 1999 - 2006 Simon Peter, <dn.tlp@gmx.net>, et al.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * rol.cpp - ROL Player by OPLx <oplx@yahoo.com>
 *
 * Source references ADLIB.C from Adlib MSC SDK.
 */
#include <cstring>
#include <algorithm>

#include "rol.h"
#include "debug.h"

#if !defined(UINT8_MAX)
    typedef signed char    int8_t;
    typedef short          int16_t;
    typedef int            int32_t;
    typedef unsigned char  uint8_t;
    typedef unsigned short uint16_t;
    typedef unsigned int   uint32_t;
#endif

 //---------------------------------------------------------
static uint32_t const skMidPitch             = 0x2000U;
static int16_t  const skNrStepPitch          = 25; // 25 steps within a half-tone for pitch bend
static int16_t  const skVersionMajor         = 4;
static int16_t  const skVersionMinor         = 0;
static uint8_t  const skMaxVolume            = 0x7FU;
static uint8_t  const skMaxNotes             = 96U;
static uint8_t  const skCarrierOpOffset      = 3U;
static uint8_t  const skNumSemitonesInOctave = 12U;
//---------------------------------------------------------
static uint8_t const skOPL2_WaveCtrlBaseAddress      = 0x01U; // Test LSI / Enable waveform control
static uint8_t const skOPL2_AaMultiBaseAddress       = 0x20U; // Amp Mod / Vibrato / EG type / Key Scaling / Multiple
static uint8_t const skOPL2_KSLTLBaseAddress         = 0x40U; // Key scaling level / Operator output level
static uint8_t const skOPL2_ArDrBaseAddress          = 0x60U; // Attack Rate / Decay Rate
static uint8_t const skOPL2_SlrrBaseAddress          = 0x80U; // Sustain Level / Release Rate
static uint8_t const skOPL2_FreqLoBaseAddress        = 0xA0U; // Frequency (low 8 bits)
static uint8_t const skOPL2_KeyOnFreqHiBaseAddress   = 0xB0U; // Key On / Octave / Frequency (high 2 bits)
static uint8_t const skOPL2_AmVibRhythmBaseAddress   = 0xBDU; // AM depth / Vibrato depth / Rhythm control
static uint8_t const skOPL2_FeedConBaseAddress       = 0xC0U; // Feedback strength / Connection type
static uint8_t const skOPL2_WaveformBaseAddress      = 0xE0U; // Waveform select
//---------------------------------------------------------
static uint8_t const skOPL2_EnableWaveformSelectMask = 0x20U;
static uint8_t const skOPL2_KeyOnMask                = 0x20U;
static uint8_t const skOPL2_RhythmMask               = 0x20U;
static uint8_t const skOPL2_KSLMask                  = 0xC0U;
static uint8_t const skOPL2_TLMask                   = 0x3FU;
static uint8_t const skOPL2_TLMinLevel               = 0x3FU;
static uint8_t const skOPL2_FNumLSBMask              = 0xFFU;
static uint8_t const skOPL2_FNumMSBMask              = 0x03U;
static uint8_t const skOPL2_FNumMSBShift             = 0x08U;
static uint8_t const skOPL2_BlockNumberShift         = 0x02U;
//---------------------------------------------------------
static uint8_t const skNoteOctave[skMaxNotes] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
};
//---------------------------------------------------------
static uint8_t const skNoteIndex[skMaxNotes] =
{
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
};
//---------------------------------------------------------
// Table below generated by initialize_fnum_table function (from Adlib Music SDK).
static uint16_t const skFNumNotes[skNrStepPitch][skNumSemitonesInOctave] =
{
    343, 364, 385, 408, 433, 459, 486, 515, 546, 579, 614, 650,
    344, 365, 387, 410, 434, 460, 488, 517, 548, 581, 615, 652,
    345, 365, 387, 410, 435, 461, 489, 518, 549, 582, 617, 653,
    346, 366, 388, 411, 436, 462, 490, 519, 550, 583, 618, 655,
    346, 367, 389, 412, 437, 463, 491, 520, 551, 584, 619, 657,
    347, 368, 390, 413, 438, 464, 492, 522, 553, 586, 621, 658,
    348, 369, 391, 415, 439, 466, 493, 523, 554, 587, 622, 660,
    349, 370, 392, 415, 440, 467, 495, 524, 556, 589, 624, 661,
    350, 371, 393, 416, 441, 468, 496, 525, 557, 590, 625, 663,
    351, 372, 394, 417, 442, 469, 497, 527, 558, 592, 627, 665,
    351, 372, 395, 418, 443, 470, 498, 528, 559, 593, 628, 666,
    352, 373, 396, 419, 444, 471, 499, 529, 561, 594, 630, 668,
    353, 374, 397, 420, 445, 472, 500, 530, 562, 596, 631, 669,
    354, 375, 398, 421, 447, 473, 502, 532, 564, 597, 633, 671,
    355, 376, 398, 422, 448, 474, 503, 533, 565, 599, 634, 672,
    356, 377, 399, 423, 449, 475, 504, 534, 566, 600, 636, 674,
    356, 378, 400, 424, 450, 477, 505, 535, 567, 601, 637, 675,
    357, 379, 401, 425, 451, 478, 506, 537, 569, 603, 639, 677,
    358, 379, 402, 426, 452, 479, 507, 538, 570, 604, 640, 679,
    359, 380, 403, 427, 453, 480, 509, 539, 571, 606, 642, 680,
    360, 381, 404, 428, 454, 481, 510, 540, 572, 607, 643, 682,
    360, 382, 405, 429, 455, 482, 511, 541, 574, 608, 645, 683,
    361, 383, 406, 430, 456, 483, 512, 543, 575, 610, 646, 685,
    362, 384, 407, 431, 457, 484, 513, 544, 577, 611, 648, 687,
    363, 385, 408, 432, 458, 485, 514, 545, 578, 612, 649, 688
};
//---------------------------------------------------------
static uint8_t const drum_op_table[4] = { 0x14, 0x12, 0x15, 0x11 };
//---------------------------------------------------------
static inline float fmin(int const a, int const b)
{
    return static_cast<float>(a < b ? a : b);
}
//---------------------------------------------------------
int   const CrolPlayer::kSizeofDataRecord    = 30;
int   const CrolPlayer::kMaxTickBeat         = 60;
int   const CrolPlayer::kSilenceNote         = -12;
int   const CrolPlayer::kNumMelodicVoices    = 9;
int   const CrolPlayer::kNumPercussiveVoices = 11;
int   const CrolPlayer::kBassDrumChannel     = 6;
int   const CrolPlayer::kSnareDrumChannel    = 7;
int   const CrolPlayer::kTomtomChannel       = 8;
int   const CrolPlayer::kTomTomNote          = 24;
int   const CrolPlayer::kTomTomToSnare       = 7; // 7 half-tones between voice 7 & 8
int   const CrolPlayer::kSnareNote           = CrolPlayer::kTomTomNote + CrolPlayer::kTomTomToSnare;
float const CrolPlayer::kDefaultUpdateTme    = 18.2f;

/*** public methods **************************************/

CPlayer * CrolPlayer::factory(Copl * pNewOpl)
{
  return new CrolPlayer(pNewOpl);
}
//---------------------------------------------------------
CrolPlayer::CrolPlayer(Copl * const pNewOpl)
    : CPlayer            (pNewOpl)
    , mpROLHeader        (NULL)
    , mpOldFNumFreqPtr   (NULL)
    , mTempoEvents       ()
    , mVoiceData         ()
    , mInstrumentList    ()
    , mFNumFreqPtrList   (kNumPercussiveVoices, skFNumNotes[0])
    , mHalfToneOffset    (kNumPercussiveVoices, 0)
    , mVolumeCache       (kNumPercussiveVoices, skMaxVolume)
    , mKSLTLCache        (kNumPercussiveVoices, 0)
    , mNoteCache         (kNumPercussiveVoices, 0)
    , mKOnOctFNumCache   (kNumMelodicVoices, 0)
    , mKeyOnCache        (kNumPercussiveVoices, false)
    , mRefresh           (kDefaultUpdateTme)
    , mOldPitchBendLength(~0)
    , mPitchRangeStep    (skNrStepPitch)
    , mNextTempoEvent    (0)
    , mCurrTick          (0)
    , mTimeOfLastNote    (0)
    , mOldHalfToneOffset (0)
    , mAMVibRhythmCache  (0)
{
}
//---------------------------------------------------------
CrolPlayer::~CrolPlayer()
{
    if (mpROLHeader != NULL)
    {
        delete mpROLHeader;
        mpROLHeader = NULL;
    }
}
//---------------------------------------------------------
bool CrolPlayer::load(const std::string & filename, const CFileProvider & fp)
{
    binistream *f = fp.open(filename);

    if (!f)
    {
        return false;
    }

    char *fn = new char[filename.length()+13];
    int i;
    std::string bnk_filename;

    AdPlug_LogWrite("*** CrolPlayer::load(f, \"%s\") ***\n", filename.c_str());
    strcpy(fn,filename.data());
    for (i = strlen(fn) - 1; i >= 0; i--)
    {
        if (fn[i] == '/' || fn[i] == '\\')
        {
            break;
        }
    }
    strcpy(fn+i+1,"standard.bnk");
    bnk_filename = fn;
    delete [] fn;
    AdPlug_LogWrite("bnk_filename = \"%s\"\n",bnk_filename.c_str());

    mpROLHeader = new SRolHeader;
    memset(mpROLHeader, 0, sizeof(SRolHeader));

    mpROLHeader->version_major = static_cast<uint16_t>(f->readInt(2));
    mpROLHeader->version_minor = static_cast<uint16_t>(f->readInt(2));

    // Version check
    if ((mpROLHeader->version_major != skVersionMinor) || (mpROLHeader->version_minor != skVersionMajor))
    {
        AdPlug_LogWrite("Unsupported file version %d.%d or not a ROL file!\n",
                        mpROLHeader->version_major, mpROLHeader->version_minor);
        AdPlug_LogWrite("--- CrolPlayer::load ---\n");
        fp.close(f);
        return false;
    }

    f->seek(ROL_UNSUED0_SIZE, binio::Add); // Seek past 'SRolHeader.unused' field of header

    mpROLHeader->ticks_per_beat    = static_cast<uint16_t>(f->readInt(2));
    mpROLHeader->beats_per_measure = static_cast<uint16_t>(f->readInt(2));
    mpROLHeader->edit_scale_y      = static_cast<uint16_t>(f->readInt(2));
    mpROLHeader->edit_scale_x      = static_cast<uint16_t>(f->readInt(2));

    f->seek(ROL_UNUSED1_SIZE, binio::Add); // Seek past 'SRolHeader.(unused1)' field of the header.

    mpROLHeader->mode = static_cast<uint8_t>(f->readInt(1));

    f->seek(ROL_UNUSED2_SIZE + ROL_FILLER0_SIZE + ROL_FILLER1_SIZE, binio::Add); // Seek past 'SRolHeader.(unused2, filler0, filler1)' field of header

    mpROLHeader->basic_tempo = static_cast<float>(f->readFloat(binio::Single));

    load_tempo_events(f);

    mTimeOfLastNote = 0;

    if (load_voice_data(f, bnk_filename, fp) != true)
    {
      AdPlug_LogWrite("CrolPlayer::load_voice_data(f) failed!\n");
      AdPlug_LogWrite("--- CrolPlayer::load ---\n");

      fp.close(f);
      return false;
    }

    fp.close(f);

    rewind(0);
    AdPlug_LogWrite("--- CrolPlayer::load ---\n");
    return true;
}
//---------------------------------------------------------
bool CrolPlayer::update()
{
    if ((mNextTempoEvent < mTempoEvents.size()) &&
        (mTempoEvents[mNextTempoEvent].time == mCurrTick))
    {
        SetRefresh(mTempoEvents[mNextTempoEvent].multiplier);
        ++mNextTempoEvent;
    }

    TVoiceData::iterator curr = mVoiceData.begin();
    TVoiceData::iterator end  = mVoiceData.end();
    int voice                 = 0;

    while(curr != end)
    {
        UpdateVoice(voice, *curr);
        ++curr;
        ++voice;
    }

    ++mCurrTick;

    if (mCurrTick > mTimeOfLastNote)
    {
        return false;
    }

    return true;
}
//---------------------------------------------------------
void CrolPlayer::rewind(int subsong)
{
    TVoiceData::iterator curr = mVoiceData.begin();
    TVoiceData::iterator end  = mVoiceData.end();

    while(curr != end)
    {
        CVoiceData & voice = *curr;

        voice.Reset();
        ++curr;
    }

    mHalfToneOffset  = TInt16Vector(kNumPercussiveVoices, 0);
    mVolumeCache     = TUInt8Vector(kNumPercussiveVoices, skMaxVolume);
    mKSLTLCache      = TUInt8Vector(kNumPercussiveVoices, 0);
    mNoteCache       = TUInt8Vector(kNumPercussiveVoices, 0);
    mKOnOctFNumCache = TUInt8Vector(kNumMelodicVoices, 0);
    mKeyOnCache      = TBoolVector(kNumPercussiveVoices, false);

    mNextTempoEvent = 0;
    mCurrTick = 0;
    mAMVibRhythmCache = 0;

    opl->init();         // initialize to melodic by default
    opl->write(skOPL2_WaveCtrlBaseAddress, skOPL2_EnableWaveformSelectMask); // Enable waveform select

    if (mpROLHeader->mode == 0)
    {
        mAMVibRhythmCache = skOPL2_RhythmMask;
        opl->write(skOPL2_AmVibRhythmBaseAddress, mAMVibRhythmCache); // Enable rhythm mode

        SetFreq(kTomtomChannel, kTomTomNote);
        SetFreq(kSnareDrumChannel, kSnareNote);
    }

    SetRefresh(1.0f);
}
//---------------------------------------------------------
void CrolPlayer::SetRefresh( float const multiplier )
{
    float const tickBeat = static_cast<float>(fmin(kMaxTickBeat, mpROLHeader->ticks_per_beat));

    mRefresh = (tickBeat*mpROLHeader->basic_tempo*multiplier) / 60.0f;
}
//---------------------------------------------------------
float CrolPlayer::getrefresh()
{
    return mRefresh;
}
//---------------------------------------------------------
void CrolPlayer::UpdateVoice(int const voice, CVoiceData & voiceData)
{
    TNoteEvents const & nEvents = voiceData.note_events;

    if (nEvents.empty() || (voiceData.mEventStatus & CVoiceData::kES_NoteEnd))
    {
        return; // no note data to process, don't bother doing anything.
    }

    TInstrumentEvents const & iEvents = voiceData.instrument_events;
    TVolumeEvents     const & vEvents = voiceData.volume_events;
    TPitchEvents      const & pEvents = voiceData.pitch_events;

    if ((voiceData.mEventStatus & CVoiceData::kES_InstrEnd) == 0)
    {
        if (voiceData.next_instrument_event < iEvents.size())
        {
            if (iEvents[voiceData.next_instrument_event].time == mCurrTick)
            {
                send_ins_data_to_chip(voice, iEvents[voiceData.next_instrument_event].ins_index);
                ++voiceData.next_instrument_event;
            }
        }
        else
        {
            voiceData.mEventStatus |= CVoiceData::kES_InstrEnd;
        }
    }

    if ((voiceData.mEventStatus & CVoiceData::kES_VolumeEnd) == 0)
    {
        if (voiceData.next_volume_event < vEvents.size())
        {
            if (vEvents[voiceData.next_volume_event].time == mCurrTick)
            {
                SVolumeEvent const & volumeEvent = vEvents[voiceData.next_volume_event];

                uint8_t const volume = (uint8_t)(skMaxVolume * volumeEvent.multiplier);

                SetVolume(voice, volume);

                ++voiceData.next_volume_event; // move to next volume event
            }
        }
        else
        {
            voiceData.mEventStatus |= CVoiceData::kES_VolumeEnd;
        }        
    }

    if (voiceData.mForceNote || (voiceData.current_note_duration > voiceData.mNoteDuration-1))
    {
        if (mCurrTick != 0)
        {
            ++voiceData.current_note;
        }

        if (voiceData.current_note < nEvents.size())
        {
            SNoteEvent const & noteEvent = nEvents[voiceData.current_note];

            SetNote(voice, noteEvent.number);
            voiceData.current_note_duration = 0;
            voiceData.mNoteDuration         = noteEvent.duration;
            voiceData.mForceNote            = false;
        }
        else
        {
            SetNote(voice, kSilenceNote);
            voiceData.mEventStatus |= CVoiceData::kES_NoteEnd;
            return;
        }
    }

    if ((voiceData.mEventStatus & CVoiceData::kES_PitchEnd) == 0)
    {
        if ( voiceData.next_pitch_event < pEvents.size() )
        {
            if (pEvents[voiceData.next_pitch_event].time == mCurrTick)
            {
                SetPitch(voice, pEvents[voiceData.next_pitch_event].variation);
                ++voiceData.next_pitch_event;
            }
        }
        else
        {
            voiceData.mEventStatus |= CVoiceData::kES_PitchEnd;
        }
    }

    ++voiceData.current_note_duration;
}
//---------------------------------------------------------
void CrolPlayer::SetNote(int const voice, int const note)
{
    if ((voice < kBassDrumChannel) || mpROLHeader->mode)
    {
        SetNoteMelodic(voice, note);
    }
    else
    {
        SetNotePercussive(voice, note);
    }
}
//---------------------------------------------------------
void CrolPlayer::SetNotePercussive(int const voice, int const note)
{
    int const channel_bit_mask = 1 << (4-voice+kBassDrumChannel);

    mAMVibRhythmCache &= ~channel_bit_mask;
    opl->write(skOPL2_AmVibRhythmBaseAddress, mAMVibRhythmCache);
    mKeyOnCache[voice] = false;

    if (note != kSilenceNote)
    {
        switch(voice)
        {
            case kTomtomChannel:
                SetFreq(kTomtomChannel, note);
                SetFreq(kSnareDrumChannel, note + kTomTomToSnare);
                break;

            case kBassDrumChannel:
                SetFreq(voice, note);
                break;
            default:
                // Does nothing
                break;
        }

        mKeyOnCache[voice] = true;
        mAMVibRhythmCache |= channel_bit_mask;
        opl->write(skOPL2_AmVibRhythmBaseAddress, mAMVibRhythmCache);
    }
}
//---------------------------------------------------------
void CrolPlayer::SetNoteMelodic(int const voice, int const note)
{
    opl->write(skOPL2_KeyOnFreqHiBaseAddress + voice, mKOnOctFNumCache[voice] & ~skOPL2_KeyOnMask);
    mKeyOnCache[voice] = false;

    if (note != kSilenceNote)
    {
        SetFreq(voice, note, true);
    }
}
//---------------------------------------------------------
// From Adlib Music SDK's ADLIB.C ...
void CrolPlayer::ChangePitch(int voice, uint16_t const pitchBend)
{
    int32_t const pitchBendLength = (int32_t)(pitchBend - skMidPitch) * mPitchRangeStep;

    if (mOldPitchBendLength == pitchBendLength)
    {
        // optimisation ...
        mFNumFreqPtrList[voice] = mpOldFNumFreqPtr;
        mHalfToneOffset[voice] = mOldHalfToneOffset;
    }
    else
    {
        int16_t const pitchStepDir = pitchBendLength / skMidPitch;
        int16_t delta;
        if (pitchStepDir < 0)
        {
            int16_t const pitchStepDown = skNrStepPitch - 1 - pitchStepDir;
            mOldHalfToneOffset = mHalfToneOffset[voice] = -(pitchStepDown / skNrStepPitch);
            delta = (pitchStepDown - skNrStepPitch + 1) % skNrStepPitch;
            if (delta)
            {
                delta = skNrStepPitch - delta;
            }
        }
        else
        {
            mOldHalfToneOffset = mHalfToneOffset[voice] = pitchStepDir / skNrStepPitch;
            delta = pitchStepDir % skNrStepPitch;
        }
        mpOldFNumFreqPtr = mFNumFreqPtrList[voice] = skFNumNotes[delta];
        mOldPitchBendLength = pitchBendLength;
    }
}
//---------------------------------------------------------
void CrolPlayer::SetPitch(int const voice, float const variation)
{
    if ((voice < kBassDrumChannel) || mpROLHeader->mode)
    {
        uint16_t const pitchBend = (variation == 1.0f) ? skMidPitch : static_cast<uint16_t>((0x3fff >> 1) * variation);

        ChangePitch(voice, pitchBend);
        SetFreq(voice, mNoteCache[voice], mKeyOnCache[voice]);
    }
}
//---------------------------------------------------------
void CrolPlayer::SetFreq(int const voice, int const note, bool const keyOn)
{
    int const biased_note = std::max(0, std::min((skMaxNotes-1), note + mHalfToneOffset[voice]));

    uint16_t const frequency = *(mFNumFreqPtrList[voice] + skNoteIndex[biased_note]);

    mNoteCache[voice] = note;
    mKeyOnCache[voice] = keyOn;

    mKOnOctFNumCache[voice] = (skNoteOctave[biased_note] << skOPL2_BlockNumberShift) | ((frequency >> skOPL2_FNumMSBShift) & skOPL2_FNumMSBMask);

    opl->write(skOPL2_FreqLoBaseAddress + voice, frequency & skOPL2_FNumLSBMask);
    opl->write(skOPL2_KeyOnFreqHiBaseAddress + voice, mKOnOctFNumCache[voice] | (keyOn ? skOPL2_KeyOnMask : 0x0));
}
//---------------------------------------------------------
uint8_t CrolPlayer::GetKSLTL(int const voice) const
{
    uint16_t kslTL = skOPL2_TLMinLevel - (mKSLTLCache[voice] & skOPL2_TLMask); // amplitude

    kslTL = mVolumeCache[voice] * kslTL;
    kslTL += kslTL + skMaxVolume;	// round off to 0.5
    kslTL = skOPL2_TLMinLevel - (kslTL / (2 * skMaxVolume));

    kslTL |= mKSLTLCache[voice] & skOPL2_KSLMask;

    return static_cast<uint8_t>(kslTL);
}
//---------------------------------------------------------
void CrolPlayer::SetVolume(int const voice, uint8_t const volume)
{
    uint8_t const op_offset = (voice < kSnareDrumChannel || mpROLHeader->mode) ? op_table[voice] + skCarrierOpOffset : drum_op_table[voice - kSnareDrumChannel];

    mVolumeCache[voice] = volume;

    opl->write(skOPL2_KSLTLBaseAddress + op_offset, GetKSLTL(voice));
}
//---------------------------------------------------------
void CrolPlayer::send_ins_data_to_chip(int const voice, int const ins_index)
{
    SRolInstrument const & instrument = mInstrumentList[ins_index].instrument;

    send_operator(voice, instrument.modulator, instrument.carrier);
}
//---------------------------------------------------------
void CrolPlayer::send_operator(int const voice, SOPL2Op const & modulator,  SOPL2Op const & carrier)
{
    if ((voice < kSnareDrumChannel) || mpROLHeader->mode)
    {
        uint8_t const op_offset = op_table[voice];

        opl->write(skOPL2_AaMultiBaseAddress  + op_offset, modulator.ammulti);
        opl->write(skOPL2_KSLTLBaseAddress    + op_offset, modulator.ksltl);
        opl->write(skOPL2_ArDrBaseAddress     + op_offset, modulator.ardr);
        opl->write(skOPL2_SlrrBaseAddress     + op_offset, modulator.slrr);
        opl->write(skOPL2_FeedConBaseAddress  + voice    , modulator.fbc);
        opl->write(skOPL2_WaveformBaseAddress + op_offset, modulator.waveform);

        mKSLTLCache[voice] = carrier.ksltl;

        opl->write(skOPL2_AaMultiBaseAddress  + op_offset + skCarrierOpOffset, carrier.ammulti);
        opl->write(skOPL2_KSLTLBaseAddress    + op_offset + skCarrierOpOffset, GetKSLTL(voice));
        opl->write(skOPL2_ArDrBaseAddress     + op_offset + skCarrierOpOffset, carrier.ardr);
        opl->write(skOPL2_SlrrBaseAddress     + op_offset + skCarrierOpOffset, carrier.slrr);
        opl->write(skOPL2_WaveformBaseAddress + op_offset + skCarrierOpOffset, carrier.waveform);
    }
    else
    {
        uint8_t const op_offset = drum_op_table[voice-kSnareDrumChannel];

        mKSLTLCache[voice] = modulator.ksltl;

        opl->write(skOPL2_AaMultiBaseAddress  + op_offset, modulator.ammulti);
        opl->write(skOPL2_KSLTLBaseAddress    + op_offset, GetKSLTL(voice));
        opl->write(skOPL2_ArDrBaseAddress     + op_offset, modulator.ardr);
        opl->write(skOPL2_SlrrBaseAddress     + op_offset, modulator.slrr);
        opl->write(skOPL2_WaveformBaseAddress + op_offset, modulator.waveform);
    }
}
//---------------------------------------------------------
void CrolPlayer::load_tempo_events(binistream *f)
{
    int16_t const num_tempo_events = static_cast<uint16_t>(f->readInt(2));

    mTempoEvents.reserve(num_tempo_events);

    for (int i=0; i<num_tempo_events; ++i)
    {
        STempoEvent event;

        event.time       = static_cast<int16_t>(f->readInt(2));
        event.multiplier = static_cast<float>(f->readFloat(binio::Single));
        mTempoEvents.push_back(event);
    }
}
//---------------------------------------------------------
bool CrolPlayer::load_voice_data(binistream *f, std::string const &bnk_filename, const CFileProvider &fp)
{
    SBnkHeader bnk_header;
    binistream *bnk_file = fp.open(bnk_filename.c_str());

    if (bnk_file)
    {
        load_bnk_info(bnk_file, bnk_header);

        int const numVoices = mpROLHeader->mode ? kNumMelodicVoices : kNumPercussiveVoices;

        mVoiceData.reserve(numVoices);
        for (int i=0; i<numVoices; ++i)
        {
            CVoiceData voice;

            load_note_events(f, voice);
            load_instrument_events(f, voice, bnk_file, bnk_header);
            load_volume_events(f, voice);
            load_pitch_events(f, voice);

            mVoiceData.push_back(voice);
        }

        fp.close(bnk_file);

        return true;
    }

    return false;
}
//---------------------------------------------------------
void CrolPlayer::load_note_events(binistream *f, CVoiceData & voice)
{
    f->seek(ROL_FILLER_SIZE, binio::Add);

    int16_t const time_of_last_note = static_cast<int16_t>(f->readInt(2));

    if (time_of_last_note != 0)
    {
        TNoteEvents & note_events = voice.note_events;
        int16_t total_duration    = 0;

        do
        {
            SNoteEvent event;

            event.number   = static_cast<int16_t>(f->readInt(2));
            event.duration = static_cast<int16_t>(f->readInt(2));

            event.number += kSilenceNote; // adding -12

            note_events.push_back(event);

            total_duration += event.duration;
        } while (total_duration < time_of_last_note);

        if (time_of_last_note > mTimeOfLastNote)
        {
            mTimeOfLastNote = time_of_last_note;
        }
    }

    f->seek(ROL_FILLER_SIZE, binio::Add);
}
//---------------------------------------------------------
void CrolPlayer::load_instrument_events(binistream *f, CVoiceData & voice,
                                        binistream *bnk_file, SBnkHeader const & bnk_header)
{
    int16_t const number_of_instrument_events = static_cast<int16_t>(f->readInt(2));

    TInstrumentEvents & instrument_events = voice.instrument_events;

    instrument_events.reserve(number_of_instrument_events);

    for (int16_t i = 0; i < number_of_instrument_events; ++i)
    {
        SInstrumentEvent event;
        event.time = static_cast<int16_t>(f->readInt(2));
        f->readString(event.name, ROL_MAX_NAME_SIZE);

	    std::string event_name = event.name;
        event.ins_index = load_rol_instrument(bnk_file, bnk_header, event_name);

        instrument_events.push_back(event);

        f->seek(ROL_INSTRUMENT_EVENT_FILLER_SIZE, binio::Add);
    }

    f->seek(ROL_FILLER_SIZE, binio::Add);
}
//---------------------------------------------------------
void CrolPlayer::load_volume_events(binistream *f, CVoiceData & voice)
{
    int16_t const number_of_volume_events = static_cast<int16_t>(f->readInt(2));

    TVolumeEvents & volume_events = voice.volume_events;

    volume_events.reserve(number_of_volume_events);

    for (int i=0; i<number_of_volume_events; ++i)
    {
        SVolumeEvent event;
        event.time       = static_cast<int16_t>(f->readInt(2));
        event.multiplier = static_cast<float>(f->readFloat(binio::Single));

        volume_events.push_back(event);
    }

    f->seek(ROL_FILLER_SIZE, binio::Add);
}
//---------------------------------------------------------
void CrolPlayer::load_pitch_events(binistream *f, CVoiceData & voice)
{
    int16_t const number_of_pitch_events = static_cast<int16_t>(f->readInt(2));

    TPitchEvents & pitch_events = voice.pitch_events;

    pitch_events.reserve(number_of_pitch_events);

    for (int i=0; i<number_of_pitch_events; ++i)
    {
        SPitchEvent event;
        event.time      = static_cast<int16_t>(f->readInt(2));
        event.variation = static_cast<float>(f->readFloat(binio::Single));

        pitch_events.push_back(event);
    }
}
//---------------------------------------------------------
bool CrolPlayer::load_bnk_info(binistream *f, SBnkHeader & header)
{
    header.version_major = static_cast<uint8_t>(f->readInt(1));
    header.version_minor = static_cast<uint8_t>(f->readInt(1));
    f->readString(header.signature, ROL_BNK_SIGNATURE_SIZE);

    header.number_of_list_entries_used  = static_cast<uint16_t>(f->readInt(2));
    header.total_number_of_list_entries = static_cast<uint16_t>(f->readInt(2));

    header.abs_offset_of_name_list = static_cast<int32>(f->readInt(4));
    header.abs_offset_of_data      = static_cast<int32>(f->readInt(4));

    f->seek(header.abs_offset_of_name_list, binio::Set);

    TInstrumentNames & ins_name_list = header.ins_name_list;
    ins_name_list.reserve(header.number_of_list_entries_used);

    for (uint16_t i=0; i<header.number_of_list_entries_used; ++i)
    {
        SInstrumentName instrument;

        instrument.index = static_cast<uint16_t>(f->readInt(2));
        instrument.record_used = static_cast<uint8_t>(f->readInt(1));
        f->readString(instrument.name, ROL_MAX_NAME_SIZE);

        ins_name_list.push_back( instrument );
    }

    return true;
}
//---------------------------------------------------------
int CrolPlayer::load_rol_instrument(binistream *f, SBnkHeader const & header, std::string const & name)
{
    TInstrumentNames const & ins_name_list = header.ins_name_list;

    int const ins_index = get_ins_index(name);

    if (ins_index != -1)
    {
        return ins_index;
    }

    SInstrument usedInstrument;
    usedInstrument.name = name;

    typedef TInstrumentNames::const_iterator TInsIter;
    typedef std::pair<TInsIter, TInsIter>    TInsIterPair;

    TInsIterPair const range = std::equal_range(ins_name_list.begin(), 
                                                ins_name_list.end(), 
                                                name, 
                                                StringCompare());

    if (range.first != range.second)
    {
        int const seekOffs = header.abs_offset_of_data + (range.first->index * kSizeofDataRecord);
        f->seek(seekOffs, binio::Set);

        read_rol_instrument(f, usedInstrument.instrument);
    }
    else
    {
        // set up default instrument data here
        memset(&usedInstrument.instrument, 0, sizeof(SRolInstrument));
    }

    mInstrumentList.push_back( usedInstrument );

    return mInstrumentList.size()-1;
}
//---------------------------------------------------------
int CrolPlayer::get_ins_index(std::string const & name) const
{
    for (size_t index = 0; index<mInstrumentList.size(); ++index)
    {
        if (stricmp(mInstrumentList[index].name.c_str(), name.c_str()) == 0)
        {
            return index;
        }
    }

    return -1;
}
//---------------------------------------------------------
void CrolPlayer::read_rol_instrument(binistream * f, SRolInstrument & instrument)
{
    instrument.mode = static_cast<uint8_t>(f->readInt(1));
    instrument.voice_number = static_cast<uint8_t>(f->readInt(1));

    read_fm_operator(f, instrument.modulator);
    read_fm_operator(f, instrument.carrier);

    instrument.modulator.waveform = static_cast<uint8_t>(f->readInt(1));
    instrument.carrier.waveform = static_cast<uint8_t>(f->readInt(1));
}
//---------------------------------------------------------
void CrolPlayer::read_fm_operator(binistream *f, SOPL2Op &opl2_op)
{
    SFMOperator fm_op;

    fm_op.key_scale_level = static_cast<uint8_t>(f->readInt(1));
    fm_op.freq_multiplier = static_cast<uint8_t>(f->readInt(1));
    fm_op.feed_back = static_cast<uint8_t>(f->readInt(1));
    fm_op.attack_rate = static_cast<uint8_t>(f->readInt(1));
    fm_op.sustain_level = static_cast<uint8_t>(f->readInt(1));
    fm_op.sustaining_sound = static_cast<uint8_t>(f->readInt(1));
    fm_op.decay_rate = static_cast<uint8_t>(f->readInt(1));
    fm_op.release_rate = static_cast<uint8_t>(f->readInt(1));
    fm_op.output_level = static_cast<uint8_t>(f->readInt(1));
    fm_op.amplitude_vibrato = static_cast<uint8_t>(f->readInt(1));
    fm_op.frequency_vibrato = static_cast<uint8_t>(f->readInt(1));
    fm_op.envelope_scaling = static_cast<uint8_t>(f->readInt(1));
    fm_op.fm_type = static_cast<uint8_t>(f->readInt(1));

    opl2_op.ammulti = fm_op.amplitude_vibrato << 7 | fm_op.frequency_vibrato << 6 | fm_op.sustaining_sound << 5 | fm_op.envelope_scaling << 4 | fm_op.freq_multiplier;
    opl2_op.ksltl   = fm_op.key_scale_level   << 6 | fm_op.output_level;
    opl2_op.ardr    = fm_op.attack_rate       << 4 | fm_op.decay_rate;
    opl2_op.slrr    = fm_op.sustain_level     << 4 | fm_op.release_rate;
    opl2_op.fbc     = fm_op.feed_back         << 1 | (fm_op.fm_type ^ 1);
}

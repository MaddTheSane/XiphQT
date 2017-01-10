/*
 *  vorbis_entrypoints.cpp
 *
 *    Declaration of the entry points for the Vorbis component.
 *
 *
 *  Copyright (c) 2005-2006  Arek Korbik
 *
 *  This file is part of XiphQT, the Xiph QuickTime Components.
 *
 *  XiphQT is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  XiphQT is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with XiphQT; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *
 *  Last modified: $Id: vorbis_entrypoints.cpp 12346 2007-01-18 13:45:42Z arek $
 *
 */


#include <AudioUnit/AudioCodec.h>

#include "ACPlugInDispatch.h"
#include "CAOpusDecoder.h"
#include "CAOggOpusDecoder.h"
#include "CAOpusEncoder.h"


AUDIOCOMPONENT_ENTRY(AudioCodecFactory, CAOpusDecoder);
AUDIOCOMPONENT_ENTRY(AudioCodecFactory, CAOggOpusDecoder);

#if !defined(XIPHQT_NO_ENCODERS)

AUDIOCOMPONENT_ENTRY(AudioCodecFactory, CAOpusEncoder);

#endif  /* XIPHQT_NO_ENCODERS */

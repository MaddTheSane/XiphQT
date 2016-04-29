//
//  EntryPoints.cpp
//  Xiph Audio Components
//
//  Created by C.W. Betts on 4/29/16.
//  Copyright © 2016 C.W. Betts. All rights reserved.
//

#include <stdio.h>
#include "CAFLACDecoder.h"
#include "CAOggFLACDecoder.h"
#include "CASpeexDecoder.h"
#include "CAOggSpeexDecoder.h"
#include "CAVorbisDecoder.h"
#include "CAOggVorbisDecoder.h"
#include "CAVorbisEncoder.h"
#include "ACPlugInDispatch.h"

AUDIOCOMPONENT_ENTRY(AudioCodecFactory, CAVorbisDecoder);
AUDIOCOMPONENT_ENTRY(AudioCodecFactory, CAFLACDecoder);
AUDIOCOMPONENT_ENTRY(AudioCodecFactory, CAOggFLACDecoder);
AUDIOCOMPONENT_ENTRY(AudioCodecFactory, CASpeexDecoder);
AUDIOCOMPONENT_ENTRY(AudioCodecFactory, CAOggSpeexDecoder);
AUDIOCOMPONENT_ENTRY(AudioCodecFactory, CAVorbisEncoder);
AUDIOCOMPONENT_ENTRY(AudioCodecFactory, CAOggVorbisDecoder);


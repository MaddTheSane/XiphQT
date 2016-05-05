//
//  OggAudioFileComponent.cpp
//  Xiph Audio Components
//
//  Created by C.W. Betts on 5/5/16.
//  Copyright © 2016 C.W. Betts. All rights reserved.
//

#include "OggAudioFileComponent.hpp"
#include "OggAudioFile.hpp"

OggAudioFormat* gOggAudioFormat = new OggAudioFormat();

AudioFileFormat* OggAudioFileComponent::GetAudioFormat() const
{
	return gOggAudioFormat;
}

OggAudioFileComponent::OggAudioFileComponent(AudioComponentInstance inInstance)
: AudioFileObjectComponentBase(inInstance)
{
#if VERBOSE
	printf("OggAudioFileComponent::OggAudioFileComponent\n");
#endif
	
	SetAudioFileObject(new OggAudioFile());
}

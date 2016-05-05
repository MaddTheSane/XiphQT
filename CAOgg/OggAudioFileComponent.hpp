//
//  OggAudioFileComponent.hpp
//  Xiph Audio Components
//
//  Created by C.W. Betts on 5/5/16.
//  Copyright © 2016 C.W. Betts. All rights reserved.
//

#ifndef OggAudioFileComponent_hpp
#define OggAudioFileComponent_hpp

#define kOggAudioFileComponent_Version 1

#include "AudioFileComponentBase.h"

class OggAudioFileComponent : public AudioFileObjectComponentBase {
public:
	OggAudioFileComponent(AudioComponentInstance inInstance);
	
	virtual AudioFileFormat* GetAudioFormat() const;
	
	virtual ComponentResult	Version() { return kOggAudioFileComponent_Version; }
};


#endif /* OggAudioFileComponent_hpp */

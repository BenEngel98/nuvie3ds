/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * $URL: https://scummvm.svn.sourceforge.net/svnroot/scummvm/scummvm/trunk/backends/mixer/sdl/sdl-mixer.cpp $
 * $Id: sdl-mixer.cpp 54584 2010-11-29 18:48:43Z lordhoto $
 *
 */
#include <assert.h>
#include <string.h>

#include "SDL.h"
#include "nuvieDefs.h"
#include "sdl-mixer.h"
//#include "common/system.h"
//#include "common/config-manager.h"

#ifdef GP2X
#define SAMPLES_PER_SEC 11025
#else
#define SAMPLES_PER_SEC 22050
#endif
//#define SAMPLES_PER_SEC 44100

SdlMixerManager::SdlMixerManager()
	:
	_mixer(0),
	_stream(0),
	_mixBuf(0),
	_mixBufLen(0),
	_audioSuspended(false) {

}

SdlMixerManager::~SdlMixerManager() {
	_mixer->setReady(false);

	if (_stream)
		SDL_DestroyAudioStream(_stream);
	delete [] _mixBuf;

	delete _mixer;
}

void SdlMixerManager::init() {
	// Start SDL Audio subsystem
	if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
		DEBUG(0, LEVEL_ERROR, "Could not initialize SDL: %s", SDL_GetError());
	}

	// Get the desired audio specs
	SDL_AudioSpec desired = getAudioSpec(SAMPLES_PER_SEC);

	// Start SDL audio with the desired specs.  The device converts from
	// our 22 kHz stereo 16-bit to whatever it actually runs at.
	_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired, sdlCallback, this);
	if (_stream == NULL) {
		DEBUG(0,LEVEL_WARNING, "Could not open audio device: %s", SDL_GetError());

		_mixer = new Audio::MixerImpl(desired.freq);
		assert(_mixer); 
		_mixer->setReady(false);
	} else {
		_obtainedRate = desired;
		DEBUG(0,LEVEL_INFORMATIONAL, "Output sample rate: %d Hz", _obtainedRate.freq);

		_mixer = new Audio::MixerImpl(_obtainedRate.freq);
		assert(_mixer); 
		_mixer->setReady(true);

		startAudio();
	}
}

SDL_AudioSpec SdlMixerManager::getAudioSpec(uint32 outputRate) {
	SDL_AudioSpec desired;

	// Determine the desired output sampling frequency.
	uint32 samplesPerSec = 0;
	//ERICif (ConfMan.hasKey("output_rate"))
	//	samplesPerSec = ConfMan.getInt("output_rate");
	if (samplesPerSec <= 0)
		samplesPerSec = outputRate;

	// Determine the sample buffer size. We want it to store enough data for
	// at least 1/16th of a second (though at most 8192 samples). Note
	// that it must be a power of two. So e.g. at 22050 Hz, we request a
	// sample buffer size of 2048.
	uint32 samples = 8192;
	while (samples * 16 > samplesPerSec * 2)
		samples >>= 1;

	memset(&desired, 0, sizeof(desired));
	desired.freq = samplesPerSec;
	desired.format = SDL_AUDIO_S16;
	desired.channels = 2;
	(void)samples;

	return desired;
}

void SdlMixerManager::startAudio() {
	// Start the sound system
	if (_stream)
		SDL_ResumeAudioStreamDevice(_stream);
}

void SdlMixerManager::callbackHandler(uint8 *samples, int len) {
	assert(_mixer);
	_mixer->mixCallback(samples, len);
}

// SDL 3 asks for more data whenever the stream runs low; mix that much and
// queue it.  The old mixer wrote into a fixed buffer, so keep one around.
void SDLCALL SdlMixerManager::sdlCallback(void *this_, SDL_AudioStream *stream, int additional_amount, int total_amount) {
	SdlMixerManager *manager = (SdlMixerManager *)this_;
	assert(manager);
	(void)total_amount;

	if (additional_amount <= 0)
		return;
	if (manager->_mixBufLen < additional_amount) {
		delete [] manager->_mixBuf;
		manager->_mixBuf = new uint8[additional_amount];
		manager->_mixBufLen = additional_amount;
	}
	manager->callbackHandler(manager->_mixBuf, additional_amount);
	SDL_PutAudioStreamData(stream, manager->_mixBuf, additional_amount);
}

void SdlMixerManager::suspendAudio() {
	if (_stream)
		SDL_PauseAudioStreamDevice(_stream);
	_audioSuspended = true;
}

int SdlMixerManager::resumeAudio() {
	if (!_audioSuspended)
		return -2;
	if (!_stream || !SDL_ResumeAudioStreamDevice(_stream)){
		return -1;
	}
	_audioSuspended = false;
	return 0;
}

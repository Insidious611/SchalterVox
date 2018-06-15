#include <switch.h>
#include <stdio.h>
#include <errno.h>
#include <string>
#include <cstring>
#include <stdlib.h>
#include <malloc.h>
#include <cmath>
#include "audiofile.hpp"
#include "playback.hpp"

#define DR_MP3_IMPLEMENTATION
#include "mp3dec.hpp"

#define SAMPLERATE 48000
#define CHANNELCOUNT 2
#define FRAMEMS 1
#define FRAMERATE (1000 / FRAMEMS)
#define SAMPLECOUNT (SAMPLERATE / FRAMERATE)
#define BYTESPERSAMPLE 2
#define BUFFERSIZE (SAMPLECOUNT * CHANNELCOUNT * BYTESPERSAMPLE)

using namespace std;

long calculate_length(int bitrate, long filesize) { // This only works properly with CBR files, but it's the closest we're going to get. Bitrate in kbps.
	// headers are 32 bits, so we add that to bitrate to compensate
	int byterate = (bitrate+32)*1024/8;
	return filesize/byterate;
}

id3 parse_id3_tags(const string& fileName) {
	id3 retval;
	retval.version = 0;
	retval.v1_title[0] = '\0';
	retval.v1_artist[0] = '\0';
	retval.v1_album[0] = '\0';
	retval.v1_comment[0] = '\0';
	retval.v1_tracknum = 0;
	retval.v1_year = 0;
	retval.v1_genre = 0;
	
	FILE* tmp=fopen(fileName.c_str(), "r");
	if (tmp==NULL) { 
		return retval;
	}
	fseek(tmp, -128, SEEK_END);
	u8 id3v1[128];
	fread(&id3v1, 128, 1, tmp);

	if ((id3v1[0] == 'T') && (id3v1[1] == 'A') && (id3v1[2] == 'G')) {
		int id3_ptr = 3;
		retval.version++;
		memcpy(retval.v1_title, id3v1+id3_ptr, 30);
		id3_ptr += 30;
		memcpy(retval.v1_artist, id3v1+id3_ptr, 30);
		id3_ptr += 30;
		memcpy(retval.v1_album, id3v1+id3_ptr, 30);
		u8 tmpyr[4];
		memcpy(tmpyr, id3v1+id3_ptr, 4);
		retval.v1_year = strtol((const char*)tmpyr, NULL, 4);
		id3_ptr += 4;
		memcpy(retval.v1_comment, id3v1+id3_ptr, 30);
		bool tracknum_present = (retval.v1_comment[28]==0);
		if (tracknum_present) {
			retval.v1_tracknum = retval.v1_comment[29];
		}
		retval.v1_genre = id3v1[127];
	}
	rewind(tmp);
	
	u8 id3v2_header[10];
	fread(&id3v2_header, 10, 1, tmp);
	
	if((id3v2_header[0] == 'I') && (id3v2_header[1] == 'D') && (id3v2_header[2] == '3')) {
		retval.version += 2;
		u8 tagVersion = id3v2_header[3];
		u32 tagsize = ((id3v2_header[9] & 0xFF) | ((id3v2_header[8] & 0xFF) << 7 ) | ((id3v2_header[7] & 0xFF) << 14 ) | ((id3v2_header[6] & 0xFF) << 21 )) + 10;
		bool synchUsed = ((id3v2_header[5] & 0x80) != 0);
		bool extHDRPresent = ((id3v2_header[5] & 0x40) != 0);
		if (extHDRPresent) {
			u8 tmpc[4];
			fread(&tmpc, 4, 1, tmp);
			u32 hdrsize = tmpc[0] << 21 | tmpc[1] << 4 | tmpc[2] << 7 | tmpc[3];
			fseek(tmp, hdrsize-4, SEEK_CUR);
		}
		u8* tagBuffer = (u8*)malloc(tagsize);
		fread(tagBuffer, tagsize, 1, tmp);
		
		if (synchUsed) {
			int npos = 0;
			u8* newBuffer = (u8*)malloc(tagsize);
			for (u32 i=0; i<tagsize; i++) {
				if ((i<tagsize-1) && (tagBuffer[i] == 0xFF) && (tagBuffer[i+1] == 0)) {
					newBuffer[npos++] = 0xFF;
					i++;
					continue;
				}
				newBuffer[npos++] = tagBuffer[i];
			}
			tagBuffer = newBuffer;
		}
		
		u32 pos = 0;
		u32 frameSize = tagVersion < 3 ? 6 : 10;
		
		while (true) {
			u32 remaining = tagsize - pos;
			if (remaining < frameSize) {
				break;
			}
			if (tagBuffer[pos] < 'A' || tagBuffer[pos] > 'Z') {
				break;
			}
			string frameName;
			u32 size;
			if (tagVersion < 3) {
				char *fnPtr = (char*)tagBuffer+pos;
				frameName.assign(fnPtr, 3);
				size = ((tagBuffer[pos+5] << 8) | (tagBuffer[pos+4] << 16) | (tagBuffer[pos+3] << 24));
			} else {
				char *fnPtr = (char*)tagBuffer+pos;
                frameName.assign(fnPtr, 4);
                size = ((tagBuffer[pos+7]) | (tagBuffer[pos+6] << 8) | (tagBuffer[pos+5] << 16) | (tagBuffer[pos+4] << 24));
            }
			
			if ((pos + size) > tagsize) {
				break;
			}
			char *fcPtr = (char*)tagBuffer+pos+frameSize;
			string frameContents(fcPtr, size);
			retval.v2_frames.insert(std::pair<string,string>(frameName,frameContents));
			pos += size + frameSize;
		}		
		free(tagBuffer);
	}
	fclose(tmp);
	return retval;
}

string get_id3_tag(id3 tags, Basic_Tag request) {
	int version = tags.version;
	if (version >= 2) {
		
		map<string,string>::iterator it;
		switch (request) {
			case artist:
				it = tags.v2_frames.find("TPE1");
				if (it != tags.v2_frames.end()) {
					string retval = it->second;
					retval.erase(0, 1);
					return retval;
				}
				it = tags.v2_frames.find("TPE2");
				if (it != tags.v2_frames.end()) {
					string retval = it->second;
					retval.erase(0, 1);
					return retval;
				}
				it = tags.v2_frames.find("TPE3");
				if (it != tags.v2_frames.end()) {
					string retval = it->second;
					retval.erase(0, 1);
					return retval;
				}
				break;
			case album:
				it = tags.v2_frames.find("TALB");
				if (it != tags.v2_frames.end()) {
					string retval = it->second;
					retval.erase(0, 1);
					return retval;
				}
				it = tags.v2_frames.find("TOAL");
				if (it != tags.v2_frames.end()) {
					string retval = it->second;
					retval.erase(0, 1);
					return retval;
				}
				break;
			case title:
				it = tags.v2_frames.find("TIT2");
				if (it != tags.v2_frames.end()) {
					string retval = it->second;
					retval.erase(0, 1);
					return retval;
				}
				it = tags.v2_frames.find("TIT");
				if (it != tags.v2_frames.end()) {
					string retval = it->second;
					retval.erase(0, 1);
					return retval;
				}
				break;
		}
		// at this point we haven't found the necessary tag in ID3v2, so try ID3v1 if it exists.
		if (tags.version == 3) {
			version = 1;
		}
	}
	if (version == 1) {
		switch (request) {
			case artist:
				if (strlen(tags.v1_artist) != 0) 
					return tags.v1_artist;
				break;
			case album:
				if (strlen(tags.v1_album) != 0)
					return tags.v1_album;
				break;
			case title:
				if (strlen(tags.v1_title) != 0)
					return tags.v1_title;
				break;
		}
	}
	return "Unknown";
}

void float_to_s16(float* in, s16* out, int length) {
	for (int i = 0; i<length; i++) {
		out[i] = in[i] * 32767;
	}
}

mp3decoder::mp3decoder(const string& fileName) {
	mutexLock(&this->decodeLock);

	// drmp3 does its own resampling and up/downmixing, so we set it up to use this instead of ours.
	drmp3_config mp3config;
	mp3config.outputChannels = 2;
	mp3config.outputSampleRate = SAMPLERATE;
	this->fileSize = 0;

	// determine filesize for checking length. this will only work on CBR files, but dr_mp3 doesn't have a better way.
	FILE *tmp=fopen(fileName.c_str(), "r");
	if (tmp!=NULL) {
		fseek(tmp, 0, SEEK_END);
		this->fileSize = ftell(tmp);
		fclose(tmp);
	}
	
	// parse ID3 tags
	this->rawid3 = parse_id3_tags(fileName);
	this->metadata.artist = get_id3_tag(this->rawid3, artist);
	this->metadata.album = get_id3_tag(this->rawid3, album);
	this->metadata.title = get_id3_tag(this->rawid3, title);
	
	if (!drmp3_init_file(&this->mp3, fileName.c_str(), &mp3config)) {
		this->decoderValid = false;
		this->decodeRunning = false;
		return;
	}

	this->metadata.out_channels = mp3config.outputChannels;
	this->metadata.out_rate = mp3config.outputSampleRate;
	this->metadata.bit_rate = 0;
	this->metadata.in_rate = 0;
	this->metadata.in_channels = 0;
	this->metadata.current_time = 0;
	this->metadata.length = 0; // can't get this until we get bitrate, which we get with the first frame.
	this->metadata.bit_depth = 16;
	this->numFrames = (this->metadata.out_rate/FRAMERATE);

	this->decodeBufferSize = (this->numFrames * BYTESPERSAMPLE * this->metadata.out_channels);
	this->decodeBuffer = (s16 *) malloc(this->decodeBufferSize);
	this->floatBufferSize = (this->numFrames * this->metadata.out_channels * sizeof(float));
	this->floatBuffer = (float *) malloc(this->floatBufferSize);

	this->decodeRunning = false;
	this->decoderValid = true; // set this to false if any step fails to validate

	mutexUnlock(&this->decodeLock);
}
		
mp3decoder::~mp3decoder() {
	mutexLock(&this->decodeLock);
	this->decodeRunning = false;
	free(this->decodeBuffer);
	free(this->floatBuffer);
	drmp3_uninit(&this->mp3);
	mutexUnlock(&this->decodeLock);
}

void mp3decoder::start() { 
	this->decodeRunning = true;
	threadCreate(&this->decodingThread, mp3decoder_trampoline, this, 0x10000, 0x2D, 3);
	threadStart(&this->decodingThread);
}

void mp3decoder::stop() { 
	mutexLock(&this->decodeStatusLock);
	condvarWait(&this->decodeStatusCV);
	this->decodeRunning = false;
	mutexUnlock(&this->decodeStatusLock);
	threadWaitForExit(&this->decodingThread);
}

long mp3decoder::tell() {
	// this should return the current position in terms of PCM samples.
	return 0;
}

long mp3decoder::tell_time() {
	// this should return the current position in terms of seconds.
	return 0;
}

long mp3decoder::length() {
	// this should return the total length of the file in terms of PCM samples.
	return 0; 
}

long mp3decoder::length_time() {
	// this should return the total length of the file in terms of seconds.
	return 0;
}

int mp3decoder::seek(long position) {
	// this should seek to a certain position in terms of PCM samples.
	return 1;
}

int mp3decoder::seek_time(double time) {
	// this should seek to a certain position in terms of seconds.
	return 1;
}

int mp3decoder::get_bitrate() {
	// this should give a bitrate, or a VBR quality. Numbers 10 and below are 
	// assumed by AudioFile to be some sort of VBR quality and well be treated 
	// accordingly.
	return 0;
}

bool mp3decoder::checkRunning() {
	mutexLock(&this->decodeStatusLock);
	condvarWaitTimeout(&this->decodeStatusCV,100000);
	bool tmp = this->decodeRunning;
	this->Metadata.currtime = floor(this->tell_time());
	mutexUnlock(&this->decodeStatusLock);
	return tmp;
}

void mp3decoder_trampoline(void *parameter) {
	mp3decoder* obj = (mp3decoder*)parameter;
	obj->main_thread(NULL);
}

void mp3decoder::main_thread(void *) {
	mutexLock(&this->decodeStatusLock);
	this->decodeRunning=true;
	bool running=this->decodeRunning;
	
	while (running) {
		condvarWakeAll(&this->decodeStatusCV);
		mutexUnlock(&this->decodeStatusLock);

		mutexLock(&this->decodeLock);
		u64 retframes = drmp3_read_f32(&this->mp3, this->numFrames, this->floatBuffer);
		this->metadata.bit_rate = mp3.frameInfo.bitrate_kbps;
		this->metadata.length = calculate_length(this->metadata.bit_rate, this->fileSize);
		this->metadata.in_rate = mp3.frameInfo.hz;
		this->metadata.in_channels = mp3.frameInfo.channels;
		this->convert_buffer();
		u32 retval = (retframes * BYTESPERSAMPLE * this->metadata.out_channels);
		if (retframes == 0) { // a 0 should mean EOF.
			mutexLock(&this->decodeStatusLock);
			this->decodeRunning = false;
			condvarWakeAll(&this->decodeStatusCV);
			mutexUnlock(&this->decodeStatusLock);
		} else {
			int x = fillPlayBuffer(this->decodeBuffer, retval/2);
			if (x == -1) { // buffer overflow. Wait before trying again.
				while (x == -1) {
					svcSleepThread((FRAMEMS * 1000000)/4);
					x = fillPlayBuffer(this->decodeBuffer, retval/2);
				}
			}
		}
		mutexUnlock(&this->decodeLock);
		
		svcSleepThread(1000); //1000ns sleep just to reduce load on CPU. can be removed if necessary.
		
		mutexLock(&this->decodeStatusLock);
		running = this->decodeRunning;
	}
	condvarWakeAll(&this->decodeStatusCV);
	mutexUnlock(&this->decodeStatusLock);
}


void mp3decoder::convert_buffer() {
	float_to_s16(this->floatBuffer, this->decodeBuffer, (this->floatBufferSize/sizeof(float)));
}

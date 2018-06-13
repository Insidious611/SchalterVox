#ifndef PLAYBACK_H
#define PLAYBACK_H

void start_playback();
void playback_thread_main(void *);
int fillPlayBuffer(int16_t *inBuffer, int length);
void stop_playback(bool playout=true);

#define AUDIO_SAMPLERATE 48000
#define AUDIO_BUFFER_SAMPLES (AUDIO_SAMPLERATE)
#define ATB_SIZE (AUDIO_BUFFER_SAMPLES * 6)

#endif
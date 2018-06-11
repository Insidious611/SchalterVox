#ifndef PLAYBACK_H
#define PLAYBACK_H

static int atbUsed = 0;
static u32 *atb;
static Mutex aLock;

static bool playing=false;
static Thread playback_thread;
static Mutex aStatusLock;
static CondVar aStatusCV;

void start_playback();
void playback_thread_main(void *);
int fillPlayBuffer(int16_t *inBuffer, int length);
void stop_playback(bool playout=true);

#define AUDIO_SAMPLERATE 48000
#define AUDIO_BUFFER_SAMPLES (AUDIO_SAMPLERATE)
#define ATB_SIZE (AUDIO_BUFFER_SAMPLES * 6)

#endif
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>
#include <string>
#include <sstream>
#include "decoder.hpp"

void vorbisdecoder_trampoline(void *parameter);

struct vorbis_tags {
	string header;
	string body;
};

vorbis_tags vorbis_comment_split(const string &s);

class vorbisdecoder : public decoder {
	public:
		vorbisdecoder(const string& fileName);
		~vorbisdecoder();
		void start();
		void stop();
		long tell();
		long tell_time();
		int seek(long position);
		int seek_time(double time);
		bool check_running();
		vorbis_info* info;
		vorbis_comment* comment;
		void main_thread(void *); // This should really be private, but it needs to be public due to the thread trampoline.
		void parse_metadata();
		void update_metadata();
	private:
		OggVorbis_File vorbisFile;
		int section;
		string fn;
};

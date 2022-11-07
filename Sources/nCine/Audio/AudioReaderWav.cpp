#include <cstring>
#include "../IO/IFileStream.h"
#include "AudioReaderWav.h"
#include "AudioLoaderWav.h"

namespace nCine
{
	///////////////////////////////////////////////////////////
	// CONSTRUCTORS and DESTRUCTOR
	///////////////////////////////////////////////////////////

	AudioReaderWav::AudioReaderWav(std::unique_ptr<IFileStream> fileHandle)
		: fileHandle_(std::move(fileHandle))
	{
		ASSERT(fileHandle_->IsOpened());
	}

	///////////////////////////////////////////////////////////
	// PUBLIC FUNCTIONS
	///////////////////////////////////////////////////////////

	unsigned long int AudioReaderWav::read(void* buffer, unsigned long int bufferSize) const
	{
		ASSERT(buffer);
		ASSERT(bufferSize > 0);

		unsigned long int bytes = 0;
		unsigned long int bufferSeek = 0;

		do {
			// Read up to a buffer's worth of decoded sound data
			bytes = fileHandle_->Read(buffer, bufferSize);
			bufferSeek += bytes;
		} while (bytes > 0 && bufferSize - bufferSeek > 0);

		return bufferSeek;
	}

	void AudioReaderWav::rewind() const
	{
		if (fileHandle_->Ptr()) {
			::clearerr(fileHandle_->Ptr());
		}
		fileHandle_->Seek(AudioLoaderWav::HeaderSize, SeekOrigin::Begin);
	}

}

#include "StandardFile.h"

#include <cstdlib>			// for exit()

// All but MSVC: Linux, Android and MinGW.
#if !(defined(DEATH_TARGET_WINDOWS) && !defined(DEATH_TARGET_MINGW))
#	include <sys/stat.h>	// for open()
#	include <fcntl.h>		// for open()
#	include <unistd.h>		// for close()
#else
#	include <io.h>			// for _access()
#endif

#include <Utf8.h>

using namespace Death;

namespace nCine
{
	///////////////////////////////////////////////////////////
	// CONSTRUCTORS and DESTRUCTOR
	///////////////////////////////////////////////////////////

	StandardFile::~StandardFile()
	{
		if (shouldCloseOnDestruction_) {
			Close();
		}
	}

	///////////////////////////////////////////////////////////
	// PUBLIC FUNCTIONS
	///////////////////////////////////////////////////////////

	void StandardFile::Open(FileAccessMode mode)
	{
		// Checking if the file is already opened
		if (fileDescriptor_ >= 0 || filePointer_ != nullptr) {
			LOGW_X("File \"%s\" is already opened", filename_.data());
		} else {
#if !(defined(DEATH_TARGET_WINDOWS) && !defined(DEATH_TARGET_MINGW))
			if ((mode & FileAccessMode::FileDescriptor) == FileAccessMode::FileDescriptor) {
				// Opening with a file descriptor
				OpenFD(mode);
			} else
#endif
			{
				// Opening with a file stream
				OpenStream(mode);
			}
		}
	}

	/*! This method will close a file both normally opened or fopened */
	void StandardFile::Close()
	{
		if (fileDescriptor_ >= 0) {
#if !(defined(DEATH_TARGET_WINDOWS) && !defined(DEATH_TARGET_MINGW))
			const int retValue = ::close(fileDescriptor_);
			if (retValue < 0) {
				LOGW_X("Cannot close the file \"%s\"", filename_.data());
			} else {
				LOGI_X("File \"%s\" closed", filename_.data());
				fileDescriptor_ = -1;
			}
#endif
		} else if (filePointer_) {
			const int retValue = ::fclose(filePointer_);
			if (retValue == EOF) {
				LOGW_X("Cannot close the file \"%s\"", filename_.data());
			} else {
				LOGI_X("File \"%s\" closed", filename_.data());
				filePointer_ = nullptr;
			}
		}
	}

	int32_t StandardFile::Seek(int32_t offset, SeekOrigin origin) const
	{
		int32_t seekValue = -1;

		if (fileDescriptor_ >= 0) {
#if !(defined(DEATH_TARGET_WINDOWS) && !defined(DEATH_TARGET_MINGW))
			seekValue = ::lseek(fileDescriptor_, offset, (int)origin);
#endif
		} else if (filePointer_) {
			seekValue = ::fseek(filePointer_, offset, (int)origin);
		}
		return seekValue;
	}

	int32_t StandardFile::GetPosition() const
	{
		int32_t tellValue = -1;

		if (fileDescriptor_ >= 0) {
#if !(defined(DEATH_TARGET_WINDOWS) && !defined(DEATH_TARGET_MINGW))
			tellValue = ::lseek(fileDescriptor_, 0L, SEEK_CUR);
#endif
		} else if (filePointer_) {
			tellValue = ::ftell(filePointer_);
		}
		return tellValue;
	}

	uint32_t StandardFile::Read(void* buffer, uint32_t bytes) const
	{
		ASSERT(buffer);

		uint32_t bytesRead = 0;

		if (fileDescriptor_ >= 0) {
#if !(defined(DEATH_TARGET_WINDOWS) && !defined(DEATH_TARGET_MINGW))
			bytesRead = ::read(fileDescriptor_, buffer, bytes);
#endif
		} else if (filePointer_) {
			bytesRead = static_cast<uint32_t>(::fread(buffer, 1, bytes, filePointer_));
		}
		return bytesRead;
	}

	uint32_t StandardFile::Write(const void* buffer, uint32_t bytes)
	{
		ASSERT(buffer);

		uint32_t bytesWritten = 0;

		if (fileDescriptor_ >= 0) {
#if !(defined(DEATH_TARGET_WINDOWS) && !defined(DEATH_TARGET_MINGW))
			bytesWritten = ::write(fileDescriptor_, buffer, bytes);
#endif
		} else if (filePointer_) {
			bytesWritten = static_cast<uint32_t>(::fwrite(buffer, 1, bytes, filePointer_));
		}
		return bytesWritten;
	}

	///////////////////////////////////////////////////////////
	// PRIVATE FUNCTIONS
	///////////////////////////////////////////////////////////

#if !(defined(DEATH_TARGET_WINDOWS) && !defined(DEATH_TARGET_MINGW))
	void StandardFile::OpenFD(FileAccessMode mode)
	{
		int openFlag = -1;

		switch (mode) {
			case (FileAccessMode::FileDescriptor | FileAccessMode::Read):
				openFlag = O_RDONLY;
				break;
			case (FileAccessMode::FileDescriptor | FileAccessMode::Write):
				openFlag = O_WRONLY;
				break;
			case (FileAccessMode::FileDescriptor | FileAccessMode::Read | FileAccessMode::Write):
				openFlag = O_RDWR;
				break;
			default:
				LOGE_X("Cannot open the file \"%s\", wrong open mode", filename_.data());
				break;
		}

		if (openFlag >= 0) {
			fileDescriptor_ = ::open(filename_.data(), openFlag);

			if (fileDescriptor_ < 0) {
				LOGE_X("Cannot open the file \"%s\"", filename_.data());
				return;
			}

			LOGI_X("File \"%s\" opened", filename_.data());

			// Calculating file size
			fileSize_ = ::lseek(fileDescriptor_, 0L, SEEK_END);
			::lseek(fileDescriptor_, 0L, SEEK_SET);
		}
	}
#endif

	void StandardFile::OpenStream(FileAccessMode mode)
	{
#if defined(DEATH_TARGET_WINDOWS) && !defined(DEATH_TARGET_MINGW)
		const wchar_t* modeInternal;
		switch (mode) {
			case FileAccessMode::Read: modeInternal = L"rb"; break;
			case FileAccessMode::Write: modeInternal = L"wb"; break;
			case FileAccessMode::Read | FileAccessMode::Write: modeInternal = L"r+b"; break;
			default:
				LOGE_X("Cannot open the file \"%s\", wrong open mode", filename_.data());
				return;
		}

		_wfopen_s(&filePointer_, Utf8::ToUtf16(filename_), modeInternal);
#else
		const char* modeInternal;
		switch (mode) {
			case FileAccessMode::Read: modeInternal = "rb"; break;
			case FileAccessMode::Write: modeInternal = "wb"; break;
			case FileAccessMode::Read | FileAccessMode::Write: modeInternal = "r+b"; break;
			default:
				LOGE_X("Cannot open the file \"%s\", wrong open mode", filename_.data());
				return;
		}

		filePointer_ = ::fopen(filename_.data(), modeInternal);
#endif

		if (filePointer_ == nullptr) {
			LOGE_X("Cannot open the file \"%s\"", filename_.data());
			return;
		}

		LOGI_X("File \"%s\" opened", filename_.data());

		// Calculating file size
		::fseek(filePointer_, 0L, SEEK_END);
		fileSize_ = ::ftell(filePointer_);
		::fseek(filePointer_, 0L, SEEK_SET);
	}
}

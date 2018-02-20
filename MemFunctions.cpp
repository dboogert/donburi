#include <dlfcn.h>
#include "MemFunctions.h"


namespace donburi
{
	void Stack::get()
	{
		f = 0;
		unw_context_t context;
		unw_getcontext(&context);

		unw_cursor_t cursor;
		unw_init_local(&cursor, &context);

		// go up one frame so we don't see the allocation capture function
		// in the call stack
		unw_step(&cursor);

		while (unw_step(&cursor) && f < maxStackDepth)
		{
			unw_get_reg(&cursor, UNW_REG_IP, &ip[f]);
			f++;
		}
	}

	void Stack::print() const
	{
		for (int i = 0; i < f; ++i)
		{
			Dl_info info;
			if (dladdr((void *) ip[i], &info))
			{
				if (info.dli_fname)
				{
					std::cout << "\t [" << info.dli_fname << "]";
				}

				if (info.dli_sname)
				{
					std::cout << " [" << info.dli_sname << "]";
				}

				std::cout << " " << std::hex << std::setw(16) << std::setfill('0') << (void *) ip[i];

				std::cout << std::endl;
			}
		}
	}

	void Stack::write(int fd)
	{
		::write(fd, &f, sizeof(f));
		for (size_t i = 0; i < f; ++i)
		{
			unsigned int p = (unsigned int) (ip[i]);
			::write(fd, &p, sizeof(p));
		}
	}

	void MemOp::write(int fd)
	{
		::write(fd, &size, sizeof(size));
		::write(fd, &ptr, sizeof(ptr) );
		unsigned char tmp = allocation ? 0xAA : 0xFF;
		::write(fd, &tmp, sizeof(tmp));

		stack.write(fd);
	}


	struct ScopedLock
	{
		ScopedLock(pthread_mutex_t *mutex)
				: mutex(mutex)
		{
			pthread_mutex_lock(mutex);
		}

		~ScopedLock()
		{
			pthread_mutex_unlock(mutex);
		}

		pthread_mutex_t *mutex;
	};

	struct RecursiveLock
	{
		RecursiveLock(bool &logging)
				: logging(logging)
		{
			logging = true;
		}

		~RecursiveLock()
		{
			logging = false;
		}

		bool &logging;
	};

	MemOpBuffer::MemOpBuffer() : index(0), totalMemOps(0), fd(-1), logging(false)
	{
		pthread_mutexattr_init(&mutexAttr);
		pthread_mutexattr_settype(&mutexAttr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&mutex, &mutexAttr);

		const char *output = getenv("DONBURI_OUTPUTFILE");
		if (output)
		{
			std::cout << "[donburi] writing output: " << output << std::endl;
			strcpy(outputFile, output);

			mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
			fd = open(outputFile, O_WRONLY | O_TRUNC | O_CREAT, mode);


			// Write 4 bytes "BURI"
			char magic[4] = {'B', 'U', 'R', 'I'};
			::write(fd, magic, sizeof(char) * 4);

			// Write offset to where we're going to write the frame names
			size_t offsetToStrings = 0;
			::write(fd, &offsetToStrings, sizeof(offsetToStrings));

			size_t numMemOps = 0;
			::write(fd, &numMemOps, sizeof(numMemOps));
		}
	}

	MemOpBuffer::~MemOpBuffer()
	{
		RecursiveLock recursiveLock(logging);

		flush();
		finalise();
		close(fd);
	}

	void MemOpBuffer::flush()
	{
		std::cout << "[donburi] flushing mem ops:" << index << std::endl;
		for (size_t i = 0; i < index; ++i)
		{
			for (size_t f = 0; f < ops[i].stack.f; ++f)
			{
				ops[i].stack.ip[f] = addressToIndex(ops[i].stack.ip[f]);
			}

			ops[i].write(fd);
		}

		totalMemOps += index;

		index = 0;
	}

	void MemOpBuffer::finalise()
	{
		const StringTable &strings = getStringTable();
		std::cout << "[donburi] writing string table: " << strings.size() << std::endl;

		char   buffer[256];
		size_t numStrings          = strings.size();

		// get file offset to string table
		off_t stringsOffset = lseek(fd, 0, SEEK_CUR);

		::write(fd, &numStrings, sizeof(size_t));

		// todo write offsets for all the strings in a table
		for (size_t i = 0; i < numStrings; ++i)
		{
			size_t length = strings[i].length();
			::write(fd, &length, sizeof(length));
			::write(fd, strings[i].c_str(), length);
			std::cout << "[donburi]		" << i << " - '" << strings[i] << "'" << std::endl;
		}

		lseek(fd, 4, SEEK_SET);
		::write(fd, &stringsOffset, sizeof(size_t));
		::write(fd, &totalMemOps, sizeof(size_t));

	}

	void MemOpBuffer::allocation(size_t size, void *ptr)
	{
		ScopedLock l(&mutex);
		if (logging)
		{
			return;
		}
		RecursiveLock r(logging);

		ops[index].stack.get();
		ops[index].size       = size;
		ops[index].allocation = true;
		ops[index].ptr        = ptr;

		index++;


		if (index == maxMemOps)
		{
			flush();
		}
	}

	void MemOpBuffer::free(void *ptr)
	{
		ScopedLock l(&mutex);
		if (logging)
		{
			return;
		}
		RecursiveLock r(logging);

		ops[index].stack.get();
		ops[index].size       = 0;
		ops[index].allocation = false;
		ops[index].ptr        = ptr;

		index++;

		if (index == maxMemOps)
		{
			flush();
		}
	}

	size_t MemOpBuffer::addressToIndex(unw_word_t address)
	{
		auto &ptrMap = getPtrMap();
		auto it      = ptrMap.find(address);
		if (it != ptrMap.end())
		{
			return it->second;
		}

		size_t newStringIndex = getStringTable().size();

		std::string functionName = "unknown";

		Dl_info info;
		if (dladdr((void *) address, &info))
		{
			if (info.dli_sname)
			{
				functionName = info.dli_sname;
			}
		}

		auto itCache = getStringCache().find(functionName);

		if (itCache != getStringCache().end())
		{
			return itCache->second;
		}

		getStringTable().push_back(functionName);
		ptrMap[address]                = newStringIndex;
		getStringCache()[functionName] = newStringIndex;
		return newStringIndex;
	}

	MemOpBuffer &GetMemOps()
	{
		static MemOpBuffer gMemOpBuffer;
		return gMemOpBuffer;
	}

}




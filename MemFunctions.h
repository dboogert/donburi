#include <stdlib.h>
#include <dlfcn.h>

#include <iostream>
#include <iomanip>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <libunwind.h>
#include <pthread.h>
#include <unordered_map>
#include <string>
#include <vector>

class Stack
{
public:
	static const int maxStackDepth = 128;
	unw_word_t ip[maxStackDepth];
	int f;

	void get();
	void print() const;
	void write(int fd);
};

class MemOp
{
public:
	Stack stack;
	size_t size;
	void* ptr;
	bool allocation;

	void write(int fd);
};

class MemOpBuffer
{
public:
	MemOpBuffer();
	~MemOpBuffer();

	void flush();
	void finalise();

	void allocation(size_t size, void* ptr);
	void free(void* ptr);

	pthread_mutex_t mutex;
	pthread_mutexattr_t mutexAttr;

	static const int maxMemOps = 1024 * 256;
	static const int maxProfileFile = 1024;
	int index;
	MemOp ops[maxMemOps];
	char outputFile[maxProfileFile];
	int fd;
	bool logging;

	typedef std::unordered_map<unw_word_t, size_t> PtrMap;
	typedef std::vector<std::string> StringTable;
	typedef std::unordered_map<std::string, size_t> StringCache;

	PtrMap &getPtrMap()
	{
		if (!m_ptrToName)
		{
			m_ptrToName = new PtrMap();
		}

		return *m_ptrToName;
	}

	StringTable &getStringTable()
	{
		if (!m_strings)
		{
			m_strings = new StringTable();
		}
		return *m_strings;
	}


	StringCache &getStringCache()
	{
		if (!m_stringCache)
		{
			m_stringCache = new StringCache;
		}
		return *m_stringCache;
	}


	unw_word_t addressToIndex(unw_word_t address);

	PtrMap *m_ptrToName;
	StringTable *m_strings;
	StringCache *m_stringCache;
};

MemOpBuffer &GetMemOps();


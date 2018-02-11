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

struct Stack
{
	static const int maxStackDepth = 64;
    unw_word_t ip[maxStackDepth];
    int f;

    void get()
    {
        f = 0;
        unw_context_t context;
        unw_getcontext( &context );
        
        unw_cursor_t cursor;
        unw_init_local(&cursor, &context);

        while( unw_step( &cursor ) && f < maxStackDepth)
        {
            unw_get_reg( &cursor, UNW_REG_IP, &ip[f] );
            f++;
        }
    }

    void print() const
    {
        for (int i = 0; i < f; ++i)
        {
            Dl_info info;
            if ( dladdr( (void*) ip[i], &info) )
            {
                if (info.dli_fname)
                {
                    std::cout << "\t [" << info.dli_fname << "]";
                }

                if (info.dli_sname)
                {
                    std::cout << " [" << info.dli_sname << "]";
                }

                std::cout << " " << std::hex << std::setw(16) << std::setfill('0') << (void*) ip[i];

                std::cout << std::endl;
            }    
        }    
    }

	void write(int fd)
	{
		::write(fd, &f, sizeof(f) );
		for (size_t i = 0; i < f; ++i)
		{
			::write(fd, &ip[i], sizeof(ip[i]));
		}
	}
};

struct MemOp
{
    Stack stack;
    size_t size;
    void* ptr;
    bool allocation;

	void write(int fd)
	{
		::write(fd, &size, sizeof(size) );
		::write(fd, &ptr, sizeof(ptr) );
		char tmp = allocation;
		::write(fd, &tmp, sizeof(tmp));

		stack.write(fd);
	}
};

struct MemOpBuffer
{
	struct ScopedLock
	{
		ScopedLock(pthread_mutex_t* mutex)
		: mutex(mutex)
		{
			pthread_mutex_lock( mutex );
		}

		~ScopedLock()
		{
			pthread_mutex_unlock( mutex );
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

    MemOpBuffer() : index( 0 ), fd(-1), logging(false)
	{
		pthread_mutexattr_init(&mutexAttr);
		pthread_mutexattr_settype(&mutexAttr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&mutex, &mutexAttr);

		const char* output = getenv("DONBURI_OUTPUTFILE");
		if ( output )
		{
			strcpy(outputFile, output);

			mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
			fd = open(outputFile, O_WRONLY | O_TRUNC | O_CREAT, mode);
		}
	}
    ~MemOpBuffer()
    {
		flush();
		close(fd);
    }

	void flush()
	{
		for (size_t i = 0; i < index; ++i)
		{
			ops[i].write( fd );
		}

		index = 0;
	}

    void allocation(size_t size, void* ptr)
    {
		ScopedLock l (&mutex);
		if (logging)
		{
			return;
		}
		RecursiveLock r(logging);

        ops[index].stack.get();
        ops[index].size = size;
        ops[index].allocation = true;
        ops[index].ptr = ptr;

		index++;

		if (index == maxMemOps)
		{
			flush();
		}
	}

    void free(void* ptr)
    {
		ScopedLock l (&mutex);
		if (logging)
		{
			return;
		}
		RecursiveLock r(logging);

        ops[index].stack.get();
        ops[index].size = 0;
        ops[index].allocation = false;
        ops[index].ptr = ptr;

        index++;

		if (index == maxMemOps)
		{
			flush();
		}

    }

	pthread_mutex_t mutex;
	pthread_mutexattr_t mutexAttr;


	static const int maxMemOps = 1024 * 256;
	static const int maxProfileFile = 1024;
	int index;
    MemOp ops[maxMemOps];
	char outputFile[maxProfileFile];
	int fd;
	bool logging;
};

struct MallocFunctions
{
	typedef void *(*fn_malloc)(size_t);
	typedef void *(*fn_free)(void *);

	fn_malloc _malloc;
	fn_free _free;

	MallocFunctions()
	{
		_malloc = (fn_malloc) dlsym(RTLD_NEXT, "malloc");
		_free = (fn_free) dlsym(RTLD_NEXT,"free");
	}
};

MemOpBuffer gMemOpBuffer;
MallocFunctions gMallocFunctions;


// lock access to MemOpBuffer [DONE]
// Serialise to profile file (environment variable for file) [DONE]
// guard against re-entry into malloc for a given thread so malloc & free work inside the instrumented malloc [DONE]
// cache address of old malloc & free only once & not on every call. [DONE]
// generate symbol from address only once [DONE]

// Insert marker API
// Write test case for threading
// split malloc preload from memory logging .cpp files
// append pid to logging file
// add a define / variable for memOp buffer size
// add a define / variable for stack depth

extern "C"
{
    void *malloc(size_t size)
    {
		void* ptr = gMallocFunctions._malloc(size);
        gMemOpBuffer.allocation(size, ptr);
        return ptr;
    }

    void free(void *ptr)
    {
		gMemOpBuffer.free(ptr);
		gMallocFunctions._free(ptr);
    }
}
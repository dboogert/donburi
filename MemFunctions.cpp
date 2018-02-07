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

typedef void* (*fn_malloc)(size_t);
typedef void* (*fn_free)(void*);

struct Stack
{
    unw_word_t ip[16];
    int f;

    void get()
    {
        f = 0;
        unw_context_t context;
        unw_getcontext( &context );
        
        unw_cursor_t cursor;
        unw_init_local(&cursor, &context);

        while( unw_step( &cursor ) && f < 16)
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
};

struct MemOp
{
    Stack stack;
    size_t size;
    void* ptr;
    bool malloc;
};

struct MemOpBuffer
{
    MemOpBuffer() : index( 0 ), fd(-1)
	{
		const char* output = getenv("DONBURI_OUTPUTFILE");
		if ( output )
		{
			strcpy(outputFile, output);

			mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
			fd = open(outputFile, O_WRONLY | O_TRUNC | O_CREAT, mode);
//			std::cout << fd << std::endl;
		}
	}
    ~MemOpBuffer()
    {
		flush();
		close(fd);

//        for (int i = 0; i < index; ++i)
//        {
//            const MemOp &op = ops[i];
//
//            if (op.malloc)
//            {
//                std::cout << "MALLOC: " <<  std::dec << op.size;
//                std::cout << " " << std::hex << std::setw(16) << std::setfill('0') <<  op.ptr;
//                std::cout << std::endl;
//
//            }
//            else
//            {
//                std::cout << "FREE ";
//                std::cout << " " << std::hex << std::setw(16) << std::setfill('0') <<  op.ptr;
//                std::cout << std::endl;
//            }
//
//            op.stack.print();
//        }
    }

	void flush()
	{
		write(fd, &ops[0], sizeof(MemOp) * index );
		index = 0;
	}

    void allocation(size_t size, void* ptr)
    {

        ops[index].stack.get();
        ops[index].size = size;
        ops[index].malloc = true;
        ops[index].ptr = ptr;

		index++;

		if (index == 1024 * 1024)
		{
			flush();
		}

    }

    void free(void* ptr)
    {
        ops[index].stack.get();
        ops[index].size = 0;
        ops[index].malloc = false;
        ops[index].ptr = ptr;

        index++;

		if (index == 1024 * 1024)
		{
			flush();
		}
    }

    int index;
    MemOp ops[1024 * 1024];
	char outputFile[1024];
	int fd;
};

MemOpBuffer gMemOpBuffer;
pthread_mutex_t gMutex;

// 1. lock access to MemOpBuffer [DONE]
// 2. Serialise to profile file (environment variable for file)
// 3. Insert marker API
// 4. generate symbol from address only once
// 5. guard against re-entry into malloc for a given thread so malloc & free work inside the instrumented malloc
// 6. Write test case for threading
// 7. cache address of old malloc & free

extern "C"
{
    void *malloc(size_t size)
    {
        fn_malloc o_malloc;
        o_malloc = (fn_malloc) dlsym(RTLD_NEXT, "malloc");
        void* ptr = o_malloc(size);

		pthread_mutex_lock( &gMutex );
        gMemOpBuffer.allocation(size, ptr);
		pthread_mutex_unlock( &gMutex );
        return ptr;
    }

    void free(void *ptr)
    {
		pthread_mutex_lock( &gMutex );
        gMemOpBuffer.free(ptr);
		pthread_mutex_unlock( &gMutex );

        fn_free o_free;
        o_free = (fn_free) dlsym(RTLD_NEXT,"free");
        o_free(ptr);
    }
}
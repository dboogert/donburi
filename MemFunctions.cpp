#include <stdlib.h>
#include <dlfcn.h>

#include <iostream>
#include <iomanip>

#include <libunwind.h>
 
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

        while( unw_step( &cursor ) && f < 16) //
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
    MemOpBuffer() : index( 0 ) {}
    ~MemOpBuffer()
    {
        for (int i = 0; i < index; ++i)
        {
            const MemOp &op = ops[i];

            if (op.malloc)
            {
                std::cout << "MALLOC: " <<  std::dec << op.size;
                std::cout << " " << std::hex << std::setw(16) << std::setfill('0') <<  op.ptr;
                std::cout << std::endl;

            }
            else
            {
                std::cout << "FREE ";
                std::cout << " " << std::hex << std::setw(16) << std::setfill('0') <<  op.ptr;
                std::cout << std::endl;
            }
            
            op.stack.print();
        }
    }

    void allocation(size_t size, void* ptr)
    {
        ops[index].stack.get();
        ops[index].size = size;
        ops[index].malloc = true;
        ops[index].ptr = ptr;

        index++;
    }

    void free(void* ptr)
    {
        ops[index].stack.get();
        ops[index].size = 0;
        ops[index].malloc = false;
        ops[index].ptr = ptr;

        index++;
    }

    int index;
    MemOp ops[1024 * 1024];
};

MemOpBuffer gMemOpBuffer;

extern "C"
{
    void *malloc(size_t size)
    {
        fn_malloc o_malloc;
        o_malloc = (fn_malloc) dlsym(RTLD_NEXT, "malloc");
        void* ptr = o_malloc(size);
        
        gMemOpBuffer.allocation(size, ptr);

        return ptr;
    }

    void free(void *ptr)
    {
        gMemOpBuffer.free(ptr);

        fn_free o_free;
        o_free = (fn_free) dlsym(RTLD_NEXT,"free");
        o_free(ptr);
    }
}
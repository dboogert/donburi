// Insert marker API
// Write test case for threading
// append pid to logging file
// write string table information

#include "MemFunctions.h"

namespace
{
	struct MallocFunctions
	{
		typedef void *(*fn_malloc)(size_t);
		typedef void *(*fn_free)(void *);

		fn_malloc _malloc;
		fn_free _free;

		MallocFunctions()
		{
			std::cout << "[donburi] locating memory functions" << std::endl;
			_malloc = (fn_malloc) dlsym(RTLD_NEXT, "malloc");
			_free = (fn_free) dlsym(RTLD_NEXT,"free");
		}
	};

	MallocFunctions& getMallocFunctions()
	{
		static MallocFunctions gMallocFunctions;
		return gMallocFunctions;
	}
}

extern "C"
{
void *malloc(size_t size)
{
	void* ptr = getMallocFunctions()._malloc(size);

	donburi::GetMemOps().allocation(size, ptr);
	return ptr;
}

void free(void *ptr)
{
	donburi::GetMemOps().free(ptr);
	getMallocFunctions()._free(ptr);
}
}

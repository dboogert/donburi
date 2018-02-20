#include "LogReader.h"

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>

namespace donburi
{
	LogReader::LogReader(const std::string &logPath)
	{
		mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
		m_fd = open(logPath.c_str(), O_RDONLY, mode);

		char magic[4];
		::read(m_fd, magic, 4);
		std::cout << m_fd << std::endl;
		if (magic[0]!='B' && magic[1]!='U' && magic[2]!='R' && magic[3]!='I')
		{
			return;
		}

		size_t offsetToStrings;
		::read(m_fd, &offsetToStrings, sizeof(offsetToStrings));

		::read(m_fd, &m_numOps, sizeof(m_numOps));

		// read the string table
		::lseek(m_fd, offsetToStrings,  SEEK_SET);

		size_t numStrings;

		::read(m_fd, &numStrings, sizeof(numStrings));
		std::cout << numStrings << std::endl;

		for (size_t i = 0; i < numStrings; ++i)
		{
			size_t stringLength;
			::read(m_fd, &stringLength, sizeof(stringLength));
			std::cout << stringLength << std::endl;

			char* buffer = (char*) malloc(stringLength + 1);
			::read(m_fd, buffer, stringLength);

			buffer[stringLength] = 0;
			m_strings.push_back(buffer);
			free(buffer);
		}

		for (size_t i = 0; i < m_strings.size(); ++i)
		{
			std::cout << m_strings[i] << std::endl;
		}

		lseek(m_fd, sizeof(size_t) * 2 + sizeof(char) * 4, SEEK_SET);
	}

	MemOp LogReader::readOp()
	{
		MemOp op;

		::read(m_fd, &op.size, sizeof(op.size));
		::read(m_fd, &op.ptr, sizeof(op.ptr) );
		unsigned char tmp ;
		::read(m_fd, &tmp, sizeof(tmp));

		op.allocation = tmp == 0xAA;

		int stackSize;
		::read(m_fd, &stackSize, sizeof(stackSize) );

		for (int i = 0 ; i <stackSize; ++i)
		{
			unsigned int f;
			::read(m_fd, &f, sizeof(f));
			op.stack.frames.push_back(f);
		}
		return op;
	}

	size_t LogReader::numOps() const
	{
		return m_numOps;
	}
}
#pragma once

#include <vector>
#include <string>

namespace donburi
{
	class Stack
	{
	public:
		std::vector<size_t> frames;
	};

	class MemOp
	{
	public:
		Stack stack;
		size_t size;
		void* ptr;
		bool allocation;
	};

	class LogReader
	{
	public:
		LogReader(const std::string &logPath);

		MemOp readOp();
		size_t numOps() const;
		const std::vector<std::string>& getStrings() const { return m_strings; }

	private:
		std::vector<std::string> m_strings;
		int m_fd;

		size_t m_numOps;
	};
}
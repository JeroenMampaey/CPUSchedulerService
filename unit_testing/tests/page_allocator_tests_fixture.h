#pragma once

#include <gtest/gtest.h>
#include "../../operating_system/paging/page_allocator.h"
#include <vector>
#include <utility>

struct RawPageDirectory{
    unsigned int pageDirectoryEntries[1024];
    unsigned int helperTableEntries[1024];
} __attribute__((packed)) __attribute__((aligned(4096)));

struct RawPageTable{
    unsigned int pageTableEntries[1024];
}__attribute__((packed)) __attribute__((aligned(4096)));

class TestPagingStructureAccessor : public PagingStructureAccessor{
    public:
        TestPagingStructureAccessor();

        PageDirectory* getPageDirectory();

        PageTable* getPageTable(unsigned int i);

        RawPageDirectory pageDirectory;
        RawPageTable pageTables[1024];
};

class PageAllocatorTests : public ::testing::Test {
    public:
        static TestPagingStructureAccessor* pTestAccessor;

    protected:
        PageAllocator* pPageAllocator = nullptr;

        static void SetUpTestCase();
	    
        static void TearDownTestCase();

        virtual void SetUp();

        virtual void TearDown();

        void checkPageDirectoryCorrectness();

        void checkAllocatedRegionsCorrectness(std::vector<std::pair<unsigned int, unsigned int>> allocatedRegions);
};
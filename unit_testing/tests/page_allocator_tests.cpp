#include "page_allocator_tests_fixture.h"

TEST_F(PageAllocatorTests, AllocatePagesBasicFunctionality_ShouldWork) {
    unsigned int ptr = pPageAllocator->allocateContiguousPages(10);
    std::vector<std::pair<unsigned int, unsigned int>> allocatedRegions = {
        {0, 9}
    };
    checkAllocatedRegionsCorrectness(allocatedRegions);
    checkPageDirectoryCorrectness();

    pPageAllocator->freeContiguousPages(ptr);
    allocatedRegions = {};
    checkAllocatedRegionsCorrectness(allocatedRegions);
    checkPageDirectoryCorrectness();
}

TEST_F(PageAllocatorTests, AllocationsComplexScenario_ShouldWork) {
    unsigned int pointer0 = 0;
    pPageAllocator->allocatePageRange(0, 1024*8-1);
    unsigned int pointer1 = pPageAllocator->allocateContiguousPages(100);
    unsigned int pointer2 = pPageAllocator->allocateContiguousPages(1500);
    unsigned int pointer3 = pPageAllocator->allocateContiguousPages(6000);
    unsigned int pointer4 = pPageAllocator->allocateContiguousPages(1);
    unsigned int pointer5 = pPageAllocator->allocateContiguousPages(0);
    std::vector<std::pair<unsigned int, unsigned int>> allocatedRegions = {
        {0, 1024*8-1},
        {1024*8, 1024*8+99},
        {1024*8+100, 1024*8+1599},
        {1024*8+1600, 1024*8+7599},
        {1024*8+7600, 1024*8+7600}
    };
    checkAllocatedRegionsCorrectness(allocatedRegions);
    checkPageDirectoryCorrectness();
    ASSERT_EQ(pointer5, ((unsigned int)(1024*1023))*((unsigned int)(0x1000)));

    pPageAllocator->freeContiguousPages(pointer5);
    pPageAllocator->freeContiguousPages(pointer1);
    pPageAllocator->freeContiguousPages(pointer3);
    unsigned int pointer6 = pPageAllocator->allocateContiguousPages(3000);
    unsigned int pointer7 = pPageAllocator->allocateContiguousPages(1500);
    unsigned int pointer8 = pPageAllocator->allocateContiguousPages(1000);
    pPageAllocator->freeContiguousPages(pointer7);
    unsigned int pointer9 = pPageAllocator->allocateContiguousPages(2000);
    unsigned int pointer10 = pPageAllocator->allocateContiguousPages(1500);
    allocatedRegions = {
        {0, 1024*8-1},
        {1024*8+100, 1024*8+1599},
        {1024*8+1600, 1024*8+4599},
        {1024*8+4600, 1024*8+6099},
        {1024*8+6100, 1024*8+7099},
        {1024*8+7600, 1024*8+7600},
        {1024*8+7601, 1024*8+9600}
    };
    checkAllocatedRegionsCorrectness(allocatedRegions);
    checkPageDirectoryCorrectness();

    pPageAllocator->freeContiguousPages(pointer0);
    pPageAllocator->freeContiguousPages(pointer2);
    pPageAllocator->freeContiguousPages(pointer4);
    pPageAllocator->freeContiguousPages(pointer6);
    pPageAllocator->freeContiguousPages(pointer8);
    pPageAllocator->freeContiguousPages(pointer9);
    pPageAllocator->freeContiguousPages(pointer10);
    allocatedRegions = {};
    checkAllocatedRegionsCorrectness(allocatedRegions);
    checkPageDirectoryCorrectness();

    unsigned int pointer11 = pPageAllocator->allocateContiguousPages(1024*1000);
    unsigned int pointer12 = pPageAllocator->allocateContiguousPages(1024*100);
    allocatedRegions = {
        {0, 1024*1000-1}
    };
    checkAllocatedRegionsCorrectness(allocatedRegions);
    checkPageDirectoryCorrectness();
    ASSERT_EQ(pointer12, ((unsigned int)(1024*1023))*((unsigned int)(0x1000)));

    pPageAllocator->freeContiguousPages(pointer11);
    pPageAllocator->freeContiguousPages(pointer12);
    allocatedRegions = {};
    checkAllocatedRegionsCorrectness(allocatedRegions);
    checkPageDirectoryCorrectness();

    //TODO: honestly, a lot more cases should be tested
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
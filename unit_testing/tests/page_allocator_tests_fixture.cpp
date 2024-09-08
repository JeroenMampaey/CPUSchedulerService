#include "page_allocator_tests_fixture.h"

TestPagingStructureAccessor* PageAllocatorTests::pTestAccessor = nullptr;

// Mocking the getCr0 function
extern "C" void getCr0(unsigned int* returnValue){
    // Act as if paging is disabled
    *returnValue = 0;
}

TestPagingStructureAccessor::TestPagingStructureAccessor(){
    new(&pageDirectory) PageDirectory();
    for(int i=0; i<1024; i++){
        new(&pageTables[i]) PageTable();
    }
}

PageTable* TestPagingStructureAccessor::getPageTable(unsigned int i){
    return (PageTable*)&pageTables[i];
}

PageDirectory* TestPagingStructureAccessor::getPageDirectory(){
    return (PageDirectory*)&pageDirectory;
}

void PageAllocatorTests::SetUpTestCase(){
    pTestAccessor = new TestPagingStructureAccessor();
}

void PageAllocatorTests::TearDownTestCase(){
    delete pTestAccessor;
}

void PageAllocatorTests::SetUp(){
    pPageAllocator = new PageAllocator(pTestAccessor);
}

void PageAllocatorTests::TearDown(){
    delete pPageAllocator;
}

void PageAllocatorTests::checkPageDirectoryCorrectness(){
    for(int i=0; i<1023; i++){
        bool noPagesAllocated = true;
        bool allPagesAllocated = true;
        for(int j=0; j<1024; j++){
            int ptFullnessCode = (PageAllocatorTests::pTestAccessor->pageTables[i].pageTableEntries[j] >> 9) & 0x3;
            
            if(ptFullnessCode==0){
                allPagesAllocated = false;
            }
            else{
                noPagesAllocated = false;
            }
        }

        int pdFullnesCode = (PageAllocatorTests::pTestAccessor->pageDirectory.pageDirectoryEntries[i] >> 9) & 0x3;
        
        if(noPagesAllocated && pdFullnesCode!=0){
            ASSERT_TRUE(false) << "noPagesAllocated && pdFullnesCode!=0";
        }
        
        if(allPagesAllocated && pdFullnesCode!=2){
            ASSERT_TRUE(false) << "allPagesAllocated && pdFullnesCode!=2";
        }

        if(!noPagesAllocated && !allPagesAllocated && pdFullnesCode!=1){
            ASSERT_TRUE(false) << "!noPagesAllocated && !allPagesAllocated && pdFullnesCode!=1";
        }
    }
}

void PageAllocatorTests::checkAllocatedRegionsCorrectness(std::vector<std::pair<unsigned int, unsigned int>> allocatedRegions){
    int lastFullnessCode = 0;
    int nextExpectedAllocatedRegion = 0;
    unsigned int lastRegionBegin = 0;
    for(int i=0; i<1023; i++){
        for(int j=0; j<1024; j++){
            int ptFullnessCode = (PageAllocatorTests::pTestAccessor->pageTables[i].pageTableEntries[j] >> 9) & 0x3;
            if(ptFullnessCode!=lastFullnessCode){
                if(lastFullnessCode!=0){
                    if(nextExpectedAllocatedRegion >= allocatedRegions.size()){
                        ASSERT_TRUE(false) << "size issue1";
                    }

                    if(allocatedRegions[nextExpectedAllocatedRegion].first!=lastRegionBegin || allocatedRegions[nextExpectedAllocatedRegion].second!=1024*i+j-1){
                        ASSERT_TRUE(false) << "range issue:" << allocatedRegions[nextExpectedAllocatedRegion].first << " " << lastRegionBegin << " " << allocatedRegions[nextExpectedAllocatedRegion].second << " " << j;
                    }

                    nextExpectedAllocatedRegion++;
                }

                if(ptFullnessCode!=0){
                    lastRegionBegin = 1024*i+j;
                }
            }

            lastFullnessCode = ptFullnessCode;
        }
    }

    if(nextExpectedAllocatedRegion!=allocatedRegions.size()){
        ASSERT_TRUE(false) << "size issue2";
    }
}
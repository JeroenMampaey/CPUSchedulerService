clean:
	rm -fr *.exe

PAGE_ALLOCATOR_C_SOURCES = $(wildcard ../operating_system/paging/page_allocator.cpp ../operating_system/paging/paging.cpp)
PAGE_ALLOCATOR_HEADERS = $(wildcard ../operating_system/paging/page_allocator.h ../operating_system/paging/paging.h)

PAGE_ALLOCATOR_TESTS_C_SOURCES = $(wildcard tests/page_allocator_tests.cpp tests/page_allocator_tests_fixture.cpp)
PAGE_ALLOCATOR_TESTS_HEADERS = $(wildcard tests/page_allocator_tests_fixture.h)

page_allocator_tests.exe: ${PAGE_ALLOCATOR_TESTS_C_SOURCES} ${PAGE_ALLOCATOR_C_SOURCES}
	g++ -DUNIT_TESTING=1 -m32 -pthread -O3 -std=c++17  $^ -lgtest -o $@

NETWORK_STACK_HANDLER_C_SOURCES = $(wildcard tests/network_stack_handler_tests.cpp tests/network_stack_handler_tests_fixture.cpp)
NETWORK_STACK_HANDLER_HEADERS = $(wildcard ../operating_system/network_management_task/network_stack_handler.h tests/network_stack_handler_tests_fixture.h)

NETWORK_STACK_HANDLER_TESTS_C_SOURCES = $(wildcard tests/network_stack_handler_tests.cpp tests/network_stack_handler_tests_fixture.cpp)
NETWORK_STACK_HANDLER_TESTS_HEADERS = $(wildcard tests/network_stack_handler_tests_fixture.h)

network_stack_handler_tests.exe: ${NETWORK_STACK_HANDLER_TESTS_C_SOURCES} ${NETWORK_STACK_HANDLER_C_SOURCES}
	g++ -DUNIT_TESTING=1 -m32 -pthread -O3 -std=c++17  $^ -lgtest -o $@
PROJECT(mthreads)
ADD_EXECUTABLE(my_threads my_thread.hpp test.cpp)
IF (NOT WIN32)
    # В POSIX нужно линковаться к pthread
    TARGET_LINK_LIBRARIES(my_threads pthread)
ENDIF (NOT WIN32)
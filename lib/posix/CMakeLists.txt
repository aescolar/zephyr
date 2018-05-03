
add_library(PTHREAD INTERFACE)

target_include_directories(PTHREAD INTERFACE ${PROJECT_SOURCE_DIR}/include/posix)

if (CONFIG_POSIX_FS)
target_include_directories(PTHREAD INTERFACE ${PROJECT_SOURCE_DIR}/ext/fs/fat/include)
target_include_directories(PTHREAD INTERFACE ${PROJECT_SOURCE_DIR}/ext/fs/nffs/include)
endif()

zephyr_library()
zephyr_library_sources(pthread_common.c)
zephyr_library_sources(pthread_cond.c)
zephyr_library_sources(pthread_mutex.c)
zephyr_library_sources(pthread_barrier.c)
zephyr_library_sources(pthread.c)
zephyr_library_sources(pthread_sched.c)
zephyr_library_sources(clock.c)
zephyr_library_sources(sleep.c)
zephyr_library_sources(timer.c)
zephyr_library_sources(pthread_rwlock.c)
zephyr_library_sources(semaphore.c)
zephyr_library_sources_ifdef(CONFIG_POSIX_MQUEUE mqueue.c)
zephyr_library_sources_ifdef(CONFIG_POSIX_FS fs.c)

zephyr_library_link_libraries(PTHREAD)
target_link_libraries(PTHREAD INTERFACE zephyr_interface)

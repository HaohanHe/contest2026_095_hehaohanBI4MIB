list(APPEND SRCS
	hal_atomic.c
	hal_interrupt.c
	hal_mutex.c  
	hal_sem.c
	hal_time.c   
	hal_workqueue.c
	hal_cache.c   
	hal_mem.c        
	hal_queue.c  
	hal_thread.c  
	hal_timer.c
)
target_sources(arch PRIVATE ${SRCS})

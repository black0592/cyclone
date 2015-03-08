/*
Copyright(C) thecodeway.com
*/
#include <cy_core.h>
#include "cyc_thread_api.h"

#ifdef CY_SYS_WINDOWS
#include <process.h>
#else
#include <sys/syscall.h>
#include <pthread.h>
#include <time.h>
#endif

namespace cyclone
{
namespace thread_api
{

//-------------------------------------------------------------------------------------
struct thread_data
{
	pid_t thread_id;
	thread_function entry_func;
	void* param;
};

//-------------------------------------------------------------------------------------
pid_t thread_get_current_id(void)
{
#ifdef CY_SYS_WINDOWS
	return ::GetCurrentThreadId();
#else
	return static_cast<pid_t>(::syscall(SYS_gettid));
#endif
}

#ifdef CY_SYS_WINDOWS
//-------------------------------------------------------------------------------------
static unsigned int __stdcall __win32_thread_entry(void* param)
{
	thread_data* data = (thread_data*)param;
	if (data && data->entry_func)
		data->entry_func(data->param);
	free(data);
	return 0;
}
#else
//-------------------------------------------------------------------------------------
static void* __pthread_thread_entry(void* param)
{
	thread_data* data = (thread_data*)param;
	if (data && data->entry_func)
		data->entry_func(data->param);
	free(data);
	return 0;
}
#endif

//-------------------------------------------------------------------------------------
thread_t thread_create(thread_function func, void* param)
{
	thread_data* data = (thread_data*)malloc(sizeof(*data));
	data->param = param;
	data->entry_func = func;

#ifdef CY_SYS_WINDOWS
	unsigned int thread_id;
	HANDLE hThread = (HANDLE)::_beginthreadex(0, 0,
		__win32_thread_entry, (void*)(data),
		THREAD_QUERY_INFORMATION | THREAD_SUSPEND_RESUME, &thread_id);
	
	data->thread_id = thread_id;
	::ResumeThread(hThread);
	return hThread;
#else
	pthread_t thread;
	pthread_create(&thread, 0, __pthread_thread_entry, data);
	return thread;
#endif
}

//-------------------------------------------------------------------------------------
void thread_sleep(int32_t msec)
{
#ifdef CY_SYS_WINDOWS
	::Sleep(msec);
#else
	struct timespec ts = { 0, 0 };
	ts.tv_sec = static_cast<time_t>(msec / 1000);
	ts.tv_nsec = static_cast<long>((msec % 1000) * 1000*1000);
	::nanosleep(&ts, NULL);
#endif
}

//-------------------------------------------------------------------------------------
void thread_join(thread_t thread)
{
#ifdef CY_SYS_WINDOWS
	::WaitForSingleObject(thread, INFINITE);
#else
	pthread_join(thread, 0);
#endif
}

//-------------------------------------------------------------------------------------
mutex_t mutex_create(void)
{
#ifdef CY_SYS_WINDOWS
	LPCRITICAL_SECTION cs = (LPCRITICAL_SECTION)malloc(sizeof(CRITICAL_SECTION));
	::InitializeCriticalSection(cs);
	return cs;
#else
	pthread_mutex_t* pm = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	::pthread_mutex_init(pm, 0);
	return pm;
#endif
}

//-------------------------------------------------------------------------------------
void mutex_destroy(mutex_t m)
{
#ifdef CY_SYS_WINDOWS
	::DeleteCriticalSection(m);
	free(m);
#else
	::pthread_mutex_destroy(m);
	free(m);
#endif
}

//-------------------------------------------------------------------------------------
void mutex_lock(mutex_t m)
{
#ifdef CY_SYS_WINDOWS
	::EnterCriticalSection(m);
#else
	::pthread_mutex_lock(m);
#endif
}

//-------------------------------------------------------------------------------------
void mutex_unlock(mutex_t m)
{
#ifdef CY_SYS_WINDOWS
	::LeaveCriticalSection(m);
#else
	::pthread_mutex_unlock(m);
#endif
}

//-------------------------------------------------------------------------------------
#ifdef CY_SYS_LINUX
struct signal_s
{
	pthread_mutex_t mutex;
	pthread_cond_t	cond;
};
#endif

//-------------------------------------------------------------------------------------
signal_t signal_create(void)
{
#ifdef CY_SYS_WINDOWS
	return ::CreateEvent(0, FALSE, FALSE, 0);
#else
	signal_s *sig = (signal_s*)malloc(sizeof(*sig));
	pthread_mutex_init(&(sig->mutex), 0);
	pthread_cond_init(&(sig->cond), 0);
	return (signal_t)sig;
#endif
}

//-------------------------------------------------------------------------------------
void signal_destroy(signal_t s)
{
#ifdef CY_SYS_WINDOWS
	::CloseHandle(s);
#else
	signal_s* sig = (signal_s*)s;
	pthread_cond_destroy(&(sig->cond));
	pthread_mutex_destroy(&(sig->mutex));
	free(sig);
#endif
}

//-------------------------------------------------------------------------------------
void signal_wait(signal_t s)
{
#ifdef CY_SYS_WINDOWS
	::WaitForSingleObject(s, INFINITE);
#else
	signal_s* sig = (signal_s*)s;
	pthread_cond_wait(&(sig->cond), &(sig->mutex));
#endif
}

//-------------------------------------------------------------------------------------
void signal_time_wait(signal_t s, int32_t msec)
{
#ifdef CY_SYS_WINDOWS
	::WaitForSingleObject(s, msec);
#else
	signal_s* sig = (signal_s*)s;
	struct timespec abstime = { 0, 0 };
	// FIXME: use CLOCK_MONOTONIC or CLOCK_MONOTONIC_RAW to prevent time rewind.
	clock_gettime(CLOCK_REALTIME, &abstime);
	abstime.tv_sec += static_cast<time_t>(msec / 1000);
	//abstime.tv_nsec = static_cast<long>((msec % 1000) * 1000 * 1000);
	pthread_cond_timedwait(&(sig->cond), &(sig->mutex), &abstime);
#endif
}

//-------------------------------------------------------------------------------------
void signal_notify(signal_t s)
{
#ifdef CY_SYS_WINDOWS
	::SetEvent(s);
#else
	signal_s* sig = (signal_s*)s;
	pthread_cond_signal(&(sig->cond));
#endif
}

}
}

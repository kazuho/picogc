/* 
 * Copyright 2012 Kazuho Oku
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of the author.
 * 
 */
#ifndef picogc_threaded_h
#define picogc_threaded_h

#include <deque>
extern "C" {
#include <pthread.h>
}

// please include picogc.h by yourself

namespace picogc {

  class threaded_gc : public gc {
    enum gc_state {
      IDLE,
      GC_REQUESTED,
      QUIT_REQUESTED,
      SWEEPING
    };
    gc_state gc_state_;
    pthread_mutex_t mutex_;
    pthread_cond_t cond_;
    pthread_t gc_thread_;
  public:
    threaded_gc(config* conf = &globals::default_config)
      : gc(conf), gc_state_(IDLE) {
      pthread_mutex_init(&mutex_, NULL);
      pthread_cond_init(&cond_, NULL);
      pthread_create(&gc_thread_, NULL, _gc_thread_start, this);
    }
    ~threaded_gc() {
      pthread_mutex_lock(&mutex_);
      _send_request(QUIT_REQUESTED);
      pthread_mutex_unlock(&mutex_);
      pthread_join(gc_thread_, NULL);
      pthread_cond_destroy(&cond_);
      pthread_mutex_destroy(&mutex_);
    }
    void trigger_gc() {
      pthread_mutex_lock(&mutex_);
      if (gc_state_ != IDLE) printf("gc stall (%p)\n", delete_head_);
      else printf("not stall\n");
      _send_request(GC_REQUESTED);
      while (gc_state_ == GC_REQUESTED) {
	pthread_cond_wait(&cond_, &mutex_);
      }
      pthread_mutex_unlock(&mutex_);
    }
  protected:
    void _send_request(gc_state request) {
      while (gc_state_ != IDLE) {
	pthread_cond_wait(&cond_, &mutex_);
      }
      _delete_pending();
      gc_state_ = request;
      pthread_cond_signal(&cond_);
    }
    void _gc_thread_start() {
      pthread_mutex_lock(&mutex_);
      while (true) {
	while (gc_state_ == IDLE) {
	  pthread_cond_wait(&cond_, &mutex_);
	}
	if (gc_state_ == QUIT_REQUESTED)
	  break;
	// start gc
	emitter_->gc_start(this);
	gc_stats stats;
	// critical section
	assert(pending_.empty());
	_setup_new(stats);
	_setup_local(stats);
	_setup_roots(stats);
	_mark(stats);
	_clear_marks_in_new(stats);
	gc_object* sweep_from = obj_head_;
	{ // insert fake object to obj_head_, to avoid thread conflict
	  gc_object* o =
	    static_cast<gc_object*>(::operator new(sizeof(gc_object)));
	  gc_object::_construct_as_gc_object(o);
	  // o->_picogc_next_ initialized after sweep
	  obj_head_ = o;
	}
	// notify the main thread that the critical section has ended
	gc_state_ = SWEEPING;
	pthread_cond_signal(&cond_);
	pthread_mutex_unlock(&mutex_);
	// sweep
	gc_object* delete_head = NULL;
	_sweep(sweep_from, delete_head, stats);
	obj_head_->_picogc_next_ = reinterpret_cast<intptr_t>(sweep_from);
	// alter the state
	pthread_mutex_lock(&mutex_);
	delete_head_ = delete_head;
	gc_state_ = IDLE;
	pthread_cond_signal(&cond_);
	// end gc
	emitter_->gc_end(this, stats);
      }
      pthread_mutex_unlock(&mutex_);
    }
    static void* _gc_thread_start(void* self) {
      static_cast<threaded_gc*>(self)->_gc_thread_start();
      return NULL;
    }
  };

}

#endif

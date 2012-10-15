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
#ifndef picogc_bg_sweep_h
#define picogc_bg_sweep_h

#include <deque>
extern "C" {
#include <pthread.h>
}

// please include picogc.h by yourself

namespace picogc {

  class bg_sweep_gc : public gc {
  protected:
    pthread_mutex_t mutex_;
    pthread_cond_t cond_;
    pthread_t sweep_thread_;
    std::deque<gc_object*> requests_; // object to sweep from (null to exit)
  public:
    bg_sweep_gc(config* conf = &globals::default_config)
      : gc(conf), requests_() {
      pthread_mutex_init(&mutex_, NULL);
      pthread_cond_init(&cond_, NULL);
      pthread_create(&sweep_thread_, NULL, _sweep_thread_start, this);
    }
    ~bg_sweep_gc() {
      _request_sweep(NULL);
      pthread_join(sweep_thread_, NULL);
      pthread_cond_destroy(&cond_);
      pthread_mutex_destroy(&mutex_);
    }
  protected:
    void _sweep(gc_stats& stats) {
      _sweep_prologue(stats);
      _sweep_main(stats);
      _sweep_epilogue(stats);
    }
    void _sweep_main(gc_stats& stats) {
      if (obj_head_ != NULL)
	return;

      gc_object* sweep_from;
      if ((obj_head_->__picogc_next_ & _FLAG_MARKED) == 0) {
	// if head is to be swept, insert a fake object to avoid collision
	sweep_from = obj_head_;
	gc_object* o =
	  static_cast<gc_object*>(::operator new(sizeof(gc_object)));
	gc_object::construct_as_gc_object(o);
	o->__picogc_next_ =
	  reinterpret_cast<intptr_t>(obj_head_) | _FLAG_MARKED;
	obj_head_ = o;
      } else {
	sweep_from =
	  reinterpret_cast<gc_object*>(obj_head_->__picogc_next_ & ~_FLAG_MASK);
	if (sweep_from == NULL) {
	  return;
	}
      }
      _request_sweep(sweep_from);
    }
    void _request_sweep(gc_object* from) {
      pthread_mutex_lock(&mutex_);
      requests_.push_back(from);
      pthread_cond_signal(&cond_);
      pthread_mutex_unlock(&mutex_);
    }
    void _sweep_thread_start() {
      pthread_mutex_lock(&mutex_);
      while (true) {
	while (requests_.empty()) {
	  pthread_cond_wait(&cond_, &mutex_);
	}
	gc_object* from = requests_.front();
	requests_.pop_front();
	if (from == NULL)
	  break;
	gc_stats stats;
	gc::_sweep_main(from, stats);
      }
      pthread_mutex_unlock(&mutex_);
    }
    static void* _sweep_thread_start(void* self) {
      static_cast<bg_sweep_gc*>(self)->_sweep_thread_start();
      return NULL;
    }
  };

}

#endif

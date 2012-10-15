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
#ifndef picogc_util_h
#define picogc_util_h

#include <cstdio>
extern "C" {
#include <sys/time.h>
}

// please include picogc.h by yourself

namespace picogc {

  class gc_log_emitter : public gc_emitter {
    FILE* fp_;
    double mark_time_;
    double sweep_time_;
    struct {
      double mark_time;
      double sweep_time;
      gc_stats stats;
    } accumulated_;
    static double now() {
      timeval tv;
      gettimeofday(&tv, NULL);
      return tv.tv_sec + tv.tv_usec / 1000000.0;
    }
  public:
    gc_log_emitter(FILE* fp) : fp_(fp) {
      accumulated_.mark_time = 0;
      accumulated_.sweep_time = 0;
      memset(&accumulated_.stats, 0, sizeof(accumulated_.stats));
    }
    virtual void gc_start(gc*) {
      fprintf(fp_, "--- picogc - garbage collection ---\n");
      fflush(fp_);
    }
    virtual void gc_end(gc*, const gc_stats& stats) {
      accumulated_.mark_time += mark_time_;
      accumulated_.sweep_time += sweep_time_;
      accumulated_.stats.on_stack += stats.on_stack;
      accumulated_.stats.slowly_marked += stats.slowly_marked;
      accumulated_.stats.not_collected += stats.not_collected;
      accumulated_.stats.collected += stats.collected;
      fprintf(fp_,
	      "mark_time:     %f (%f)\n"
	      "sweep_time:    %f (%f)\n"
	      "on_stack:      %zd (%zd)\n"
	      "slowly_marked: %zd (%zd)\n"
	      "not_collected: %zd (%zd)\n"
	      "collected:     %zd (%zd)\n"
	      "-----------------------------------\n",
	      mark_time_, accumulated_.mark_time,
	      sweep_time_, accumulated_.sweep_time,
	      stats.on_stack, accumulated_.stats.on_stack,
	      stats.slowly_marked, accumulated_.stats.slowly_marked,
	      stats.not_collected, accumulated_.stats.not_collected,
	      stats.collected, accumulated_.stats.collected);
      fflush(fp_);
    }
    virtual void mark_start(gc*) {
      mark_time_ = now();
    }
    virtual void mark_end(gc*) {
      mark_time_ = now() - mark_time_;
    }
    virtual void sweep_start(gc*) {
      sweep_time_ = now();
    }
    virtual void sweep_end(gc*) {
      sweep_time_ = now() - sweep_time_;
    }
  };

}

#endif

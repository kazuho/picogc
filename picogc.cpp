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

#include "picogc.h"

namespace picogc {

config config::default_;

gc_emitter gc_emitter::default_;

gc* scope::top_ = NULL;

gc::gc(config* conf)
  : roots_(NULL), stack_(), new_obj_head_(NULL), old_obj_head_(NULL),
    old_obj_tail_(reinterpret_cast<intptr_t*>(&old_obj_head_)), pending_(),
    bytes_allocated_since_gc_(0), config_(conf), emitter_(&gc_emitter::default_)
{
}

gc::~gc()
{
  assert(roots_ == NULL);
  // move the new object list to the old object list
  *old_obj_tail_ = reinterpret_cast<intptr_t>(new_obj_head_);
  // free all objs
  for (gc_object* o = old_obj_head_; o != NULL; ) {
    gc_object* next = reinterpret_cast<gc_object*>(o->next_ & ~FLAG_MASK);
    o->gc_destroy();
    o = next;
  }
}

void gc::trigger_gc()
{
  assert(pending_.empty());
  
  emitter_->gc_start(this);
  gc_stats stats;
  
  // move the new object list to the old object list
  assert((*old_obj_tail_ & ~FLAG_MASK) == 0);
  *old_obj_tail_ = reinterpret_cast<intptr_t>(new_obj_head_)
    | (*old_obj_tail_ & FLAG_HAS_GC_MEMBERS);
  new_obj_head_ = NULL;
  // setup local
  for (std::vector<gc_object*>::iterator i = stack_.begin(); i != stack_.end();
       ++i)
    _mark_object(*i);
  // setup root
  _setup_roots(stats);
  
  // mark
  _mark(stats);
  // sweep
  _sweep(stats);
  
  emitter_->gc_end(this, stats);
}

void gc::_mark(gc_stats& stats)
{
  emitter_->mark_start(this);
  
  // mark all the objects
  while (! pending_.empty()) {
    // pop the target object
    gc_object* o = pending_.back();
    pending_.pop_back();
    // request deferred marking of the properties
    stats.slowly_marked++;
    o->gc_mark(this);
  }
  
  emitter_->mark_end(this);
}

void gc::_sweep(gc_stats& stats)
{
  emitter_->sweep_start(this);
  
  // collect unmarked objects, as well as clearing the mark of live objects
  intptr_t* ref = reinterpret_cast<intptr_t*>(&old_obj_head_);
  for (gc_object* obj = old_obj_head_; obj != NULL; ) {
    intptr_t next = obj->next_;
    if ((next & FLAG_MARKED) != 0) {
      // alive, clear the mark and connect to the list
      *ref = reinterpret_cast<intptr_t>(obj) | (*ref & FLAG_HAS_GC_MEMBERS);
      ref = &obj->next_;
      stats.not_collected++;
    } else {
      // dead, destroy
      obj->gc_destroy();
      stats.collected++;
    }
    obj = reinterpret_cast<gc_object*>(next & ~FLAG_MASK);
  }
  *ref &= FLAG_HAS_GC_MEMBERS;
  old_obj_tail_ = ref;
  
  emitter_->sweep_end(this);
  
  // FIXME KAZUHO clear the marks on new objects
}

void gc::_setup_roots(gc_stats& stats)
{
  if (roots_ != NULL) {
    gc_root* root = roots_;
    do {
      gc_object* obj = **root;
      _mark_object(obj);
      stats.on_stack++;
    } while ((root = root->next_) != roots_);
  }
}

void gc::_register(gc_root* root)
{
  if (roots_ == NULL) {
    root->next_ = root->prev_ = root;
    roots_ = root;
  } else {
    root->next_ = roots_;
    root->prev_ = roots_->prev_;
    root->prev_->next_ = root;
    root->next_->prev_ = root;
    roots_ = root;
  }
}

void gc::_unregister(gc_root* root)
{
  if (root->next_ != root) {
    root->next_->prev_ = root->prev_;
    root->prev_->next_ = root->next_;
    if (root == roots_)
      roots_ = root->next_;
  } else {
    roots_ = NULL;
  }
}

}

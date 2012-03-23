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

config config::default_ = {
  8192 * 1024 // gc_interval_bytes_
};

gc_emitter gc_emitter::default_;

gc* scope::top_ = NULL;

gc::gc(config* conf)
  : roots_(NULL), stack_(), new_objs_(NULL), old_objs_(NULL),
    old_objs_end_(reinterpret_cast<intptr_t*>(&old_objs_)), pending_(),
    bytes_allocated_since_gc_(0), config_(conf), emitter_(&gc_emitter::default_)
{
}

gc::~gc()
{
  assert(roots_ == NULL);
  // move the new object list to the old object list
  *old_objs_end_ = reinterpret_cast<intptr_t>(new_objs_);
  // free all objs
  for (gc_object* o = old_objs_; o != NULL; ) {
    gc_object* next = reinterpret_cast<gc_object*>(o->next_ & ~FLAG_MASK);
    o->gc_destroy();
    o = next;
  }
}

void gc::trigger_gc()
{
  assert(pending_.empty());
  
  emitter_->gc_start(this);
  
  // move the new object list to the old object list
  *old_objs_end_ = reinterpret_cast<intptr_t>(new_objs_);
  new_objs_ = NULL;
  // setup local
  for (std::vector<gc_object*>::iterator i = stack_.begin(); i != stack_.end();
       ++i)
    _mark_object(*i);
  // setup root
  _setup_roots();
  
  // mark
  _mark();
  // sweep
  _sweep();
  
  emitter_->gc_end(this, gc_stats());
}

void gc::_mark()
{
  emitter_->mark_start(this);
  
  // mark all the objects
  while (! pending_.empty()) {
    // pop the target object
    gc_object* o = pending_.back();
    pending_.pop_back();
    // request deferred marking of the properties
    if ((o->next_ & FLAG_HAS_GC_MEMBERS) != 0)
      o->gc_mark(this);
  }
  
  emitter_->mark_end(this);
}

void gc::_sweep()
{
  emitter_->sweep_start(this);
  
  // collect unmarked objects, as well as clearing the mark of live objects
  intptr_t* ref = reinterpret_cast<intptr_t*>(&old_objs_);
  for (gc_object* obj = old_objs_; obj != NULL; ) {
    intptr_t next = obj->next_;
    if ((next & FLAG_MARKED) != 0) {
      // alive, clear the mark and connect to the list
      *ref = reinterpret_cast<intptr_t>(obj) | (*ref & FLAG_HAS_GC_MEMBERS);
      ref = &obj->next_;
    } else {
      // dead, destroy
      obj->gc_destroy();
    }
    obj = reinterpret_cast<gc_object*>(next & ~FLAG_MASK);
  }
  *ref = (intptr_t) NULL;
  old_objs_end_ = ref;
  
  emitter_->sweep_end(this);
  
  // FIXME KAZUHO clear the marks on new objects
}

void gc::_setup_roots()
{
  if (roots_ != NULL) {
    gc_root* root = roots_;
    do {
      gc_object* obj = **root;
      _mark_object(obj);
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

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
#ifndef picogc_h
#define picogc_h

extern "C" {
#include <stdint.h>
}
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cassert>

namespace picogc {
  
  // internal flags
  enum {
    _FLAG_MARKED = 1,
    _FLAG_HAS_GC_MEMBERS = 2,
    _FLAG_MASK = 3
  };
  
  // external flags
  enum {
    IS_ATOMIC = 0x1,
    IMMEDIATELY_TRACEABLE = 0x2,
    MAY_TRIGGER_GC = 0x4
  };

  class gc;
  class gc_object;
  
  template <typename value_type, size_t VALUES_PER_NODE = 2048> class _stack {
    struct node {
      value_type values[VALUES_PER_NODE];
      node *prev; // null if bottom
    };
    node* node_;
    node* reserved_node_;
    value_type* top_;
  public:
    class iterator {
      node* node_;
      value_type* cur_;
    public:
      iterator(_stack& s) : node_(s.node_), cur_(s.top_) {}
      value_type* get() {
	if (cur_ == node_->values) {
	  if (node_->prev == NULL) {
	    return NULL;
	  }
	  node_ = node_->prev;
	  cur_ = node_->values + VALUES_PER_NODE;
	}
	return --cur_;
      }
    };
    _stack() : node_(new node), reserved_node_(NULL), top_(node_->values) {
      node_->prev = NULL;
    }
    ~_stack() {
      delete reserved_node_;
      while (node_ != NULL) {
	node* n = node_->prev;
	delete node_;
	node_ = n;
      }
    }
    bool empty() const {
      return node_->prev == NULL && top_ == node_->values;
    }
    value_type* push() {
      if (top_ == node_->values + VALUES_PER_NODE) {
	node* new_node;
	if (reserved_node_ != NULL) {
	  new_node = reserved_node_;
	  reserved_node_ = NULL;
	} else {
	  new_node = new node;
	}
	new_node->prev = node_;
	node_ = new_node;
	top_ = new_node->values;
      }
      return top_++;
    }
    value_type* pop() {
      if (top_ == node_->values) {
	if (node_->prev == NULL) {
	  return NULL;
	}
	delete reserved_node_;
	reserved_node_ = node_;
	node_ = node_->prev;
	top_ = node_->values + VALUES_PER_NODE;
      }
      return --top_;
    }
    value_type* preserve() {
      return top_;
    }
    void restore(value_type* slot) {
      node* n = node_;
      while (! (n->values <= slot && slot <= n->values + VALUES_PER_NODE)) {
	if (reserved_node_ == NULL) {
	  reserved_node_ = n;
	  n = n->prev;
	} else {
	  node* prev = n->prev;
	  delete n;
	  n = prev;
	}
      }
      // found
      node_ = n;
      top_ = slot;
    }
  };

  struct config {
    size_t gc_interval_bytes_;
    config() : gc_interval_bytes_(8 * 1024 * 1024) {}
    size_t gc_interval_bytes() const { return gc_interval_bytes_; }
    config& gc_interval_bytes(size_t v) {
      gc_interval_bytes_ = v;
      return *this;
    }
  };
  
  struct gc_stats {
    size_t on_stack;
    size_t slowly_marked;
    size_t not_collected;
    size_t collected;
    gc_stats() : on_stack(0), slowly_marked(0), not_collected(0), collected(0)
    {}
  };
  
  struct gc_emitter {
    virtual ~gc_emitter() {}
    virtual void gc_start(gc*) {}
    virtual void gc_end(gc*, const gc_stats&) {}
    virtual void mark_start(gc*) {}
    virtual void mark_end(gc*) {}
    virtual void sweep_start(gc*) {}
    virtual void sweep_end(gc*) {}
  };
  
  // global variables
  template <bool T> struct _globals {
    static config default_config;
    static gc_emitter default_emitter;
    static gc* _top_scope;
  };
  template <bool T> config _globals<T>::default_config;
  template <bool T> gc_emitter _globals<T>::default_emitter;
  template <bool T> gc* _globals<T>::_top_scope;
  typedef _globals<false> globals;
  
  template <typename T> class local {
    gc_object** slot_;
  public:
    local(T* obj = NULL);
    local(const local<T>& x);
    local& operator=(const local<T>& x) { *slot_ = *x.slot_; return *this; }
    local& operator=(T* obj) { *slot_ = obj; return *this; }
    T* get() const { return static_cast<T*>(*slot_); }
    operator T*() const { return get(); }
    T* operator->() const { return get(); }
  };
  
  class gc_scope {
    gc* prev_;
  public:
    gc_scope(gc* gc) : prev_(globals::_top_scope) {
      globals::_top_scope = gc;
    }
    ~gc_scope() {
      globals::_top_scope = prev_;
    }
  };
  
  class scope {
    friend class gc;
    gc_object* new_head_;
    intptr_t* new_tail_slot_;
    scope* prev_;
    gc_object** stack_state_;
    void _destruct(gc* gc);
  public:
    scope();
    ~scope();
    template <typename T> T* close(T* obj);
  };
  
  class gc {
    friend class scope;
    scope* scope_;
    _stack<gc_object*> stack_;
    gc_object* obj_head_;
    _stack<gc_object*> pending_;
    size_t bytes_allocated_since_gc_;
    config conf_;
    gc_emitter* emitter_;
  public:
    gc(const config& conf = config())
      : scope_(NULL), stack_(), obj_head_(NULL), pending_(),
	bytes_allocated_since_gc_(0), conf_(conf),
	emitter_(&globals::default_emitter)
    {}
    ~gc();
    void* allocate(size_t sz, int flags);
    void trigger_gc();
    void may_trigger_gc();
    void mark(gc_object* obj);
    gc_object** _acquire_local_slot() {
      return stack_.push();
    }
    gc_emitter* emitter() { return emitter_; }
    void emitter(gc_emitter* emitter) { emitter_ = emitter; }
    static gc* top() {
      assert(globals::_top_scope != NULL);
      return globals::_top_scope;
    }
  protected:
    virtual void _mark(gc_stats& stats);
    virtual void _sweep(gc_stats& stats);
  };
  
  class gc_object {
    friend class gc;
    intptr_t next_;
    gc_object(const gc_object&); // = delete;
    gc_object& operator=(const gc_object&); // = delete;
  protected:
    gc_object() /* next_ is initialized in operator new */ {}
    virtual ~gc_object() {}
    virtual void gc_mark(picogc::gc* gc) {}
  public:
    bool gc_is_marked() const { return (next_ & _FLAG_MARKED) != 0; }
    static void* operator new(size_t sz);
    static void* operator new(size_t sz, int flags);
    static void operator delete(void* p);
    static void operator delete(void* p, int flags);
  private:
    static void* operator new(size_t, void* buf) { return buf; }
  };
  
  template <typename T>
  inline local<T>::local(T* obj) : slot_(gc::top()->_acquire_local_slot())
  {
    *slot_ = obj;
  }

  template <typename T>
  inline local<T>::local(const local<T>& x)
  : slot_(gc::top()->_acquire_local_slot())
  {
    *slot_ = *x.slot_;
  }

  inline scope::scope() : new_head_(NULL), new_tail_slot_(NULL)
  {
    gc* gc = gc::top();
    prev_ = gc->scope_;
    gc->scope_ = this;
    stack_state_ = gc->stack_.preserve();
  }
  
  inline void scope::_destruct(gc* gc)
  {
    gc->stack_.restore(stack_state_);
    gc->scope_ = prev_;
    if (new_head_ != NULL) {
      *new_tail_slot_ |= reinterpret_cast<intptr_t>(gc->obj_head_);
      gc->obj_head_ = new_head_;
    }
  }

  inline scope::~scope()
  {
    gc* gc = gc::top();
    if (stack_state_ != NULL)
      _destruct(gc);
    gc->may_trigger_gc();
  }
  
  template <typename T> inline T* scope::close(T* obj) {
    gc* gc = gc::top();
    // destruct the frame, and push the returning value on the prev frame
    _destruct(gc);
    stack_state_ = NULL;
    *gc->stack_.push() = static_cast<gc_object*>(obj);
    return obj;
  }
  
  inline gc::~gc()
  {
    // free all objs
    for (gc_object* o = obj_head_; o != NULL; ) {
      gc_object* next = reinterpret_cast<gc_object*>(o->next_ & ~_FLAG_MASK);
      o->~gc_object();
      ::operator delete(static_cast<void*>(o));
      o = next;
    }
  }
  
  inline void* gc::allocate(size_t sz, int flags)
  {
    bytes_allocated_since_gc_ += sz;
    if ((flags & MAY_TRIGGER_GC) != 0) {
      may_trigger_gc();
    }
    gc_object* p = static_cast<gc_object*>(::operator new(sz));
    // GC might walk through the object during construction
    if ((flags & IS_ATOMIC) == 0) {
      memset(static_cast<void*>(p), 0, sz);
    }
    // register to GC list
    if ((flags & IMMEDIATELY_TRACEABLE) != 0) {
      p->next_ = reinterpret_cast<intptr_t>(obj_head_)
          | ((flags & IS_ATOMIC) != 0 ? 0 : _FLAG_HAS_GC_MEMBERS);
      obj_head_ = p;
    } else {
      scope* scope = scope_;
      if (scope->new_head_ == NULL)
        scope->new_tail_slot_ = &p->next_;
      p->next_ = reinterpret_cast<intptr_t>(scope->new_head_)
          | ((flags & IS_ATOMIC) != 0 ? 0 : _FLAG_HAS_GC_MEMBERS);
      scope->new_head_ = p;
    }
    return p;
  }
  
  inline void gc::_mark(gc_stats& stats)
  {
    // mark all the objects
    gc_object** slot;
    while ((slot = pending_.pop()) != NULL) {
      // request deferred marking of the properties
      stats.slowly_marked++;
      (*slot)->gc_mark(this);
    }
  }
  
  inline void gc::_sweep(gc_stats& stats)
  {
    // collect unmarked objects, as well as clearing the mark of live objects
    intptr_t* ref = reinterpret_cast<intptr_t*>(&obj_head_);
    for (gc_object* obj = obj_head_; obj != NULL; ) {
      intptr_t next = obj->next_;
      if ((next & _FLAG_MARKED) != 0) {
	// alive, clear the mark and connect to the list
	*ref = reinterpret_cast<intptr_t>(obj) | (*ref & _FLAG_HAS_GC_MEMBERS);
	ref = &obj->next_;
	stats.not_collected++;
      } else {
	// dead, destroy
	obj->~gc_object();
	::operator delete(static_cast<void*>(obj));
	stats.collected++;
      }
      obj = reinterpret_cast<gc_object*>(next & ~_FLAG_MASK);
    }
    *ref &= _FLAG_HAS_GC_MEMBERS;
  }
  
  inline void gc::trigger_gc()
  {
    assert(pending_.empty());
    
    emitter_->gc_start(this);
    gc_stats stats;
    
    // setup new
    for (scope* scope = scope_; scope != NULL; scope = scope->prev_) {
      for (gc_object* o = scope_->new_head_;
	   o != NULL;
	   o = reinterpret_cast<gc_object*>(o->next_ & ~_FLAG_MASK)) {
	mark(o);
	stats.on_stack++;
      }
    }
    { // setup local
      _stack<gc_object*>::iterator iter(stack_);
      gc_object** o;
      while ((o = iter.get()) != NULL) {
	mark(*o);
	stats.on_stack++;
      }
    }
    
    // mark
    emitter_->mark_start(this);
    _mark(stats);
    emitter_->mark_end(this);
    // sweep
    emitter_->sweep_start(this);
    _sweep(stats);
    emitter_->sweep_end(this);
    
    // clear the marks in new (as well as count the number)
    for (scope* scope = scope_; scope != NULL; scope = scope->prev_) {
      for (gc_object* o = scope_->new_head_;
	   o != NULL;
	   o = reinterpret_cast<gc_object*>(o->next_ & ~_FLAG_MASK)) {
	o->next_ &= ~_FLAG_MARKED;
	stats.not_collected++;
      }
    }

    emitter_->gc_end(this, stats);
  }
  
  inline void gc::may_trigger_gc()
  {
    if (bytes_allocated_since_gc_ >= conf_.gc_interval_bytes()) {
      trigger_gc();
      bytes_allocated_since_gc_ = 0;
    }
  }

  inline void gc::mark(gc_object* obj)
  {
    if (obj == NULL)
      return;
    // return if already marked
    if ((obj->next_ & _FLAG_MARKED) != 0)
      return;
    // mark
    obj->next_ |= _FLAG_MARKED;
    // push to the mark stack
    if ((obj->next_ & _FLAG_HAS_GC_MEMBERS) != 0)
      *pending_.push() = obj;
  }
  
  inline void* gc_object::operator new(size_t sz)
  {
    return gc::top()->allocate(sz, 0);
  }

  inline void* gc_object::operator new(size_t sz, int flags)
  {
    return gc::top()->allocate(sz, flags);
  }

  // only called when an exception is raised within ctor
  inline void gc_object::operator delete(void* p)
  {
    // vtbl should point to an empty dtor
    new (p) gc_object;
  }

  inline void gc_object::operator delete(void* p, int)
  {
    gc_object::operator delete(p);
  }
  
}

#endif

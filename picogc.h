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
    IS_ATOMIC = 1
  };

  class gc;
  class gc_root;
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
    _stack() : node_(new node), reserved_node_(NULL), top_(node_->values) {}
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
    config(size_t gc_interval_bytes = 8192 * 1024)
      : gc_interval_bytes_(gc_interval_bytes) {}
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
    gc_root* roots_;
    scope* scope_;
    _stack<gc_object*> stack_;
    gc_object* obj_head_;
    _stack<gc_object*> pending_;
    size_t bytes_allocated_since_gc_;
    config* config_;
    gc_emitter* emitter_;
  public:
    gc(config* conf = &globals::default_config)
      : roots_(NULL), scope_(NULL), stack_(), obj_head_(NULL), pending_(),
	bytes_allocated_since_gc_(0), config_(conf),
	emitter_(&globals::default_emitter)
    {}
    ~gc();
    void* allocate(size_t sz, bool has_gc_members);
    void trigger_gc();
    void may_trigger_gc();
    void mark(gc_object* obj);
    void _register(gc_root* root);
    void _unregister(gc_root* root);
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
    void _setup_roots(gc_stats& stats);
    void _setup_new(gc_stats& stats);
    void _clear_marks_in_new(gc_stats& stats);
    void _setup_local(gc_stats& stats);
    void _mark(gc_stats& stats);
    void _sweep(gc_stats& stats);
  };
  
  class gc_root {
    friend class gc;
    gc_object* obj_;
    gc_root* prev_;
    gc_root* next_;
  public:
    gc_root(gc_object* obj) : obj_(obj) {
      gc::top()->_register(this);
    }
    ~gc_root() {
      gc::top()->_unregister(this);
    }
    gc_object* operator*() { return obj_; }
  };
  
  class gc_object {
    friend class gc;
    gc_object(const gc_object&); // = delete;
    gc_object& operator=(const gc_object&); // = delete;
  public:
    intptr_t _picogc_next_;
    gc_object() /* _picogc_next_ is initialized in operator new */ {}
    virtual ~gc_object() {}
    virtual void gc_mark(picogc::gc* gc) {}
  public:
    static void* operator new(size_t sz, int flags = 0);
    static void operator delete(void* p);
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
    assert(roots_ == NULL);
    // free all objs
    for (gc_object* o = obj_head_; o != NULL; ) {
      gc_object* next =
	reinterpret_cast<gc_object*>(o->_picogc_next_ & ~_FLAG_MASK);
      o->~gc_object();
      ::operator delete(static_cast<void*>(o));
      o = next;
    }
  }
  
  inline void* gc::allocate(size_t sz, bool has_gc_members)
  {
    bytes_allocated_since_gc_ += sz;
    gc_object* p = static_cast<gc_object*>(::operator new(sz));
    // GC might walk through the object during construction
    if (has_gc_members) {
      memset(p, 0, sz);
    }
    // register to GC list
    scope* scope = scope_;
    if (scope->new_head_ == NULL)
      scope->new_tail_slot_ = &p->_picogc_next_;
    p->_picogc_next_ = reinterpret_cast<intptr_t>(scope->new_head_)
      | (has_gc_members ? _FLAG_HAS_GC_MEMBERS : 0);
    scope->new_head_ = p;
    return p;
  }
  
  inline void gc::_mark(gc_stats& stats)
  {
    emitter_->mark_start(this);
    
    // mark all the objects
    gc_object** slot;
    while ((slot = pending_.pop()) != NULL) {
      // request deferred marking of the properties
      stats.slowly_marked++;
      (*slot)->gc_mark(this);
    }
    
    emitter_->mark_end(this);
  }
  
  inline void gc::_sweep(gc_stats& stats)
  {
    emitter_->sweep_start(this);
    
    // collect unmarked objects, as well as clearing the mark of live objects
    intptr_t* ref = reinterpret_cast<intptr_t*>(&obj_head_);
    for (gc_object* obj = obj_head_; obj != NULL; ) {
      intptr_t next = obj->_picogc_next_;
      if ((next & _FLAG_MARKED) != 0) {
	// alive, clear the mark and connect to the list
	*ref = reinterpret_cast<intptr_t>(obj) | (*ref & _FLAG_HAS_GC_MEMBERS);
	ref = &obj->_picogc_next_;
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
    
    emitter_->sweep_end(this);
  }
  
  inline void gc::_setup_roots(gc_stats& stats)
  {
    if (roots_ != NULL) {
      gc_root* root = roots_;
      do {
	gc_object* obj = **root;
	mark(obj);
	stats.on_stack++;
      } while ((root = root->next_) != roots_);
    }
  }
  
  inline void gc::_setup_new(gc_stats& stats)
  {
    for (scope* scope = scope_; scope != NULL; scope = scope->prev_) {
      for (gc_object* o = scope_->new_head_;
	   o != NULL;
	   o = reinterpret_cast<gc_object*>(o->_picogc_next_ & ~_FLAG_MASK)) {
	mark(o);
	stats.on_stack++;
      }
    }
  }

  inline void gc::_clear_marks_in_new(gc_stats& stats)
  {
    for (scope* scope = scope_; scope != NULL; scope = scope->prev_) {
      for (gc_object* o = scope_->new_head_;
	   o != NULL;
	   o = reinterpret_cast<gc_object*>(o->_picogc_next_ & ~_FLAG_MASK)) {
	o->_picogc_next_ &= ~_FLAG_MARKED;
	stats.not_collected++;
      }
    }
  }

  inline void gc::_setup_local(gc_stats& stats)
  {
    _stack<gc_object*>::iterator iter(stack_);
    gc_object** o;
    while ((o = iter.get()) != NULL) {
      mark(*o);
      stats.on_stack++;
    }
  }

  inline void gc::trigger_gc()
  {
    assert(pending_.empty());

    emitter_->gc_start(this);
    gc_stats stats;

    _setup_new(stats);
    _setup_local(stats);
    _setup_roots(stats);

    _mark(stats);

    _clear_marks_in_new(stats);

    _sweep(stats);

    emitter_->gc_end(this, stats);
  }
  
  inline void gc::may_trigger_gc()
  {
    if (bytes_allocated_since_gc_ >= config_->gc_interval_bytes_) {
      trigger_gc();
      bytes_allocated_since_gc_ = 0;
    }
  }

  inline void gc::mark(gc_object* obj)
  {
    if (obj == NULL)
      return;
    // return if already marked
    if ((obj->_picogc_next_ & _FLAG_MARKED) != 0)
      return;
    // mark
    obj->_picogc_next_ |= _FLAG_MARKED;
    // push to the mark stack
    if ((obj->_picogc_next_ & _FLAG_HAS_GC_MEMBERS) != 0)
      *pending_.push() = obj;
  }
  
  inline void gc::_register(gc_root* root)
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
  
  inline void gc::_unregister(gc_root* root)
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
  
  inline void* gc_object::operator new(size_t sz, int flags)
  {
    return gc::top()->allocate(sz, (flags & IS_ATOMIC) == 0);
  }

  // only called when an exception is raised within ctor
  inline void gc_object::operator delete(void* p)
  {
    // vtbl should point to an empty dtor
    new (p) gc_object;
  }
  
}

#endif

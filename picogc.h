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
#include <cstring>
#include <cassert>
#include <vector>

namespace picogc {
  
  enum {
    FLAG_MARKED = 1,
    FLAG_HAS_GC_MEMBERS = 2,
    FLAG_MASK = 3
  };
  
  class gc;
  class gc_root;
  class gc_object;
  
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
    T* obj_;
  public:
    local(T* obj = NULL);
    local(const local<T>& x) : obj_(x.obj_) {}
    local& operator=(const local<T>& x) { obj_ = x.obj_; return *this; }
    local& operator=(T* obj);
    operator T*() const { return obj_; }
    T* operator->() const { return obj_; }
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
    std::vector<gc_object*>::size_type frame_;
  public:
    scope();
    ~scope();
    template <typename T> local<T>& close(local<T>& l);
  };
  
  class gc {
    friend class scope;
    gc_root* roots_;
    std::vector<gc_object*> stack_;
    gc_object* skip_dtor_obj_head_, * call_dtor_obj_head_;
    std::vector<gc_object*> pending_;
    size_t bytes_allocated_since_gc_;
    config* config_;
    gc_emitter* emitter_;
  public:
    gc(config* conf = &globals::default_config)
      : roots_(NULL), stack_(), skip_dtor_obj_head_(NULL),
        call_dtor_obj_head_(NULL), pending_(), bytes_allocated_since_gc_(0),
	config_(conf), emitter_(&globals::default_emitter)
    {}
    ~gc();
    void* allocate(size_t sz, bool has_gc_members);
    void trigger_gc();
    void _register(gc_object* obj, bool has_gc_members, bool skip_dtor);
    void mark(gc_object* obj);
    void _register(gc_root* root);
    void _unregister(gc_root* root);
    void _register_local(gc_object* o) {
      stack_.push_back(o);
    }
    gc_emitter* emitter() { return emitter_; }
    void emitter(gc_emitter* emitter) { emitter_ = emitter; }
    static gc* top() {
      assert(globals::_top_scope != NULL);
      return globals::_top_scope;
    }
  protected:
    void _setup_roots(gc_stats& stats);
    void _mark(gc_stats& stats);
    template<bool skip_dtor> void _sweep_list(gc_stats& stats,
					      gc_object*& head);
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
    intptr_t next_;
    gc_object(const gc_object&); // = delete;
    gc_object& operator=(const gc_object&); // = delete;
  protected:
    gc_object(bool has_gc_members = true, bool skip_dtor = false);
    virtual ~gc_object() {}
    virtual void gc_mark(picogc::gc* gc) {}
  public:
    static void* operator new(size_t sz);
  };
  
  class gc_atomic_object : public gc_object {
    gc_atomic_object(const gc_atomic_object&); // = delete;
    gc_atomic_object& operator=(const gc_atomic_object&); // = delete;
  protected:
    gc_atomic_object() : gc_object(false) {}
  public:
    static void* operator new(size_t sz);
  };
  
  template <typename T> local<T>::local(T* obj) : obj_(obj)
  {
    if (obj != NULL) {
      gc* gc = gc::top();
      gc->_register_local(obj);
    }
  }
  
  template <typename T> local<T>& local<T>::operator=(T* obj)
  {
    if (obj_ != obj) {
      if (obj != NULL) {
	gc* gc = gc::top();
	gc->_register_local(obj);
      }
      obj_ = obj;
    }
    return *this;
  }
  
  inline scope::scope() : frame_(gc::top()->stack_.size())
  {
  }
  
  inline scope::~scope()
  {
    gc::top()->stack_.resize(frame_);
  }
  
  template <typename T> local<T>& scope::close(local<T>& obj) {
    gc_object* o = obj;
    // destruct the frame, and push the returning value on the prev frame
    gc* gc = gc::top();
    gc->stack_[frame_] = o;
    gc->stack_.resize(++frame_);
    return obj;
  }
  
  inline gc::~gc()
  {
    assert(roots_ == NULL);
    // free all objs
    for (gc_object* o = skip_dtor_obj_head_; o != NULL; ) {
      gc_object* next = reinterpret_cast<gc_object*>(o->next_ & ~FLAG_MASK);
      operator delete(o); // do not invoke dtor
      o = next;
    }
    for (gc_object* o = call_dtor_obj_head_; o != NULL; ) {
      gc_object* next = reinterpret_cast<gc_object*>(o->next_ & ~FLAG_MASK);
      delete o;
      o = next;
    }
  }
  
  inline void* gc::allocate(size_t sz, bool has_gc_members)
  {
    bytes_allocated_since_gc_ += sz;
    if (bytes_allocated_since_gc_ >= config_->gc_interval_bytes_) {
      trigger_gc();
      bytes_allocated_since_gc_ = 0;
    }
    void* p = ::operator new(sz);
    if (has_gc_members)
      memset(p, 0, sz); // GC might walk through the object during construction
    return p;
  }
  
  inline void gc::_mark(gc_stats& stats)
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
  
  template<bool skip_dtor> inline void gc::_sweep_list(gc_stats& stats,
						       gc_object*& head)
  {
    // collect unmarked objects, as well as clearing the mark of live objects
    intptr_t* ref = reinterpret_cast<intptr_t*>(&head);
    for (gc_object* obj = head; obj != NULL; ) {
      intptr_t next = obj->next_;
      if ((next & FLAG_MARKED) != 0) {
	// alive, clear the mark and connect to the list
	*ref = reinterpret_cast<intptr_t>(obj) | (*ref & FLAG_HAS_GC_MEMBERS);
	ref = &obj->next_;
	stats.not_collected++;
      } else {
	// dead, destroy
	if (skip_dtor) {
	  operator delete(obj);
	} else {
	  delete obj;
	}
	stats.collected++;
      }
      obj = reinterpret_cast<gc_object*>(next & ~FLAG_MASK);
    }
    *ref &= FLAG_HAS_GC_MEMBERS;
  }

  inline void gc::_sweep(gc_stats& stats)
  {
    emitter_->sweep_start(this);

    _sweep_list<true>(stats, skip_dtor_obj_head_);
    _sweep_list<false>(stats, call_dtor_obj_head_);
    
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
  
  inline void gc::trigger_gc()
  {
    assert(pending_.empty());
    
    emitter_->gc_start(this);
    gc_stats stats;
    
    // setup local
    for (std::vector<gc_object*>::iterator i = stack_.begin();
	 i != stack_.end(); ++i)
      mark(*i);
    // setup root
    _setup_roots(stats);
    
    // mark
    _mark(stats);
    // sweep
    _sweep(stats);
    
    emitter_->gc_end(this, stats);
  }
  
  inline void gc::_register(gc_object* obj, bool has_gc_members, bool skip_dtor)
  {
    gc_object*& head = skip_dtor ? skip_dtor_obj_head_ : call_dtor_obj_head_;
    obj->next_ = reinterpret_cast<intptr_t>(head)
      | (has_gc_members ? FLAG_HAS_GC_MEMBERS : 0);
    head = obj;
    // NOTE: not marked
  }
  
  inline void gc::mark(gc_object* obj)
  {
    if (obj == NULL)
      return;
    // return if already marked
    if ((obj->next_ & FLAG_MARKED) != 0)
      return;
    // mark
    obj->next_ |= FLAG_MARKED;
    // push to the mark stack
    if ((obj->next_ & FLAG_HAS_GC_MEMBERS) != 0)
      pending_.push_back(obj);
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
  
  inline gc_object::gc_object(bool has_gc_members, bool skip_dtor)
  {
    gc* gc = gc::top();
    // protect the object by first registering the object to the local list and then to the GC chain
    gc->_register_local(this);
    gc->_register(this, has_gc_members, skip_dtor);
  }
  
  inline void* gc_object::operator new(size_t sz)
  {
    return gc::top()->allocate(sz, true);
  }

  inline void* gc_atomic_object::operator new(size_t sz)
  {
    return gc::top()->allocate(sz, false);
  }
  
}

#endif

#ifndef picogc_h
#define picogc_h

extern "C" {
#include <stdint.h>
}
#include <cassert>
#include <vector>

namespace picogc {
  
  class gc;
  class gc_root;
  class gc_object;
  
  template <typename T> class member {
    T* obj_;
  public:
    member(T* obj = NULL);
    member& operator=(T* obj);
    operator T*() const { return obj_; }
    T* operator->() const { return obj_; }
  };
  
  template <typename T> class local {
    T* obj_;
  public:
    local() : obj_(NULL) {}
    local(const local<T>& x) : obj_(x.obj_) {}
    local(T* obj);
    local& operator=(const local<T>& x) { obj_ = x.obj_; return *this; }
    local& operator=(T* obj);
    operator T*() const { return obj_; }
    T* operator->() const { return obj_; }
  };
  
  class scope {
    static gc* top_;
    gc* prev_;
    std::vector<gc_object*>::size_type frame_;
  public:
    scope(gc* gc = top_);
    ~scope();
    template <typename T> local<T>& close(local<T>& l);
    static gc* top() {
      assert(top_ != NULL);
      return top_;
    }
  };
  
  class gc {
    friend class scope;
    gc_root* roots_;
    std::vector<gc_object*> stack_;
    gc_object* new_objs_;
    gc_object* old_objs_;
    intptr_t* old_objs_end_;
    std::vector<gc_object*> pending_;
  public:
    gc();
    void trigger_gc();
    void _enter();
    void _register(gc_object* obj);
    template <typename T> void mark(member<T>& _obj);
    void _mark_object(gc_object* obj);
    void _register(gc_root* root);
    void _unregister(gc_root* root);
    void _assign_barrier(gc_object*) {} // for concurrent GC
    void _unassign_barrier(gc_object*) {} // for concurrent GC
    void _register_local(gc_object* o) {
      stack_.push_back(o);
    }
  protected:
    void _setup_roots();
    void _mark();
    void _compact();
  };
  
  class gc_root {
    friend class gc;
    gc_object* obj_;
    gc_root* prev_;
    gc_root* next_;
  public:
    gc_root(gc_object* obj) : obj_(obj) {
      scope::top()->_register(this);
    }
    ~gc_root() {
      scope::top()->_unregister(this);
    }
    gc_object* operator*() { return obj_; }
  };
  
  class gc_object {
    friend class gc;
    intptr_t next_;
    gc_object(const gc_object&); // = delete;
    gc_object& operator=(const gc_object&); // = delete;
  protected:
    gc_object();
    ~gc_object() {}
    virtual void gc_destroy() = 0;
    virtual void gc_mark(picogc::gc* gc) = 0;
  };

  template <typename T> member<T>::member(T* obj) : obj_(obj)
  {
    gc_object* o = obj; // all GC-able objects should be inheriting gc_object
    scope::top()->_assign_barrier(o);
  }
  
  template <typename T> member<T>& member<T>::operator=(T* obj)
  {
    if (obj_ != obj) {
      gc_object* o = obj; // all GC-able objects should be inheriting gc_object
      scope::top()->_assign_barrier(o);
      scope::top()->_unassign_barrier(obj_);
      obj_ = obj;
    }
    return *this;
  }
  
  template <typename T> local<T>::local(T* obj) : obj_(obj)
  {
    gc* gc = scope::top();
    gc->_assign_barrier(obj);
    gc->_register_local(obj);
  }
  
  template <typename T> local<T>& local<T>::operator=(T* obj)
  {
    if (obj_ != obj) {
      gc* gc = scope::top();
      gc->_unassign_barrier(obj_);
      gc->_assign_barrier(obj);
      gc->_register_local(obj);
      obj_ = obj;
    }
    return *this;
  }
  
  inline scope::scope(gc* gc) : prev_(top_), frame_(gc->stack_.size())
  {
    top_ = gc;
    top_->_enter();
  }
  
  inline scope::~scope()
  {
    top_->stack_.resize(frame_);
    top_ = prev_;
    top_->_enter();
  }
  
  template <typename T> local<T>& scope::close(local<T>& obj) {
    gc_object* o = obj;
    // destruct the frame, and push the returning value on the prev frame
    top_->stack_[frame_] = o;
    top_->stack_.resize(++frame_);
    return obj;
  }
  
  inline void gc::_register(gc_object* obj)
  {
    obj->next_ = reinterpret_cast<intptr_t>(new_objs_);
    new_objs_ = obj;
    // NOTE: not marked
  }
  
  inline void gc::_mark_object(gc_object* obj)
  {
    if (obj == NULL)
      return;
    // return if already marked
    if ((obj->next_ & 1) != 0)
      return;
    // mark
    obj->next_ |= 1;
    // push to the mark stack
    pending_.push_back(obj);
  }

  template <typename T> void gc::mark(member<T>& _obj)
  {
    gc_object* obj = &*_obj; // members should conform this type conversion
    _mark_object(obj);
  }

  inline gc_object::gc_object() {
    scope::top()->_register(this);
  }

}

#endif

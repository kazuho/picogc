#include "picogc.h"

using namespace picogc;

gc* scope::top_ = NULL;

gc::gc()
  : roots_(NULL), stack_(), new_objs_(NULL), old_objs_(NULL),
    old_objs_end_(reinterpret_cast<intptr_t*>(&old_objs_)), pending_()
{
}

void gc::_enter()
{
  // FIXME KAZUHO check the heuristics whether or not to trigger GC
}

void gc::trigger_gc()
{
  assert(pending_.empty());
  
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
  // compact
  _compact();
}

void gc::_mark()
{
  // mark all the objects
  while (! pending_.empty()) {
    // pop the target object
    gc_object* o = pending_.back();
    pending_.pop_back();
    // request deferred marking of the properties
    o->gc_mark(this);
  }
}

void gc::_compact()
{
  // collect unmarked objects, as well as clearing the mark of live objects
  intptr_t* ref = reinterpret_cast<intptr_t*>(&old_objs_);
  for (gc_object* obj = old_objs_; obj != NULL; ) {
    intptr_t next = obj->next_;
    if ((next & 1) != 0) {
      // alive, clear the mark and connect to the list
      next &= ~1;
      *ref = reinterpret_cast<intptr_t>(obj);
      ref = &obj->next_;
    } else {
      // dead, destroy
      obj->gc_destroy();
    }
    obj = reinterpret_cast<gc_object*>(next);
  }
  *ref = (intptr_t) NULL;
  old_objs_end_ = ref;
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

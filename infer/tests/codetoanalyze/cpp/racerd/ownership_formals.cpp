/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <mutex>
#include <utility>
#include <vector>

namespace ownership_formals {

struct Callback {
  void* exec_ = nullptr;
  Callback() = default;
  Callback(Callback&& o) noexcept : exec_(o.exec_) { o.exec_ = nullptr; }
  Callback& operator=(Callback&& o) noexcept {
    exec_ = o.exec_;
    o.exec_ = nullptr;
    return *this;
  }
  explicit operator bool() const { return exec_ != nullptr; }
};

// A by-value class parameter belongs to this call: no other thread can reach
// it. The frontend passes it by reference, so it is indistinguishable here from
// a real reference formal, and in both cases whether the object is shared is a
// property of the call site.
class Registry {
 public:
  bool add_ok(Callback cb) {
    if (!cb) {
      return false;
    }
    std::lock_guard<std::mutex> g(mu_);
    slot_ = std::move(cb);
    return true;
  }

 private:
  std::mutex mu_;
  Callback slot_;
};

// An out-parameter filled under the lock and read after it. A race needs a
// caller that shares one buffer between threads.
class Store {
 public:
  void snapshot_ok(std::vector<int>& out) {
    out.clear();
    {
      std::lock_guard<std::mutex> g(mu_);
      for (int v : values_) {
        out.push_back(v);
      }
    }
    if (!out.empty()) {
      last_ = out.front();
    }
  }

 private:
  std::mutex mu_;
  std::vector<int> values_;
  int last_ = 0;
};

// CONTROL: the receiver's own fields are still reported. This is the ordinary
// shared-object race and must not be deferred to a caller.
class Receiver {
 public:
  int get_bad() { return count_; }

  void set(int v) {
    std::lock_guard<std::mutex> g(mu_);
    count_ = v;
  }

 private:
  std::mutex mu_;
  int count_ = 0;
};

// CONTROL: a by-value HANDLE to shared state. The handle is copied, but what it
// points at is shared.
struct Shared {
  int value = 0;
};
struct Handle {
  Shared* p = nullptr;
};

class HandleUser {
 public:
  // Silent on the frontier base too, so this control cannot currently detect over-deferral;
  // named for what it is rather than left to read as a passing control.
  int FN_read_through_handle(Handle h) { return h.p->value; }

  void write_through_handle(Handle h) {
    std::lock_guard<std::mutex> g(mu_);
    h.p->value = 1;
  }

 private:
  std::mutex mu_;
};

// CONTROL: a caller that really does share one buffer between the locked and
// the unlocked access is where the out-parameter race lives.
class Sharer {
 public:
  void fill(std::vector<int>& out) {
    {
      std::lock_guard<std::mutex> g(mu_);
      out.push_back(1);
    }
  }

  // The shared buffer is a member, so the actual is not owned and this caller is where the
  // deferred race should land. It does not report, and measurement says that is NOT a loss
  // caused by the deferral: the frontier base cc63c3ec99, without this change, is silent here
  // too. The race was never caught at the callee either, so nothing is moved and dropped --
  // this is a standing coverage gap, recorded so the control is not mistaken for a live one.
  int FN_use_shared_buffer_bad() {
    fill(buffer_);
    return buffer_.empty() ? 0 : buffer_.front();
  }

 private:
  std::mutex mu_;
  std::vector<int> buffer_;
};

} // namespace ownership_formals

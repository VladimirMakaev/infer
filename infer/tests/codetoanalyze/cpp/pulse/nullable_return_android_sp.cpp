/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// The nullable-return model applied to android::sp<T> / android::wp<T>.
//
// Unlike std::shared_ptr these are not modelled: get(), operator-> and operator bool are ordinary
// code reading the real m_ptr member. So the model writes m_ptr rather than the modelled backing
// pointer, and the cases below check the null then travels through that ordinary code, including
// the real copy constructor and the real assignment operator, which a model writing the backing
// pointer would never have reached.
//
// Two things here are load-bearing and easy to undo by accident:
//   - `android` is at GLOBAL scope, as it is in the real header. Nesting it inside the test
//     namespace makes the type `...::android::sp`, which the model's matcher does not match, and
//     then every case below passes while exercising nothing.
//   - the nullable-returning declarations are global and start with `dangerous`, because
//     --pulse-model-return-nullable is anchored at the start of the procedure name.

namespace android {
template <typename T>
class sp {
 public:
  sp() : m_ptr(nullptr) {}
  explicit sp(T* p) : m_ptr(p) {}
  sp(const sp& o) : m_ptr(o.m_ptr) {}
  sp& operator=(const sp& o) {
    m_ptr = o.m_ptr;
    return *this;
  }
  T* operator->() const { return m_ptr; }
  T* get() const { return m_ptr; }
  explicit operator bool() const { return m_ptr != nullptr; }

 private:
  T* m_ptr;
};

template <typename T>
class wp {
 public:
  wp() : m_ptr(nullptr) {}
  T* unsafe_get() const { return m_ptr; }

 private:
  T* m_ptr;
};
} // namespace android

namespace nullable_return_android_sp {
struct Buffer {
  int width;
};
} // namespace nullable_return_android_sp

// declared, not defined, and matched by --pulse-model-return-nullable
android::sp<nullable_return_android_sp::Buffer> dangerous_sp(int slot);
android::wp<nullable_return_android_sp::Buffer> dangerous_wp(int slot);

namespace nullable_return_android_sp {

int deref_bad(int slot) { return dangerous_sp(slot)->width; }

int get_bad(int slot) { return dangerous_sp(slot).get()->width; }

// the null has to survive the real copy constructor
int copy_ctor_bad(int slot) {
  android::sp<Buffer> a = dangerous_sp(slot);
  android::sp<Buffer> b(a);
  return b->width;
}

// ... and the real assignment operator
int assign_bad(int slot) {
  android::sp<Buffer> b;
  b = dangerous_sp(slot);
  return b->width;
}

int weak_bad(int slot) { return dangerous_wp(slot).unsafe_get()->width; }

// ------------------------------- must stay silent -------------------------------

int checked_ok(int slot) {
  android::sp<Buffer> b = dangerous_sp(slot);
  if (!b) {
    return -1;
  }
  return b->width;
}

// the check still counts after the value has been copied
int copy_then_checked_ok(int slot) {
  android::sp<Buffer> a = dangerous_sp(slot);
  android::sp<Buffer> b(a);
  if (!b) {
    return -1;
  }
  return b->width;
}

// an sp that no nullable-return function produced, and that is known non-null
int local_nonnull_ok() {
  Buffer buf;
  android::sp<Buffer> b(&buf);
  return b->width;
}

} // namespace nullable_return_android_sp

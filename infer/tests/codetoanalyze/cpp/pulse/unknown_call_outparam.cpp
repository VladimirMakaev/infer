/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
namespace unknown_call_outparam {

struct Resource {
  int use();
};

// Declared only: calls to these have no summary and may write through the slot.
void create(Resource*& out);
bool create_checked(Resource*& out);

int flag;

// A known callee that reads the slot and never writes it. Its body has to do something: a
// trivial one is modelled as a skipped call, which would make this an unknown call too.
void inspect(Resource*& out) {
  if (out == nullptr) {
    flag = 1;
  } else {
    flag = 2;
  }
}

Resource* global_resource;

// A known callee that does write the slot.
void assign(Resource*& out) { out = global_resource; }

int via_reference(Resource*& r) {
  create(r);
  return r->use();
}

int via_pointer_to_pointer(Resource** r) {
  create(*r);
  return (*r)->use();
}

int via_known_callee(Resource*& r) {
  inspect(r);
  return r->use();
}

int via_known_writer(Resource*& r) {
  assign(r);
  return r->use();
}

struct Holder {
  Resource* resource = nullptr;

  int use_member() {
    create(resource);
    return resource->use();
  }

  int use_member_checked() {
    if (!create_checked(resource)) {
      return 0;
    }
    return resource->use();
  }

  int inspect_member() {
    inspect(resource);
    return resource->use();
  }
};

int via_field_of_formal(Holder& h) {
  create(h.resource);
  return h.resource->use();
}

int call_via_reference_ok() {
  Resource* r = nullptr;
  return via_reference(r);
}

int call_via_pointer_to_pointer_ok() {
  Resource* r = nullptr;
  return via_pointer_to_pointer(&r);
}

int call_via_field_of_formal_ok() {
  Holder h;
  return via_field_of_formal(h);
}

int call_via_field_of_formal_assigned_ok() {
  Holder h;
  h.resource = nullptr;
  return via_field_of_formal(h);
}

int call_member_unknown_ok() {
  Holder h;
  return h.use_member();
}

int call_member_unknown_checked_ok() {
  Holder h;
  return h.use_member_checked();
}

int call_via_known_writer_ok() {
  Resource* r = nullptr;
  return via_known_writer(r);
}

int call_via_known_callee_bad() {
  Resource* r = nullptr;
  return via_known_callee(r);
}

int call_member_known_callee_bad() {
  Holder h;
  return h.inspect_member();
}

int plain_null_bad() {
  Resource* r = nullptr;
  return r->use();
}

} // namespace unknown_call_outparam

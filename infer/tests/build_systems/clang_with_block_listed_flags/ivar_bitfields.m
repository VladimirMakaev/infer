/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

__attribute__((objc_root_class))
@interface BitFieldContainer {
 @public
  unsigned first : 3;
  unsigned : 2;
  unsigned : 0;
  unsigned second : 4;
  unsigned : 0;
}
@property int value;
@end

@interface BitFieldContainer () {
  unsigned : 1;
  unsigned extension_value : 2;
}
@end

@implementation BitFieldContainer {
  unsigned : 1;
  unsigned implementation_value : 2;
}
@synthesize value;
@end

int objc_bitfields_read_ok(BitFieldContainer* object) {
  return object->first + object->second;
}

int objc_bitfields_null_bad(void) {
  BitFieldContainer* object = 0;
  return object->first;
}

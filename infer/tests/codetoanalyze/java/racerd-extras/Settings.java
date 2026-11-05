/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package codetoanalyze.java.checkers;

/**
 * Declares a static field but races on none itself: the races on it live in Caller.java. Reports on
 * this field must name Settings as the declaring class even though they are filed against another
 * file, which is the case a consumer grouping by field cannot get from the report's own class.
 */
class Settings {
  static Object instance;
}

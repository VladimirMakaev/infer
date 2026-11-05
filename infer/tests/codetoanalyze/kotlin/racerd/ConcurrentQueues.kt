/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

import java.util.Queue
import java.util.concurrent.ConcurrentLinkedQueue

class ConcurrentQueueConcrete {
  private val queue = ConcurrentLinkedQueue<String>()

  @Synchronized fun locked(value: String) {
    queue.add(value)
  }
  fun add_ok(value: String) {
    queue.add(value)
  }
  fun clear_ok() {
    queue.clear()
  }
}

class ConcurrentQueueInterface {
  private val queue: Queue<String> = ConcurrentLinkedQueue<String>()

  @Synchronized fun locked(value: String) {
    queue.add(value)
  }
  fun add_ok(value: String) {
    queue.add(value)
  }
  fun clear_ok() {
    queue.clear()
  }
}

class ConcurrentQueueSharedList {
  private val queue = ArrayList<String>()

  @Synchronized fun locked(value: String) {
    queue.add(value)
  }
  fun add_bad(value: String) {
    queue.add(value)
  }
  fun clear_bad() {
    queue.clear()
  }
}

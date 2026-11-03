/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package kotlinx.coroutines

import com.facebook.infer.annotation.ThreadSafe

// Minimal declarations preserve the library's suspend-interface call shape.
interface Job
interface Deferred<T> : Job { suspend fun await(): T }
interface CompletableDeferred<T> : Deferred<T> { fun complete(value: T): Boolean }
interface UnannotatedTask<T> { suspend fun await(): T }

@ThreadSafe
class CoroutineAwait(
    private val initialized: CompletableDeferred<Unit>,
    private val unannotated: UnannotatedTask<Unit>,
) {
  suspend fun initializedAwait_ok() { initialized.await() }
  suspend fun unannotatedAwait_bad() { unannotated.await() }
}

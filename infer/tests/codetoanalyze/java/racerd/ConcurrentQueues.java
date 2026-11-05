/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package codetoanalyze.java.checkers;

import com.google.common.collect.Queues;
import java.util.AbstractQueue;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.Queue;
import java.util.concurrent.ConcurrentLinkedDeque;
import java.util.concurrent.ConcurrentLinkedQueue;

public class ConcurrentQueues {
  public static class Concrete {
    private static final ConcurrentLinkedQueue<String> queue = new ConcurrentLinkedQueue<>();

    public synchronized void locked(String value) {
      queue.add(value);
    }
    public void add_ok(String value) {
      queue.add(value);
    }
    public void clear_ok() {
      queue.clear();
    }
  }

  public static class Declared {
    private ConcurrentLinkedQueue<String> queue;

    public synchronized void locked(String value) {
      queue.add(value);
    }
    public void add_ok(String value) {
      queue.add(value);
    }
    // clear is declared by AbstractQueue in the captured library model.
    public void clear_ok() {
      queue.clear();
    }
  }

  public static class Interface {
    private static final Queue<String> queue = new ConcurrentLinkedQueue<>();

    public synchronized void locked(String value) {
      queue.add(value);
    }
    public void add_ok(String value) {
      queue.add(value);
    }
    public void clear_ok() {
      queue.clear();
    }
  }

  public static class Abstract {
    private static final AbstractQueue<String> queue = new ConcurrentLinkedQueue<>();

    public synchronized void locked(String value) {
      queue.add(value);
    }
    public void add_ok(String value) {
      queue.add(value);
    }
    public void clear_ok() {
      queue.clear();
    }
  }

  public static class Deque {
    private static final ConcurrentLinkedDeque<String> queue = new ConcurrentLinkedDeque<>();

    public synchronized void locked(String value) {
      queue.add(value);
    }
    public void add_ok(String value) {
      queue.add(value);
    }
    public void clear_ok() {
      queue.clear();
    }
  }

  public static class Factory {
    private static final Queue<String> queue = Queues.newConcurrentLinkedQueue();

    public synchronized void locked(String value) {
      queue.add(value);
    }
    public void add_ok(String value) {
      queue.add(value);
    }
    public void clear_ok() {
      queue.clear();
    }
  }

  public static class SharedList {
    private static final ArrayList<String> queue = new ArrayList<>();

    public synchronized void locked(String value) {
      queue.add(value);
    }
    public void add_bad(String value) {
      queue.add(value);
    }
    public void clear_bad() {
      queue.clear();
    }
  }

  public static class UnknownQueue {
    private Queue<String> queue;

    public synchronized void locked(String value) {
      queue.add(value);
    }
    public void add_bad(String value) {
      queue.add(value);
    }
    public void clear_bad() {
      queue.clear();
    }
  }

  public static class UnknownAbstractQueue {
    private AbstractQueue<String> queue;

    public synchronized void locked(String value) {
      queue.add(value);
    }
    public void add_bad(String value) {
      queue.add(value);
    }
    public void clear_bad() {
      queue.clear();
    }
  }

  public static class UnsafeQueue extends AbstractQueue<String> {
    private final ArrayList<String> values = new ArrayList<>();

    @Override public boolean offer(String value) {
      return values.add(value);
    }
    @Override public String poll() {
      return values.remove(0);
    }
    @Override public String peek() {
      return values.get(0);
    }
    @Override public Iterator<String> iterator() {
      return values.iterator();
    }
    @Override public int size() {
      return values.size();
    }
  }

  public static class Custom {
    private static final UnsafeQueue queue = new UnsafeQueue();

    public synchronized void locked(String value) {
      queue.add(value);
    }
    public void add_bad(String value) {
      queue.add(value);
    }
    public void clear_bad() {
      queue.clear();
    }
  }

  // A concurrent queue's contract protects its queue state, not arbitrary user fields.
  public static class ExtendedQueue extends ConcurrentLinkedQueue<String> {
    private int count;

    public synchronized void lockedCount() {
      count++;
    }
    public void count_bad() {
      count++;
    }
  }
}

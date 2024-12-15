#ifndef WAITFREEATOMICSNAPSHOT_HPP
#define WAITFREEATOMICSNAPSHOT_HPP

// Wait-free Atomic Snapshot

// At first scan all registers. Then scan again to check whether this snapshot is atomic.
// If these two snapshot are same, choose it as a snapshot.
// Otherwise, scan again until finding a register that has been modified twice.

// All writers need to get an atomic snapshot before changing a value.
// So, if there is a register that has been modified twice during the scan, the writer who modified the register will be sure to take a snapshot after that scan is executed.
// That is, the writer who has been changed this register twice must have a snapshot which is created after scan's linearization point.


#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "AtomicRegister.hpp"


constexpr std::size_t hardware_destructive_interference_size = 64; // To avoid false sharing, set the cache line's size.


class Snapshot
{
public:

  // Constructor with thread count, set the size of atomic_register_vector to the thread_count.
  Snapshot(int thread_count) { atomic_register_vector.resize(thread_count); }

  // Constructor with register, using copy.
  Snapshot(std::vector<AtomicRegister>& registers) : atomic_register_vector(registers) {}

  // Constructor with register, using move.
  Snapshot(std::vector<AtomicRegister>&& registers) : atomic_register_vector(std::move(registers)) {}

  // Copy constructor, only copy the atomic registers.
  Snapshot(const Snapshot& s) { atomic_register_vector = s.atomic_register_vector; }

  // Move contructor, only move the atomic registers.
  Snapshot(const Snapshot&& s) { atomic_register_vector = std::move(s.atomic_register_vector); }

  // Access to the ith atomic register.
  AtomicRegister& operator[](int i) { return atomic_register_vector[i]; }

  // Copy the snapshot into this snapshot, using copy.
  void operator=(Snapshot& s) { atomic_register_vector = s.atomic_register_vector; inner_cnt.store(0); recycle_flag = false; }

  // Copy the snapshot into this snapshot, using move.
  void operator=(Snapshot&& s) { atomic_register_vector = std::move(s.atomic_register_vector); inner_cnt.store(0); recycle_flag = false; }

  // Get the  recycle flag
  bool get_recycle_flag() { return recycle_flag; }

  // Release the snapshot. If there are no threads referencing this snapshot, set the recyle flag.
  void release();

  // Reset the reference count. It is used for exchange() of shared_snapshot.
  void reset(int reset_cnt);

private:

  // Control reference count to avoid recycling during use.
  alignas(hardware_destructive_interference_size) std::atomic<int> inner_cnt{0};  // To avoid false sharing, use alignas keyword.

  // Atomic registers that are captured to this snapshot.
  std::vector<AtomicRegister> atomic_register_vector;

  // Instead of deallocating, recycle it.
  bool recycle_flag = false;

};

class shared_snapshot
{
public:

  // Constructor with version count. 
  // It is recommended that this value be larger than the number of atomic registers to guarantee the wait-free. (thread count + 1)
  shared_snapshot(const int version_count);

  // Copy constructor
  shared_snapshot(shared_snapshot& ss) : version_count(ss.get_version_count()) { this->snapshot_ptr_vector = ss.snapshot_ptr_vector; this->outer_cnt_with_index.store(ss.outer_cnt_with_index);}

  // Move constructor
  shared_snapshot(shared_snapshot&& ss) : version_count(ss.get_version_count()) { this->snapshot_ptr_vector = std::move(ss.snapshot_ptr_vector); this->outer_cnt_with_index.store(ss.outer_cnt_with_index);}

  // Deallocate all snapshots.
  ~shared_snapshot();

  // Install new snapshot.
  void exchange(Snapshot& snapshot);

  // Install new snapshot, using move.
  void exchange(Snapshot&& snapshot);

  // Get version count.
  int get_version_count() { return version_count; }

  // Access to the snapshot with increasing reference count.
  Snapshot& acquire();

private:

  // Control block which contains reference count and index in 8byte.
  alignas(hardware_destructive_interference_size) std::atomic<uint64_t> outer_cnt_with_index{0}; // To avoid false sharing, use alignas keyword.

  // The pointers of snapshot versions.
  std::vector<Snapshot*> snapshot_ptr_vector;

  // Thread count.
  const int version_count;

};

class WaitfreeAtomicSnapshot
{
public:

  // Constructor with thread count. This count is used for makding atomic register count.
  WaitfreeAtomicSnapshot(const int thread_count);

  // Register the thread id to it's index.
  int RegisterTid(std::thread::id tid);

  // Get the atomic snapshot.
  Snapshot scan();

  // Update the value of atomic register. If the caller knows it's index, give it as an argument.
  void update(int value, int index = -1);

private:

  // SWMR Atomic registers.
  std::vector<AtomicRegister> atomic_register_vector;

  // Atomic snapshots held by each writer.
  std::vector<shared_snapshot> shared_snapshot_vector;

  // Hash table. Key is the thread id and value is the index.
  std::unordered_map<std::thread::id, int> tid_to_index;

  // Mutex to protect inserting of hash table.
  std::mutex hash_mutex;

  // Used in making index of threads.
  int tid_index = 0;

  // Thread count.
  const int thread_count;

};


#endif  // WAITFREEATOMICSNAPSHOT_HPP
// Writers have their own snapshot and this snapshot can be read by multiple readers.
// So if the writers just deallocate existing snapshots to replace it, the readers may access to the wrong memory.

// To avoid accessing the wrong memeory, use multiple versions of snapshot.
// The snapshots are managed by shared_snapshot object.

// shared_snapshot has 8byte control block which contains outer reference counter and index of current version of snapshot.
// If some readers want to access current version, they will increase outer reference counter by using fetch_add().
// When the fetch_add() is returned, lower 4byte of that return value represent the index of the snapshot whose reference count is increased.
// So, The reader can increment the reference counter while obtaining information about the index at the same time.

// After the reader used the acquired snapshot. It must be released.
// When releasing, the reader increases the inner reference counter by 1.
// If increased counter is 0, it means that there are no other threads who are referencing this snapshot.
// Then, set the recycle bit for recycling.

// But why we increase the inner reference counter in releasing phase?
// In replacing version of snapshot, the writer will exchange the 8byte control block to the new one atomically using exchange().
// Then it will return the old 8byte control block. It has old reference count and old index of old version.

// Because this change occured atomically, other new readers can't access to this old version anymore.
// In here, the writer will decrease the inner counter of old snapshot as much as old control block's outer reference count.
// So when increasing inner reference count, if increased inner count is 0, it means that this thread is the last thread who used that snapshot.

// cf) Why divide the reference counter into two?
// Because it is only possible to increment the reference counter in an 8-byte control block, but it is not possible to decrement.
// The reader wants to decrease the reference count at the end. But the writer may changed the control block to the other index, so the reader can't use it.
// So, the reader must notice to the other reference counter, inner counter.


#include <iostream>

#include "../include/WaitfreeAtomicSnapshot.hpp"


#define REFERENCE_CNT_INC                          ((uint64_t)(0x0000000100000000))

#define REFERENCE_CNT_MASK(ref_cnt_with_index)     ((uint64_t)(0xffffffff00000000) & static_cast<uint64_t>(ref_cnt_with_index))

#define EXTRACT_REFERENCE_CNT(ref_cnt_with_index)  ((int)(REFERENCE_CNT_MASK(ref_cnt_with_index) >> 32))

#define INDEX_MASK(ref_cnt_with_index)             ((uint64_t)(0x00000000ffffffff) & static_cast<uint64_t>(ref_cnt_with_index))


// Release the snapshot. If there are no threads referencing this snapshot, set the recyle flag.
void Snapshot::release()
{
  int remain_cnt = inner_cnt.fetch_add(1) + 1;

  if (remain_cnt == 0)
  {
    recycle_flag = true;
  }
}

// Reset the reference count. It is used to exchange of shared_snapshot.
void Snapshot::reset(int reset_cnt)
{
  int remain_cnt = inner_cnt.fetch_sub(reset_cnt) - reset_cnt;

  if (remain_cnt == 0)
  {
    recycle_flag = true;
  }
}

// Constructor with version count. 
// It is recommended that this value be larger than the number of atomic registers to guarantee the wait-free. (thread count + 1)
shared_snapshot::shared_snapshot(const int version_count) : version_count(version_count)
{
  snapshot_ptr_vector.reserve(version_count);

  for (int i = 0; i < version_count; i++)
  {
    snapshot_ptr_vector.push_back(nullptr);
  }
}

// Deallocate all snapshots.
shared_snapshot::~shared_snapshot()
{
  for (int i = 0; i < version_count; i++)
  {
    if (snapshot_ptr_vector[i])
    {
      delete snapshot_ptr_vector[i];
    }
  }
}

// Install new snapshot.
void shared_snapshot::exchange(Snapshot& snapshot)
{
  uint64_t i = 0;

  uint64_t old_ref_cnt_with_index = 0;

  uint64_t old_index = 0;

  int old_ref_cnt = 0;


  // Find the empty index.

  for (i = 0; i < version_count; i++)
  {
    if (snapshot_ptr_vector[i] == nullptr || snapshot_ptr_vector[i]->get_recycle_flag())
      break;
  }

  assert(i != version_count); // If version count is bigger than the number of atomic registers, search can be done in a one loop.


  // Push new snapshot into that index

  if (snapshot_ptr_vector[i] == nullptr)
  {
    snapshot_ptr_vector[i] = new Snapshot(snapshot);
  }
  else
  {
    *snapshot_ptr_vector[i] = snapshot;
  }


  // Change the current version into that snapshot and get the old control block.

  old_ref_cnt_with_index = outer_cnt_with_index.exchange(i);

  old_ref_cnt = EXTRACT_REFERENCE_CNT(old_ref_cnt_with_index);

  assert(old_ref_cnt >= 0);

  old_index = INDEX_MASK(old_ref_cnt_with_index);


  // Reset the inner reference count of old version.

  if (old_index != i)
    snapshot_ptr_vector[old_index]->reset(old_ref_cnt);
}

// Install new snapshot, using move.
void shared_snapshot::exchange(Snapshot&& snapshot)
{
  uint64_t i = 0;

  uint64_t old_ref_cnt_with_index = 0;

  uint64_t old_index = 0;

  int old_ref_cnt = 0;


  // Find the empty index.

  for (i = 0; i < version_count; i++)
  {
    if (snapshot_ptr_vector[i] == nullptr || snapshot_ptr_vector[i]->get_recycle_flag())
      break;
  }

  assert(i != version_count); // If version count is bigger than the number of atomic registers, search can be done in a one loop.


  // Push new snapshot into that index

  if (snapshot_ptr_vector[i] == nullptr)
  {
    snapshot_ptr_vector[i] = new Snapshot(std::move(snapshot));
  }
  else
  {
    *snapshot_ptr_vector[i] = std::move(snapshot);
  }


  // Change the current version into that snapshot and get the old control block.

  old_ref_cnt_with_index = outer_cnt_with_index.exchange(i);

  old_ref_cnt = EXTRACT_REFERENCE_CNT(old_ref_cnt_with_index);

  assert(old_ref_cnt >= 0);

  old_index = INDEX_MASK(old_ref_cnt_with_index);


  // Reset the inner reference count of old version.

  if (old_index != i)
    snapshot_ptr_vector[old_index]->reset(old_ref_cnt);
}

// Access to the snapshot with increasing reference count.
Snapshot& shared_snapshot::acquire()
{
  uint64_t ref_cnt_with_index = outer_cnt_with_index.fetch_add(REFERENCE_CNT_INC);

  uint64_t index = INDEX_MASK(ref_cnt_with_index);

  return *snapshot_ptr_vector[index];
}

// Constructor with thread count. This count is used for makding atomic register count.
WaitfreeAtomicSnapshot::WaitfreeAtomicSnapshot(const int thread_count) : thread_count(thread_count)
{
  atomic_register_vector.reserve(thread_count);

  shared_snapshot_vector.reserve(thread_count);

  for (int i = 0; i < thread_count; i++)
  {
    atomic_register_vector.emplace_back();

    shared_snapshot_vector.emplace_back(thread_count + 1);
  }
}  

// Register the thread id to it's index.
int WaitfreeAtomicSnapshot::RegisterTid(std::thread::id tid) 
{ 
  std::unique_lock<std::mutex> lock(hash_mutex);

  int index = tid_index++;

  tid_to_index.insert(std::pair<std::thread::id, int>(tid, index)); 
    
  return index; 
}

// Get the atomic snapshot.
Snapshot WaitfreeAtomicSnapshot::scan()
{
  std::vector<int> change_count_vector(thread_count);

  Snapshot first_snapshot(atomic_register_vector);

  Snapshot second_snapshot(thread_count);


  // Scan all atomic registers until get an atomic snapshot.

  bool same_flag = true;
  
  while (true)
  {
    for (int i = 0; i < thread_count; i++)
    {
      // Build the second snapshot.

      second_snapshot[i] = atomic_register_vector[i];
      
      // If the previously copied register and the current register are different, set the flag to false.

      if (first_snapshot[i] != second_snapshot[i])
      {
        same_flag = false;

        // But if this register is changed twice, it means that this writer thread has proper atomic snapshot, return it.

        if (++change_count_vector[i] == 2)
        {
          Snapshot& writer_snapshot = shared_snapshot_vector[i].acquire();
        
          first_snapshot = writer_snapshot;
        
          writer_snapshot.release();

          return std::move(first_snapshot);
        }
      }
    }

    // If the atomic snapshot was built successfully, return it.

    if (same_flag)
      return std::move(first_snapshot);

    // Otherwise, loop again with second snapshot

    first_snapshot = second_snapshot;

    same_flag = true;
  }
}

// Update the value of atomic register. If the caller knows it's index, give it as an argument.
void WaitfreeAtomicSnapshot::update(int value, int index)
{
  // If the thread alraedy knew its index, use it. Otherwise, find the index

  index = index == -1 ? tid_to_index[std::this_thread::get_id()] : index;


  // Before updating, get the snapshot and replace it

  shared_snapshot_vector[index].exchange(std::move(scan()));
    

  // Change the value of atomic register

  atomic_register_vector[index].write(value);
}


#undef REFERENCE_CNT_INC     // ((uint64_t)(0x0000000100000000))

#undef REFERENCE_CNT_MASK    // ((uint64_t)(0xffffffff00000000) & static_cast<uint64_t>(ref_cnt_with_index))

#undef EXTRACT_REFERENCE_CNT // (REFERENCE_CNT_MASK(ref_cnt_with_index) >> 32)

#undef INDEX_MASK            // ((uint64_t)(0x00000000ffffffff) & static_cast<uint64_t>(ref_cnt_with_index))
// Deadlock means that more than two transactions are waiting for each other.
// In acquire(), we got a vector containing the locks that the new_lock might be waiting for.
// For each lock in that vector, we will check whether the transaction of that locks is waiting for our new transaction.

// For this check, it is necessary to know where that transactions are conflicting.
// However, because we do not use global mutex, that conflict lock's location may change during deadlock checking.
// That is, the location viewed in the deadlock checking and the actual location may be different.
// So it can be a problem?

// The important thing in deadlock checking is that it doesn't matter determining a non-deadlock as a deadlock. Because you can try again.
// But it's a problem to decide that a real deadlock as a non-deadlock.

// If conflict lock is changed while deadlock checking, this transaction was not a deadlock with a new transaction until that point.
// Because if it was a deadlock, it can't change its states.
// Then, does the fact that the conflict lock has changed guarantees that it won't create a new deadlock until the deadlock check is done?

// No. A new deadlock can be created by a changing conflict lock during the deadlock checking.
// Let's assume that the conflict lock is changed, but deadlock checker can't recognize it.
// So a new wait cycle has been created, but the deadlock checker is not aware of it.
// Then the deadlock checker will tell us 'there are no deadlock', even if the new deadlock is created.

// But do you remember we first setup our conflict lock before deadlock checking in Aquire()?
// Because we pre-set the conflict lock before the deadlock check and we won't touch it until deadlock check is done, our conflict lock is fixed during the deadlock check. 
// If the above situation occurs, our deadlock checking will not find a new created deadlock,
// But this is not a problem because the transaction that changed the conflict lock will look at our fixed conflict lock, then it will find a new deadlock on its deadlock checking.

#include <assert.h>
#include <iostream>
#include <queue>
#include <set>

#include "../include/LockManager.hpp"
#include "../include/TrxManager.hpp"


extern TrxManager trx_manager;


// Deadlock check
bool LockManager::IsDeadlock(std::vector<lock_t*>& waiting_lock_vector)
{
  TrxNode* trx = nullptr;

  lock_t* target = nullptr;

  uint64_t local_lock_id_on_record = 0;

  lock_t* local_conflict_lock = nullptr;

  LockTableNode* lock_table_node = nullptr;

  std::queue<lock_t*> wait_lock_queue;

  std::set<std::thread::id> pass_id_set;


  // Before searching, push the waiting locks into queue

  for (int i = waiting_lock_vector.size() - 1; i >= 0; i--)
  {
    target = waiting_lock_vector[i];

    if (target->state != lock_t::State::OBSOLETE)
      wait_lock_queue.push(target);
  }


  // Search until there are no waiting locks

  while (!wait_lock_queue.empty())
  {
    // Pop the wait lock

    target = wait_lock_queue.front();

    wait_lock_queue.pop();


    // If this target transaction already checked or target lock is removed, skip it.

    if (pass_id_set.count(target->thread_id) || target->state == lock_t::State::OBSOLETE)
      continue;


    // Go to the comflict lock

    trx = trx_manager.GetTrxNode(target->thread_id);

    local_conflict_lock = trx->conflict_lock.load();

    if (local_conflict_lock == nullptr)
    {
      pass_id_set.insert(trx->thread_id);
      
      continue;
    }

    local_lock_id_on_record = local_conflict_lock->lock_id_on_record.load();


    // Gather new waiting locks from the list where the confilct lock is located.

    std::vector<lock_t*> tmp_wait_lock_vector;

    lock_table_node = lock_table[local_conflict_lock->record_id];

    //int local_record_id = local_conflict_lock->record_id; // Commented code : Non record by record recycling method

    //lock_table_node = lock_table[local_record_id]; // Commented code : Non record by record recycling method

    target = lock_table_node->head.load();

    while (target != local_conflict_lock)
    {
      // If conflict lock is changed, it means that the execution flow is still in progress.
      // It means that the execution flow is not blocked, which means it is not a deadlock.
      // So there is no need to add a new wait lock here.

      if (local_conflict_lock != trx->conflict_lock.load() || local_conflict_lock->lock_id_on_record.load() != local_lock_id_on_record) // || local_conflict_lock->record_id != local_record_id) // Commented code : Non record by record recycling method
      {
        tmp_wait_lock_vector.clear();

        break;
      }

      
      // Deadlock is detected

      if (target->thread_id == std::this_thread::get_id() && target->state != lock_t::State::OBSOLETE)
        return true;
      
      
      // This target lock is not a deadlock, save it temporarily.
      // Then move to the next.
      
      tmp_wait_lock_vector.push_back(target);

      target = target->next;


      // If the moved lock is logically behind the conflict lock, restart from head.

      if (target == nullptr || target->lock_id_on_record.load() > local_lock_id_on_record) // || target->record_id != local_record_id) // Commented code : Non record by record recycling method
      {
        tmp_wait_lock_vector.clear();

        target = lock_table_node->head.load();

        // If head is moved to behind the target id, it means that target lock is alredy released.
        // So no need to wait.

        if (target->lock_id_on_record.load() >= local_lock_id_on_record)
          break;
      }
    }

    for (int i = 0; i < tmp_wait_lock_vector.size(); i++)
      wait_lock_queue.push(tmp_wait_lock_vector[i]);


    // There are no deadlock in this transaction.

    pass_id_set.insert(trx->thread_id);
  }

  // no deadlock

  return false;
}

// Deadlock check, global mutex version
// When this function is called, the global mutex is acquired.
bool LockManager::IsDeadlock2(std::vector<lock_t*>& waiting_lock_vector)
{
  TrxNode* trx = nullptr;

  lock_t* target = nullptr;

  lock_t* local_conflict_lock = nullptr;

  LockTableNode* lock_table_node = nullptr;

  std::queue<lock_t*> wait_lock_queue;

  std::set<std::thread::id> pass_id_set;


  // Before searching, push the waiting locks into queue

  for (int i = waiting_lock_vector.size() - 1; i >= 0; i--)
  {
    target = waiting_lock_vector[i];

    if (target->state != lock_t::State::OBSOLETE)
      wait_lock_queue.push(target);
  }


  // Search until there are no waiting locks

  while (!wait_lock_queue.empty())
  {
    // Pop the wait lock

    target = wait_lock_queue.front();

    wait_lock_queue.pop();


    // If this target transaction already checked or target lock is removed, skip it.

    if (pass_id_set.count(target->thread_id) || target->state == lock_t::State::OBSOLETE)
      continue;


    // Go to the comflict lock

    trx = trx_manager.GetTrxNode(target->thread_id);

    local_conflict_lock = trx->conflict_lock.load();

    if (local_conflict_lock == nullptr)
    {
      pass_id_set.insert(trx->thread_id);
      
      continue;
    }


    // Gather new waiting locks from the list where the confilct lock is located.

    std::vector<lock_t*> tmp_wait_lock_vector;

    lock_table_node = lock_table[local_conflict_lock->record_id];

    target = lock_table_node->head.load();

    while (target != local_conflict_lock)
    {
      // Deadlock is detected

      if (target->thread_id == std::this_thread::get_id() && target->state != lock_t::State::OBSOLETE)
        return true;
      
      
      // This target lock is not a deadlock, save it temporarily.
      // Then move to the next.
      
      tmp_wait_lock_vector.push_back(target);

      target = target->next;

      assert(target != nullptr);
    }

    for (int i = 0; i < tmp_wait_lock_vector.size(); i++)
      wait_lock_queue.push(tmp_wait_lock_vector[i]);


    // There are no deadlock in this transaction.

    pass_id_set.insert(trx->thread_id);
  }

  // no deadlock

  return false;
}
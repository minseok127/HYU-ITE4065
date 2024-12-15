#include <assert.h>
#include <iostream>

#include "../include/LockManager.hpp"


LockManager lock_manager;


// Constructor. Make records.
void LockManager::MakeRecords(int record_count)
{
  this->record_count = record_count;

  LockTableNode* node = nullptr;

  // Record id starts from 1

  for (int i = 1; i <= record_count; i++)
  {
    node = new LockTableNode(i);

    lock_table.insert(std::make_pair(i, node));
  }
}

// Return the value of record
int64_t LockManager::GetRecord(lock_t* lock)
{
  assert(lock->state != lock_t::State::OBSOLETE);

  return lock_table[lock->record_id]->record_value;
}

// Change the value of record and get the changed value
int64_t LockManager::ChangeRecord(lock_t* lock, int64_t diff)
{
  assert(lock->mode == lock_t::Mode::EXCLUSIVE && lock->state != lock_t::State::OBSOLETE); // Change is only possible when the lock mode is EXCLUSIVE.

  return (lock_table[lock->record_id]->record_value += diff);
}
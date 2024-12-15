#ifndef TRX_HPP
#define TRX_HPP

#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

#include "Lock.hpp"


// Transaction data structure
struct TrxNode
{
  // Each transaction has corresponding thraed id
  TrxNode(const std::thread::id thread_id) : thread_id(thread_id) {}

  // Deque of locks that have been acquired
  std::deque<lock_t*> trx_lock_deque;

  // Lock waiting for a transaction to acquire
  // To use memory barrier, allocate it as an atomic type
  std::atomic<lock_t*> conflict_lock{nullptr};

  // Mutex to synchronize sleep and wake up
  std::mutex mutex;

  // Conditional variable to wait
  std::condition_variable cond;

  // Thread id corresponding this transaction
  const std::thread::id thread_id;

  // thread#
  int thread_number = -1;
};

#endif  // TRX_HPP
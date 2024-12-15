#include <algorithm>
#include <assert.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#include <queue>
#include <thread>
#include <vector>

#include "../include/LockManager.hpp"
#include "../include/TrxManager.hpp"


extern TrxManager trx_manager;

extern LockManager lock_manager;

extern thread_local int thread_local_recycled_lock_count;

extern thread_local int thread_local_total_created_lock_count;

std::atomic<int> global_recycled_lock_count(0);

std::atomic<int> global_total_created_lock_count(0);

void InitManagers(int last_global_execution_order, int record_count);

void RepeatTrx(int record_count);

void RepeatTrx2(int record_count, std::mutex& global_mutex);

bool IsCorrect(int record_count, int thread_count, int last_global_execution_order);


int main(const int argc, const char* argv[])
{
  // Recieve arguments

  if (argc < 4)
  {
    std::cout << "Not enough argument" << std::endl;

    return 0;
  }

  int thread_count = std::stoi(argv[1]);

  int record_count = std::stoi(argv[2]);

  int last_global_execution_order = std::stoi(argv[3]);

  std::vector<std::thread> thread_vector;

  std::mutex global_mutex; // Only used for RepeatTrx2()
  

  // Initialize lock manager and transaction manager.

  InitManagers(last_global_execution_order, record_count);


  // Test start
  
  auto start = std::chrono::system_clock::now();

  for (int i = 0; i < thread_count; i++)
  {
    thread_vector.emplace_back(std::thread(RepeatTrx, record_count)); // No global mutex version
    
    //thread_vector.emplace_back(std::thread(RepeatTrx2, record_count, std::ref(global_mutex))); // Global mutex version
  }

  for (int i = 0; i < thread_count; i++)
  {
    thread_vector[i].join();
  }

  auto end = std::chrono::system_clock::now();

  auto timecount = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);


  // Print results

  std::cout << "#########################################################################################" << std::endl;

  std::cout << "1. The number of threads : " << thread_count << std::endl;

  std::cout << "2. The number of records : " << record_count << std::endl;

  std::cout << "3. Last commit ID : " << last_global_execution_order << std::endl;

  std::cout << "4. Throughput (total number of commits / miliseconds) : " << (double)last_global_execution_order / timecount.count() << std::endl;

  std::cout << "5. Correctness : " << (IsCorrect(record_count, thread_count, last_global_execution_order) ? "true" : "false") << std::endl;

  std::cout << "6. Percentage of Recycled Locks : " << 100 * (double)global_recycled_lock_count / global_total_created_lock_count << std::endl;

  std::cout << "#########################################################################################" << std::endl;

  return 0;
}

void InitManagers(int last_global_execution_order, int record_count)
{
  lock_manager.MakeRecords(record_count);

  trx_manager.SetLastCommitId(last_global_execution_order);
}

void RepeatTrx(int record_count)
{
  int i = 0, j = 0, k = 0, commit_id = 0;

  int64_t record_i, record_j, record_k;

  std::random_device rd;

  std::mt19937 gen(rd());

  std::uniform_int_distribution<int> dis(1, record_count);


  // Repeat transaction until the commit id exceeds the maximum commit id

  while (true)
  {
    // Randomly pick up three different records i, j, k, respectively.

    i = dis(gen); j = dis(gen); k = dis(gen);

    if (i == j || i == k || j == k)
      continue;


    // Transaction begin

    trx_manager.Begin();


    // Read Ri

    if (trx_manager.Find(i, &record_i) == TrxManager::Response::MUST_ABORTED)
    {
      trx_manager.Abort();

      continue;
    }


    // Increase the value of Rj by (1 + Ri), i.e., Rj = Rj + Ri + 1.

    if (trx_manager.Update(j, record_i + 1, &record_j) == TrxManager::Response::MUST_ABORTED)
    {
      trx_manager.Abort();

      continue;
    }


    // Decrease Rk by Ri. i.e., Rk = Rk - Ri.

    if (trx_manager.Update(k, -record_i, &record_k) == TrxManager::Response::MUST_ABORTED)
    {
      trx_manager.Abort();

      continue;
    }


    // Transaction commit

    commit_id = trx_manager.Commit();

    if (commit_id == -1)
      break;
  }  

  global_recycled_lock_count.fetch_add(thread_local_recycled_lock_count);

  global_total_created_lock_count.fetch_add(thread_local_total_created_lock_count);
}

void RepeatTrx2(int record_count, std::mutex& global_mutex)
{
  int i = 0, j = 0, k = 0, commit_id = 0;

  int64_t record_i, record_j, record_k;

  std::random_device rd;

  std::mt19937 gen(rd());

  std::uniform_int_distribution<int> dis(1, record_count);


  // Repeat transaction until the commit id exceeds the maximum commit id

  while (true)
  {
    // Randomly pick up three different records i, j, k, respectively.

    i = dis(gen); j = dis(gen); k = dis(gen);

    if (i == j || i == k || j == k)
      continue;


    // Transaction begin

    trx_manager.Begin();


    // Read Ri

    if (trx_manager.Find(i, &record_i, global_mutex) == TrxManager::Response::MUST_ABORTED)
    {
      trx_manager.Abort(global_mutex);

      continue;
    }


    // Increase the value of Rj by (1 + Ri), i.e., Rj = Rj + Ri + 1.

    if (trx_manager.Update(j, record_i + 1, &record_j, global_mutex) == TrxManager::Response::MUST_ABORTED)
    {
      trx_manager.Abort(global_mutex);

      continue;
    }


    // Decrease Rk by Ri. i.e., Rk = Rk - Ri.

    if (trx_manager.Update(k, -record_i, &record_k, global_mutex) == TrxManager::Response::MUST_ABORTED)
    {
      trx_manager.Abort(global_mutex);

      continue;
    }


    // Transaction commit

    commit_id = trx_manager.Commit(global_mutex);

    if (commit_id == -1)
      break;
  }  

  global_recycled_lock_count.fetch_add(thread_local_recycled_lock_count);

  global_total_created_lock_count.fetch_add(thread_local_total_created_lock_count);
}

struct Log
{
  bool operator<(Log l) { return this->commit_id < l.commit_id; }

  int commit_id = -1;

  int i = -1, j = -1, k = -1;
  
  int64_t vi = -1, vj = -1, vk = -1;
};

bool IsCorrect(int record_count, int thread_count, int last_global_execution_order)
{
  int i_index = -1;

  int j_index = -1;

  int k_index = -1;

  std::vector<Log> log_vector;

  int64_t* record_table = new int64_t[record_count];


  // Initailzie all record's value to 100

  for (int i = 0; i < record_count; i++)
  {
    record_table[i] = 100;
  }


  // Read commit logs from thread#.txt files.

  for (int tid = 1; tid <= thread_count; tid++)
  {
    Log commit_log;

    std::string file_name = "thread" + std::to_string(tid) + ".txt";

    std::ifstream fin(file_name);

    while (!fin.eof()) 
    {
      fin >> commit_log.commit_id;

      if (fin.eof()) 
        break;

      fin >> commit_log.i >> commit_log.j >> commit_log.k >> commit_log.vi >> commit_log.vj >> commit_log.vk;
      
      log_vector.push_back(commit_log);
    }
  }

  assert(log_vector.size() == last_global_execution_order);


  // Sort the vector in ascending order by commit id

  std::sort(log_vector.begin(), log_vector.end());


  // Compare commit log's value results with real record table.
  // If there is a difference, return false.

  for (Log& log : log_vector)
  {    
    i_index = log.i - 1;

    j_index = log.j - 1;

    k_index = log.k - 1;

    record_table[j_index] += record_table[i_index] + 1;

    record_table[k_index] -= record_table[i_index];

    if (record_table[i_index] != log.vi || record_table[j_index] != log.vj || record_table[k_index] != log.vk)
      return false;
  }

  return true;
}
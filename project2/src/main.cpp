#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "../include/WaitfreeAtomicSnapshot.hpp"


std::atomic<int> total_count(0);


void CountUpdate(std::chrono::system_clock::time_point tp, WaitfreeAtomicSnapshot& waitfree_atomic_snapshot)
{
  // Random number

  std::random_device rd;

  std::mt19937 gen(rd());

  std::uniform_int_distribution<int> dis(-2147483648, 2147483647);


  // During 60 seconds, update snapshot.

  int index = waitfree_atomic_snapshot.RegisterTid(std::this_thread::get_id());

  int count = 0;

  while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - tp).count() <= 60)
  {
    waitfree_atomic_snapshot.update(dis(gen), index);
  
    count++;
  }

  total_count.fetch_add(count);
}

int main(int argc, char* argv[])
{
  // Recieve thread count by argument

  if (argc < 2)
  {
    std::cout << "Not enough argument" << std::endl;

    return 0;
  }

  int thread_count = std::stoi(argv[1]);

  std::cout << "Total thread count is " << thread_count << std::endl;


  // Start test

  WaitfreeAtomicSnapshot waitfree_atomic_snapshot(thread_count);

  std::vector<std::thread> thread_vector;

  std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();

  for (int i = 0; i < thread_count; i++)
  {
    thread_vector.emplace_back(std::thread(CountUpdate, tp, std::ref(waitfree_atomic_snapshot)));
  }

  for (int i = 0; i < thread_count; i++)
  {
    thread_vector[i].join();
  }

  std::cout << "Total update count is " << total_count << std::endl;

  return 0;
}
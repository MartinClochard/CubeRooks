
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include "grid_monothread.h"

class main_grid_monothread : public grid_monothread {
public:
  main_grid_monothread(int reminder_rate);
  virtual void monitor(const grid &);
  virtual void register_optimum(const grid &);
  void after_run();
private:
  std::unique_ptr<grid,grid_deleter> _best_grid;
  int _reminder_rate;
  int _reminder;
};

main_grid_monothread::main_grid_monothread(int reminder_rate) :
  grid_monothread(),_best_grid(),
  _reminder_rate(reminder_rate),_reminder(reminder_rate) {}

void main_grid_monothread::monitor(const grid & g) {
  std::cout << "Current state:" << std::endl;
  grid_job::print(g,std::cout);
  if(--_reminder == 0) {
    _reminder = _reminder_rate;
    if(_best_grid.get() != nullptr) {
      std::cout << "Reminder (best state):" << std::endl;
      grid_job::print(*_best_grid,std::cout);
    }
  }
}

void main_grid_monothread::register_optimum(const grid & g) {
  _best_grid = std::unique_ptr<grid,grid_deleter>(grid_job::make_copy(g));
  std::cout << "New optimum found!" << std::endl;
  grid_job::print(*_best_grid,std::cout);
}

void main_grid_monothread::after_run() {
  if(_best_grid == nullptr) {
    std::cout << "No optimum found. Initial guess was too high." << std::endl;
  } else {
    std::cout << "Best grid found:" << std::endl;
    grid_job::print(*_best_grid,std::cout);
  }
}

int main(int argc,const char * argv[]) {
  main_grid_monothread gm(10);
  dims len(8);
  int guess(0);
  if(sizeof(bitset) < static_cast<unsigned int>(len)) {
    std::cout << "Recompile with larger bitset" << std::endl;
    return(-1);
  }
  gm.run(len,guess,true,std::chrono::milliseconds(1000));
  gm.after_run();
  return(0);
}


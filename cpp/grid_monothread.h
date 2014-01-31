
#ifndef GRID_MONOTHREAD_H
#define GRID_MONOTHREAD_H

#include "grid.h"
#include <chrono>

class grid_monothread {
public:
  grid_monothread();
  grid_monothread(const grid_monothread &) = delete;
  grid_monothread(grid_monothread &&) = delete;
  grid_monothread & operator=(const grid_monothread &) = delete;
  grid_monothread & operator=(grid_monothread &&) = delete;
  virtual ~grid_monothread();
  //What to do with monitored grid. Nothing by default.
  virtual void monitor(const grid &);
  //What to do with a fresh optimum grid. Nothing by default.
  virtual void register_optimum(const grid &);
  //Run an instance of the grid problem.
  void run(dims len,int initial_guess,bool monitor,std::chrono::milliseconds monitor_frequency);
private:
  query_engine<grid_query> _gqe;
  query_engine<grid_signal> _gse;
};

#endif


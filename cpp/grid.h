
#ifndef GRID_H
#define GRID_H

#include <vector>
#include <memory>
#include <tuple>
#include <iostream>
#include "query.h"
#include "bitset.h"
#include "job.h"

class grid_job;
struct grid;
//Required since we do not provide grid implementation.
struct grid_deleter {
  void operator()(grid *) const;
};

/* codes/structures for in-computer communication. */

//Query code to communicate with a grid job.
enum grid_query_code {
  //Query: get current state.
  monitor_code,
  //Query: get current job queue
  get_jobs_code,
  //Query: kill backtracking thread.
  kill_code,
  //Query: register potentially "new" optimum
  register_code,
  //Send a new job to the working thread.
  go_to_work_code
};

enum grid_signal_code {
  //Signal: new optimum found, do corresponding updates.
  optimum_code,
  //Signal: normal (unasked for) termination, e.g job done and thread waiting
  //for more.
  job_done_code,
};

struct grid_query {
  //query code
  grid_query_code query_type;
  //return space for current/start state
  std::unique_ptr<grid,grid_deleter> monitor_grid;
  //return space for job queue.
  std::vector< std::unique_ptr < grid_job > > jobs;
  //optimum to register.
  int new_optimum;
  //Job on which to start work.
  std::unique_ptr< grid_job > start_job;
};

struct grid_signal {
  //signal code
  grid_signal_code signal_type;
  //Found optimum, transmission data.
  std::unique_ptr<grid,grid_deleter> best_grid;
  int found_optimum;
};

class grid_job : public job {
public:
  //Get the job identifiers for grid jobs.
  static std::vector< std::unique_ptr< job_id > > get_ids();
  //Create a (communication structures un-initialized)
  //grid job for fixed size.
  static grid_job * make(dims size,int initial_guess);
  //Grid inspection.
  static dims size(const grid &);
  static bool have_rook(const grid &,dims x,dims y,dims z);
  static int num_rooks(const grid &);
  static std::vector<std::tuple<dims,dims,dims> > list_rooks(const grid &);
  //Negative if no rook on the corresponding line.
  static int rook_z(const grid &,dims x,dims y);
  static int rook_y(const grid &,dims x,dims z);
  static int rook_x(const grid &,dims y,dims z);
  static void print(const grid &,std::ostream &);
  static grid * make_copy(const grid &);
  //Grid serialization by appending to the given string.
  static void serialize(const grid &,std::string &);
  //Grid deserialization.
  static grid * deserialize(std::string &,size_t,size_t);
  //Initialize communication structures. Should be done only once.
  inline void initialize_comm(answer_side<grid_query> a,
                              query_side<grid_signal> q) {
    _a = a;
    _q = q;
  }
  //Give an estimate of the optimum that may ameliorate the one known by
  //the job.
  virtual void minorate_optimum(int minopt) = 0;
  //This is abstract (v-methods not implemented).
protected:
  inline grid_job() : _a(),_q() {}
  answer_side<grid_query> _a;
  query_side<grid_signal> _q;
};

//Worker for grid jobs.
class grid_worker {
public:
  grid_worker() = delete;
  grid_worker(const grid_worker &) = delete;
  grid_worker(grid_worker &&) = delete;
  grid_worker operator=(const grid_worker &) = delete;
  grid_worker operator=(grid_worker &&) = delete;
  inline grid_worker(answer_side<grid_query> a,query_side<grid_signal> q) :
    _a(a),_q(q) {}
  void run();
protected:
  answer_side<grid_query> _a;
  query_side<grid_signal> _q;
};

#endif



#define FAST_FFS
//#define EQUILIBRIUM
//#define OTHER_CARDS
#include "grid.h"

//This is were all the magical stuff should happen.

struct grid {
  explicit grid(dims);
  //Dimension of the grid.
  dims size;
  //Number of rooks in the grid.
  int rooks;
  //Projections on the three axis.
  std::vector<bitset> gridxy;
  std::vector<bitset> gridyx;
  std::vector<bitset> gridxz;
  std::vector<bitset> gridzx;
  std::vector<bitset> gridyz;
  std::vector<bitset> gridzy;
  //Maximum allowed rook height (for sorting
  //of rook heights with respect to the traversal order)
  dims max_rook_height;
#ifdef EQUILIBRIUM
  //First encountered row cardinal.
  dims first_card;
#endif
  //Last encountered row cardinal (for sorting cardinals).
  dims last_card;
  //Cardinal for current row (for sorting row cardinals).
  dims current_card;
#ifdef OTHER_CARDS
  //Cardinals for every floors (may break a symmetry).
  std::vector<dims> floor_cards;
  //Cardinals for every columns (may break a symmetry).
  std::vector<dims> column_cards;
#endif
};

namespace {
  
  const std::string grid_job_next_pillar_name("grid_job.backtrack_next_pillar");
  const std::string grid_job_pillar_name("grid_job.backtrack_pillar");
  
  struct state {
    explicit inline state(grid && g,dims opt) : g0(g),optimum_so_far(opt) {}
    grid g0;
    //Part of the state that is not exactly part of the grid.
    //Optimum reached so far.
    int optimum_so_far;
  };
  
  class grid_job_inter : public grid_job {
  protected:
    inline grid_job_inter(grid && g,dims opt) : grid_job(),
      s(std::move(g),opt) {}
    state s;
    void backtrack_pillar(dims x,dims y,dims z);
    void backtrack_next_pillar(dims x,dims y);
    //It may happen that we want to call that directly from
    //backtrack_pillar.
    void backtrack_next_row(dims y);
    //Do communication stuff (including receiving GetCallStack msg & cie!)
    inline void communicate();
  };
  
  class grid_job_next_pillar : public grid_job_inter {
  public:
    grid_job_next_pillar(grid && g,dims x,dims y,dims optimum);
    virtual ~grid_job_next_pillar() = default;
    virtual void serialize(std::string &);
    virtual const std::string & get_job_id();
    virtual void run();
    virtual void minorate_optimum(int minopt);
  private:
    dims xstart;
    dims ystart;
  };
  
  class grid_job_next_pillar_id : public job_id {
  public:
    inline grid_job_next_pillar_id() : job_id(grid_job_next_pillar_name) {}
    virtual ~grid_job_next_pillar_id() = default;
    virtual grid_job_next_pillar *
      deserialize(const std::string & s,size_t l,size_t u);
  };
  
  class grid_job_pillar : public grid_job_inter {
  public:
    grid_job_pillar(grid && g,dims x,dims y,dims z,dims optimum);
    virtual ~grid_job_pillar() = default;
    virtual void serialize(std::string &);
    virtual const std::string & get_job_id();
    virtual void run();
    virtual void minorate_optimum(int minopt);
  private:
    dims xstart;
    dims ystart;
    dims zstart;
  };
  
  class grid_job_pillar_id : public job_id {
  public:
    grid_job_pillar_id() : job_id(grid_job_pillar_name) {}
    virtual ~grid_job_pillar_id() = default;
    virtual grid_job_pillar *
      deserialize(const std::string & s,size_t l,size_t u);
  };
  
  class GetCallStackException : public std::exception {
  public:
    inline GetCallStackException
      (std::vector< std::unique_ptr < grid_job > > & rt) : call_stack(rt) {}
    std::vector<std::unique_ptr< grid_job > > & call_stack;
  };
  class KillWorkerException : public std::exception {};
}

void grid_deleter::operator()(grid * g) const {
  delete g;
}


std::vector< std::unique_ptr< job_id > > grid_job::get_ids() {
  std::vector< std::unique_ptr< job_id > > ret;
  auto gjnpid(new grid_job_next_pillar_id);
  auto gjpi(new grid_job_pillar_id);
  ret.push_back(std::unique_ptr<job_id>(gjnpid));
  ret.push_back(std::unique_ptr<job_id>(gjpi));
  //Thanks c++11, copy is not allowed anymore
  return ret;
}

grid_job * grid_job::make(dims len,int initial_guess) {
  grid g(len);
  return new grid_job_pillar(std::move(g),len-1,len-1,0,initial_guess);
}

dims grid_job::size(const grid & g) {
  return(g.size);
}

bool grid_job::have_rook(const grid & g,dims x,dims y,dims z) {
  //not casting 1 to bitset might cause trouble...
  //if sizeof(int) < sizeof(bitset)
  return((g.gridxy[x] & (static_cast<bitset>(1) << y)) &&
   (g.gridyz[y] & (static_cast<bitset>(1) << z)) &&
   (g.gridxz[x] & (static_cast<bitset>(1) << z)));
}

int grid_job::num_rooks(const grid & g) {
  return(g.rooks);
}

std::vector<std::tuple<dims,dims,dims> > grid_job::list_rooks(const grid & g) {
  std::vector<std::tuple<dims,dims,dims> > ret;
  dims s(g.size);
  for(dims x(0);x != s;++x) {
    for(dims y(0);y != s;++y) {
      dims z(grid_job::rook_z(g,x,y));
      if(z >= 0) {
        ret.push_back(std::tuple<dims,dims,dims>(x,y,z));
      }
    }
  }
  return ret;
}

int grid_job::rook_z(const grid & g,dims x,dims y) {
  if(g.gridxy[x] & (static_cast<bitset>(1) << y)) {
    return (FFS_BITSET(0,g.gridyz[y] & g.gridxz[x]) - 1);
  } else {
    return (-1);
  }
}

int grid_job::rook_y(const grid & g,dims x,dims z) {
  if(g.gridxz[x] & (static_cast<bitset>(1) << z)) {
    return (FFS_BITSET(0,g.gridxy[x] & g.gridzy[z]) - 1);
  } else {
    return (-1);
  }
}

int grid_job::rook_x(const grid & g,dims y,dims z) {
  if(g.gridyz[y] & (static_cast<bitset>(1) << z)) {
    return (FFS_BITSET(0,g.gridyx[y] & g.gridzx[z]) - 1);
  } else {
    return (-1);
  }
}

void grid_job::print(const grid & g,std::ostream & os) {
  dims s(grid_job::size(g));
  os << "size : " << grid_job::num_rooks(g) << std::endl;
  int l10 = 1;
  {
    dims s2 = s/10;
    while(s2 != 0) {
      ++l10;
      s2 /= 10;
    }
  }
  auto vline([&]() {
    for(dims x(0);x != 2 + s * (l10+1);++x) {
      os << '-';
    }
    os << std::endl;
  });
  vline();
  for(dims y(0);y != s;++y) {
    auto hline([&]() {
      os << '|';
    });
    hline();
    for(dims x(0);x != s;++x) {
      dims z(grid_job::rook_z(g,x,y));
      if(z >= 0) {
        os << z+1;
      } else {
        os << '*';
      }
      for(dims i = 0;i != l10;++i) {
        os << ' ';
      }
    }
    hline();
    os << std::endl;
  }
  vline();
  os << std::endl;
}

grid * grid_job::make_copy(const grid & g) {
  return(new grid(g));
}

void grid_job::serialize(const grid & g,std::string & buf) {
  //TODO.
}

grid * grid_job::deserialize(std::string & buf,size_t l,size_t u) {
  //TODO.
  return(NULL);
}

grid::grid(dims len) : size(len),
  rooks(0),
  //One more (always set to 0) allows
  //to remove a test for column binary decreasing test by defaulting
  //the previous column (filled in decreasing order) to 0
  //if it do not exists.
  gridxy(len+1,0),
  //Same thing for row cardinality tie-breaking.
  gridyx(len+1,0),
  gridxz(len,0),
  gridzx(len,0),
  gridyz(len,0),
  gridzy(len,0),
  max_rook_height(0),
#ifdef EQUILIBRIUM
  first_card(len+1),
#endif
  last_card(len+1),
  current_card(0)
#ifdef OTHER_CARDS
  ,floor_cards(len,0),
  column_cards(len,0)
#endif
  {}

void grid_worker::run() {
  int min_opt = 0;
  grid_signal gs;
  while(true) {
    _a.wait_query();
    auto qr(_a.get_query());
    switch(qr->query_type) {
    case monitor_code: {
      //Waiting for you to send work.
      //However, this may have be sent before the
      //master knew work were done.
      qr->monitor_grid.reset();
      _a.answer();
      break; }
    case get_jobs_code: {
      //...
      qr->jobs.clear();
      _a.answer();
      break; }
    case kill_code: {
      //Fine !
      _a.answer();
      return; }
    case register_code: {
      //I am not a job you know ?
      int proposal = qr->new_optimum;
      if(proposal > min_opt) { min_opt = proposal; }
      _a.answer();
      return; }
    case go_to_work_code: {
      //Finally!
      std::unique_ptr<grid_job> ptr(std::move(qr->start_job));
      ptr->initialize_comm(_a,_q);
      ptr->minorate_optimum(min_opt);
      _a.answer();
      bool normal_termination = true;
      try {
        ptr->run();
      } catch(GetCallStackException &) {
        _a.answer();
        normal_termination = false;
      } catch(KillWorkerException &) {
        return;
      }
      if(normal_termination) {
        gs.signal_type = job_done_code;
        _q.query(&gs);
        //Cannot continue otherwise.
        _q.wait_answer();
      }
      break; }
    }
  }
}

namespace {
  
  grid_job_next_pillar::grid_job_next_pillar(grid && g,dims x,dims y,dims opt) :
    grid_job_inter(std::move(g),opt),xstart(x),ystart(y) {}
  
  void grid_job_next_pillar::serialize(std::string & s) {
    //TODO.
  }
  const std::string & grid_job_next_pillar::get_job_id() {
    return(grid_job_next_pillar_name);
  }
  
  void grid_job_next_pillar::run() {
    backtrack_next_pillar(xstart,ystart);
  }
  
  void grid_job_next_pillar::minorate_optimum(int minopt) {
    if(minopt > s.optimum_so_far) { s.optimum_so_far = minopt; }
  }
  
  grid_job_pillar::grid_job_pillar(grid && g,
                                   dims x,
                                   dims y,
                                   dims z,
                                   dims opt) :
    grid_job_inter(std::move(g),opt),xstart(x),ystart(y),zstart(z) {}
  
  void grid_job_pillar::serialize(std::string &) {
    //TODO.
  }
  
  const std::string & grid_job_pillar::get_job_id() {
    return(grid_job_pillar_name);
  }
  
  void grid_job_pillar::run() {
    backtrack_pillar(xstart,ystart,zstart);
  }
  
  void grid_job_pillar::minorate_optimum(int minopt) {
    if(minopt > s.optimum_so_far) { s.optimum_so_far = minopt; }
  }
  
  grid_job_next_pillar *
    grid_job_next_pillar_id::deserialize(const std::string & s,
                                         size_t l,
                                         size_t u) {
    //TODO.
    return(NULL);
  }
  
  grid_job_pillar *
    grid_job_pillar_id::deserialize(const std::string & s,size_t l,size_t u) {
    //TODO
    return(NULL);
  }
  
  inline void grid_job_inter::communicate() {
    if(_a.have_query()) {
      auto qr(_a.get_query());
      switch(qr->query_type) {
      case monitor_code: {
        qr->monitor_grid =
          std::unique_ptr<grid,grid_deleter>(grid_job::make_copy(s.g0));
        _a.answer();
        return; }
      case get_jobs_code: {
        //The answer is the worker responsibility here.
        throw(GetCallStackException(qr->jobs)); }
      case kill_code: {
        _a.answer();
        throw(KillWorkerException()); }
      case register_code: {
        int proposal = qr->new_optimum;
        if(proposal > s.optimum_so_far) { s.optimum_so_far = proposal; }
        _a.answer();
        return; }
      case go_to_work_code: {
        //Erroneous situation. Blindly terminates the process.
        std::terminate();
        break; }
      }
    }
  }
  
  //TODO: should insert communication reading somewhere in those three
  //procedures.
  
  void grid_job_inter::backtrack_pillar(dims x,dims y,dims z0) {
    bitset & rgxz(s.g0.gridxz[x]);
    bitset & rgyz(s.g0.gridyz[y]);
    bitset gxz(rgxz);
    bitset gyz(rgyz);
    dims sz(s.g0.size);
    /* Check consistency of NO update relatively to binary ordering on columns.
       Columns must be in decreasing order for binary ordering. In order
       for this test to be consistent,
       high bits (high rows) must be filled first. Also,
       there is no need for this test to be done in case we add a rook.
       Why: 1) either the two columns were already ill-ordered before,
               which should have been detected.
            2) either they were already correctly ordered
               (current(x) > previous(x+1)),
               in which case it changes nothing.
            3) either they were equal, so in case of update the current column
               is at least geq the previous one (remember:
               going in reverse order) */
    bitset & rgxy(s.g0.gridxy[x]);
    bitset gxy(rgxy);
    auto consistency_check([&]() {
      //This is THE place to check for valid remaining_count
      //(adding a rook would not have helped to decrease it)
      dims cc(s.g0.current_card);
      //Might add up to x rooks in the full row.
      dims cce(cc+x);
      //And it is bounded by last cardinal.
      dims lc(s.g0.last_card);
      int max_possible_card(cce > lc ? lc : cce);
      //This is what we are really allowed to place in the remaining space.
      int allowed_rooks(max_possible_card - cc);
      if(max_possible_card * y + allowed_rooks + s.g0.rooks <= s.optimum_so_far) {
        //Well, we obviously will not find anything better here.
        return false;
      }
      return(gxy >= s.g0.gridxy[x+1]);
    });
    //Test for double attack on the pillar.
    //If yes, go directly to next pillar.
    if(!(gxz & gyz)) {
      dims cc = s.g0.current_card;
      dims cc1 = cc+1;
      bitset & rgyx(s.g0.gridyx[y]);
      bitset gyx(rgyx);
      bitset _1(1);
      bitset mask_x(_1 << x);
      bitset ugyx(gyx ^ mask_x);
      //Check whether we reached max allowed card.
      bool max_allowed_card_reached = (cc1 == s.g0.last_card);
      //Because if we did it is time to check for row ordering.
      if(max_allowed_card_reached) {
        if(ugyx < s.g0.gridyx[y+1]) {
          //If the test fail, this is not even worth checking
          //for other row fillings. Indeed, we can only generates smaller
          //values for the bitset...while wanting bigger ones (remember:
          //iteration in reverse order!)
          s.g0.last_card = cc;
          s.g0.current_card = 0;
          auto undo([&]() {
            s.g0.current_card = cc;
            s.g0.last_card = cc1;
          });
          try {
            backtrack_next_row(y);
            undo();
          } catch(GetCallStackException &) {
            undo();
            throw;
          }
          return;
        } else {
          s.g0.current_card = 0;
        }
      } else {
        s.g0.current_card = cc1;
      }
      bitset mask_y(_1 << y);
      /* Speculative updates. */
      rgxy = gxy ^ mask_y;
      rgyx = ugyx;
      int rk = s.g0.rooks;
      s.g0.rooks = rk+1;
      /* The compiler have better inline this... */
      auto speculative_undo([&]() {
        s.g0.rooks = rk;
        s.g0.current_card = cc;
        rgyx = gyx;
        rgxy = gxy;
      });
      int max_z = s.g0.max_rook_height;
      int maj_z = max_z + 1;
      //set of allowed z with respect to direct attacks.
      bitset guz = ((~(gxz | gyz)) & ((_1 << maj_z) - 1)) >> z0;
      int z = (z0-1);
      while(true) {
        int offset = FFS_BITSET(0,guz);
        if(offset == 0) { break; }
        guz >>= offset;
        z += offset;
        bitset & rgzx(s.g0.gridzx[z]);
        bitset & rgzy(s.g0.gridzy[z]);
        bitset gzx(rgzx);
        bitset gzy(rgzy);
        /* Test for potential double attacks on row/columns. */
        if((gzx & gyx) || (gzy & gxy)) {
          continue;
        }
        bitset mask_z(_1 << z);
        rgxz = gxz ^ mask_z;
        rgyz = gyz ^ mask_z;
        rgzx = gzx ^ mask_x;
        rgzy = gzy ^ mask_y;
        /* Just a magical way to factor code. */
        auto loop_undo([&]() {
          rgzy = gzy;
          rgzx = gzx;
          rgyz = gyz;
          rgxz = gxz;
        });
        if(maj_z < sz && z == max_z) {
          s.g0.max_rook_height = max_z + 1;
          try {
            if(max_allowed_card_reached) {
              backtrack_next_row(y);
            } else {
              backtrack_next_pillar(x,y);
            }
          } catch(GetCallStackException & e) {
            s.g0.max_rook_height = max_z;
            loop_undo();
            speculative_undo();
            if(consistency_check()) {
              grid g2(s.g0);
              auto job(new
                grid_job_next_pillar(std::move(g2),x,y,s.optimum_so_far));
              e.call_stack.emplace_back(job);
            }
            throw;
          }
          s.g0.max_rook_height = max_z;
          //we know we where on the last possible z.
          loop_undo();
          break;
        } else {
          try {
            if(max_allowed_card_reached) {
              backtrack_next_row(y);
            } else {
              backtrack_next_pillar(x,y);
            }
          } catch(GetCallStackException & e) {
            loop_undo();
            speculative_undo();
            if(guz == 0) {
              if(consistency_check()) {
                grid g2(s.g0);
                auto job(new
                  grid_job_next_pillar(std::move(g2),x,y,s.optimum_so_far));
                e.call_stack.emplace_back(job);
              }
            } else {
              grid g2(s.g0);
              auto job(new
                grid_job_pillar(std::move(g2),x,y,z+1,s.optimum_so_far));
              e.call_stack.emplace_back(job);
            }
            throw;
          }
        }
        loop_undo();
      }
      speculative_undo();
    }
    if(consistency_check()) {
      backtrack_next_pillar(x,y);
    } else {
      communicate();
    }
  }
  
  /* We now that when we enter this function, the previous row
     is not filled up to full allowed cardinality. */
  void grid_job_inter::backtrack_next_pillar(dims x,dims y) {
    if(x == 0) {
      //TODO:insert equilibrium code here.
      //Reached the end of the row with lower card,
      //so do some maintenance stuff
      //This stuff is only needed if the end of the row
      //is reached without max allowed cardinality.
      dims lc(s.g0.last_card);
      dims cc(s.g0.current_card);
      s.g0.last_card = cc;
      s.g0.current_card = 0;
      auto undo([&]() {
        s.g0.current_card = cc;
        s.g0.last_card = lc;
      });
      try {
        backtrack_next_row(y);
      } catch(GetCallStackException &) {
        undo();
        throw;
      }
      undo();
    } else {
      //direct recursion.
      backtrack_pillar(x-1,y,0);
    }
  }
  
  void grid_job_inter::backtrack_next_row(dims y) {
    if(y == 0) {
      communicate();
      int candidate = s.g0.rooks;
      if(candidate > s.optimum_so_far) {
        s.optimum_so_far = candidate;
        grid_signal gs;
        gs.signal_type = optimum_code;
        gs.found_optimum = s.optimum_so_far;
        auto gr = new grid(s.g0);
        gs.best_grid = std::unique_ptr<grid,grid_deleter>(gr);
        _q.query(&gs);
        _q.wait_answer();
      }
    } else {
      backtrack_pillar(s.g0.size-1,y-1,0);
    }
  }
  
}




#include "grid_monothread.h"
#include <thread>
#include <chrono>

grid_monothread::grid_monothread() : _gqe(),_gse() {}

grid_monothread::~grid_monothread() {}

void grid_monothread::monitor(const grid &) {}

void grid_monothread::register_optimum(const grid &) {}

void grid_monothread::run(dims len,
                          int initial_guess,
                          bool do_monitor,
                          std::chrono::milliseconds monitor_frequency) {
  query_engine<grid_query> gq;
  query_engine<grid_signal> gs;
  auto ps(gs.get_answer_side());
  auto pq(gq.get_query_side());
  grid_worker wk(gq.get_answer_side(),gs.get_query_side());
  std::thread t([&]() { wk.run(); });
  grid_query gqs;
  gqs.query_type = go_to_work_code;
  grid_job * gj = grid_job::make(len,initial_guess);
  gqs.start_job = std::unique_ptr<grid_job>(gj);
  pq.query(&gqs);
  pq.wait_answer();
  auto time([]() { return std::chrono::high_resolution_clock::now(); });
  std::chrono::high_resolution_clock::time_point last_monitor(time());
  bool monitor_sent(false);
  while(true) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if(monitor_sent) {
      if(pq.have_answer()) {
        monitor_sent = false;
        last_monitor = time();
        monitor(*(gqs.monitor_grid));
        gqs.monitor_grid.reset();
      }
    } else if(do_monitor && time() - last_monitor > monitor_frequency) {
      monitor_sent = true;
      gqs.query_type = monitor_code;
      pq.query(&gqs);
    }
    if(ps.have_query()) {
      auto qr(ps.get_query());
      switch(qr->signal_type) {
      case optimum_code: {
          register_optimum(*(qr->best_grid));
          qr->best_grid.reset();
          ps.answer();
          break;
        }
      case job_done_code: {
          ps.answer();
          gqs.query_type = kill_code;
          pq.query(&gqs);
          pq.wait_answer();
          t.join();
          return;
        }
      }
    }
  }
}



#ifndef QUERY_H
#define QUERY_H
#include <atomic>
#include <thread>
#include <chrono>

/* template for inter-thread communication.
 * It is asymetric: the master thread query and the slave thread answer.
 */

template < typename Q > class query_side;
template < typename Q > class answer_side;
template < typename Q > class query_engine;

/* note: query and answer sides are "raw" references
   on the engine, and as such should be used only
   during its lifetime. Their purpose is to split
   the logic. */

/* Query side, for master thread. */
template < typename Q > class query_side {
public:
  inline query_side() = default;
  //Pre: at least a query was sent.
  //Check whether we have an answer to the last query.
  inline bool have_answer();
  //Pre: at least a query was sent.
  //Wait for an answer to the last query.
  inline void wait_answer();
  //Pre: the last query was answered/no query were sent.
  //Post: send the corresponding query.
  inline void query(Q *);
private:
  friend class query_engine<Q>;
  inline query_side(query_engine<Q> *);
  query_engine<Q> * _qe;
};

/* Answer side, for slave thread. */
template < typename Q > class answer_side {
public:
  inline answer_side() = default;
  //Check whether we have a query.
  inline bool have_query();
  //Wait until we have a query.
  inline void wait_query();
  //Pre: already have a query.
  //Post: return the corresponding query object.
  inline Q * get_query();
  //Pre: every modification answering the query were done.
  //(typically by modiying the query object)
  //Post: "send" the answer.
  inline void answer();
private:
  friend class query_engine<Q>;
  inline answer_side(query_engine<Q> *);
  query_engine<Q> * _qe;
};

/* Engine from which query/answer sides are derived. */
template < typename Q > class query_engine {
public:
  inline query_engine();
  inline ~query_engine() = default;
  query_engine(const query_engine<Q> &) = delete;
  query_engine<Q> & operator=(const query_engine<Q> &) = delete;
  inline query_side<Q> get_query_side();
  inline answer_side<Q> get_answer_side();
private:
  friend class query_side<Q>;
  friend class answer_side<Q>;
  Q * _query;
  std::atomic<bool> _ms;
};

template < typename Q >
inline query_side<Q>::query_side(query_engine<Q> * q) : _qe(q) {}

template < typename Q >
inline bool query_side<Q>::have_answer() {
  return(!((_qe->_ms).load(std::memory_order_acquire)));
}

template < typename Q >
inline void query_side<Q>::wait_answer() {
  while(!have_answer()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

template < typename Q >
inline void query_side<Q>::query(Q * q) {
  _qe->_query = q;
  (_qe->_ms).store(true,std::memory_order_release);
}

template < typename Q >
inline answer_side<Q>::answer_side(query_engine<Q> * q) : _qe(q) {}

template < typename Q >
inline bool answer_side<Q>::have_query() {
  return((_qe->_ms).load(std::memory_order_acquire));
}

template < typename Q >
inline void answer_side<Q>::wait_query() {
  while(!have_query()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

template < typename Q >
inline Q * answer_side<Q>::get_query() {
  return(_qe->_query);
}

template < typename Q >
inline void answer_side<Q>::answer() {
  (_qe->_ms).store(false,std::memory_order_release);
}

template < typename Q >
inline query_engine<Q>::query_engine() : _query(nullptr),_ms(false) {}

template < typename Q >
inline query_side<Q> query_engine<Q>::get_query_side() {
  return(query_side<Q>(this));
}

template < typename Q >
inline answer_side<Q> query_engine<Q>::get_answer_side() {
  return(answer_side<Q>(this));
}

#endif


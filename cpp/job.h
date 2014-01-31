
#ifndef JOB_H
#define JOB_H

#include <cstddef>
#include <cinttypes>
#include <string>
#include <unordered_map>
#include <memory>

class job;
class job_id;
class job_id_manager;

/* Represent a job: can be serialized (to be moved in
   a network) or run. */
class job {
public:
  inline job() {}
  job(const job &) = delete;
  job & operator=(const job &) = delete;
  job(job &&) = delete;
  job & operator=(job &&) = delete;
  virtual ~job();
  /* Serialize the job (appended to given string buffer).
     Do not contain the job id, although
     the id should be part of the full serialization. */
  virtual void serialize(std::string &) = 0;
  /* Get the job identifier (for class reconstruction). */
  virtual const std::string & get_job_id() = 0;
  /* run the job. Note: serialization become invalid once started.
     Also, sub-classes are perfectly right to ask for
     some initialization procedures (e.g for communication structures,
     which have no impact on serialization) to be run before run() itself. */
  virtual void run() = 0;
};

/* Represent a group of similar jobs (e.g similar code). */
class job_id {
public:
  job_id() = delete;
  job_id & operator=(const job_id &) = delete;
  job_id(const job_id &) = delete;
  job_id & operator=(job_id &&) = delete;
  job_id(job_id &&) = delete;
  virtual ~job_id();
  /* return the id of the kind of job it manages. */
  inline const std::string & id() const { return _id; }
  /* re-construct a fresh job of the given kind from its serialization
     (between bounds in the string) */
  virtual job * deserialize(const std::string &,size_t l,size_t u) = 0;
protected:
  inline job_id(const std::string & s) : _id(s) {}
private:
  const std::string & _id;
};

/* To manage a full group of job id. */
class job_id_manager {
public:
  inline job_id_manager() : _table() {}
  job_id_manager(const job_id_manager &) = delete;
  job_id_manager & operator=(const job_id_manager &) = delete;
  inline job_id & from_id(std::string & s) { return *(_table[s]); }
  inline void register_id(std::string && s,std::unique_ptr<job_id> && j) {
    std::pair<std::string,std::unique_ptr<job_id> >
      pr(std::move(s),std::move(j));
    _table.insert(std::move(pr));
  }
private:
  std::unordered_map<std::string,std::unique_ptr<job_id> > _table;
};

#endif


#pragma once

#include <duckdb.h>
#include <cassert>
#include <memory>
#include <cstdio>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <sstream>
#include <string>
#include <chrono>
class Timer{
  typedef std::chrono::high_resolution_clock Clock;
  typedef std::chrono::milliseconds MilliSec;
  typedef std::chrono::microseconds MicroSec;
  Clock::time_point m_start; /**< The start timepoint */
  Clock::duration m_add; /**< Durations between previous start/unpause and pause */
  bool m_running; /**< Is the timer running? */
public:
  Timer(bool start_now = true);

  void start();
  void pause();
  void unpause();
  double elapsed_us() const;
  double elapsed_ms() const;
};
Timer::Timer(bool start_now) : m_running(false), m_add(Clock::duration::zero()){
  if(start_now) start();
}

void Timer::start(){
  m_start = Clock::now();
  m_add = Clock::duration::zero();
  m_running = true;
}

void Timer::pause(){
  if(m_running){
    Clock::time_point now = Clock::now();    
    m_add += now - m_start;
    m_running = false;
  }
}

void Timer::unpause(){
  assert(!m_running);
  m_start = Clock::now();
  m_running = true;
}

double Timer::elapsed_us() const{
  if(m_running){
    Clock::time_point now = Clock::now();
    return std::chrono::duration_cast<MicroSec>(now - m_start + m_add).count();
  }else return std::chrono::duration_cast<MicroSec>(m_add).count();
}

double Timer::elapsed_ms() const{
  return elapsed_us()/1000.;
}



template<typename T>
struct _valueWriter{};

template<>
struct _valueWriter<int>{
  static inline void write(duckdb_appender appender, int v){ duckdb_append_int32(appender, v); }
  static inline std::string typeString(){ return "INTEGER"; }
};

template<>
struct _valueWriter<uint64_t>{
  static inline void write(duckdb_appender appender, uint64_t v){ duckdb_append_uint64(appender, v); }
  static inline std::string typeString(){ return "UBIGINT"; }
};

template<>
struct _valueWriter<int64_t>{
  static inline void write(duckdb_appender appender, int64_t v){ duckdb_append_int64(appender, v); }
  static inline std::string typeString(){ return "BIGINT"; }
};


template<>
struct _valueWriter<double>{
  static inline void write(duckdb_appender appender, double v){ duckdb_append_double(appender, v); }
  static inline std::string typeString(){ return "DOUBLE"; }
};

template<>
struct _valueWriter<std::string>{
  static inline void write(duckdb_appender appender, const std::string &v){ duckdb_append_varchar(appender, v.c_str()); }
  static inline std::string typeString(){ return "VARCHAR"; }
};


template<typename T>
struct _valueReader{
  static inline char* read(T &into, char* vdata){
    T* vdata_t = (T*)vdata;
    into = *(vdata_t++);
    return (char*)vdata_t;
  }
};
template<>
struct _valueReader<std::string>{
  static inline char* read(std::string &into, char* vdata){
    //cf https://duckdb.org/docs/api/c/vector
    duckdb_string_t* vdata_t = (duckdb_string_t*)vdata;
    if(vdata_t->value.inlined.length <= 12){
      into = std::string(vdata_t->value.inlined.inlined);
    }else{
      into = std::string(vdata_t->value.pointer.ptr);
    }
    return (char*)(++vdata_t);
  }
};



template<typename T>
struct valueContainer;

struct valueContainerBase{
  template<typename T>
  valueContainerBase& operator=(const T&v){
    try{
      dynamic_cast<valueContainer<T> &>(*this).v = v;
    }catch(const std::exception &e){
      std::cerr << "Failed to assign value of type " << _valueWriter<T>::typeString() << " to container of type " << this->typeString() << " because: " << e.what() << std::endl;
      throw std::runtime_error("Fail");																				       
    }     
    return *this;
  }
  virtual void write(duckdb_appender appender) const = 0;
  virtual void write(std::ostream &os) const = 0;

  //read from the result of calling duckdb_vector_get_data
  virtual char* read(char *vdata) = 0;
  virtual valueContainerBase* deepCopy() const = 0;
  virtual std::string typeString() const = 0;
  virtual ~valueContainerBase(){}
};

//suitable for POD types
template<typename T>
struct valueContainer: public valueContainerBase{
  T v;
  valueContainer(const T &v): v(v){}
  valueContainer(){}
  
  void write(duckdb_appender appender) const override{
    _valueWriter<T>::write(appender, v);
  }
  void write(std::ostream &os) const{
    os << v;
  }
  char* read(char *vdata) override{
    return _valueReader<T>::read(v, vdata);
  }    
  
  valueContainerBase* deepCopy() const override{ return new valueContainer(v); }
  std::string typeString() const override{
    return _valueWriter<T>::typeString();
  }  
  
};

struct value{
  std::unique_ptr<valueContainerBase> _p;

  value(){} //_p is null!
  value(valueContainerBase* v): _p(v){}
  value(const value &r): _p(r._p->deepCopy()){}
  value(value &&v): _p(std::move(v._p)){}
  
  template<typename T>
  value & operator=(const T&v){ (*_p) = v; return *this; }

  value & operator=(const value &r){ _p.reset(r._p->deepCopy()); return *this; }
  value & operator=(value &&r){ _p = std::move(r._p); return *this; }

  std::string typeString() const{ return _p->typeString(); }
  void write(duckdb_appender appender) const{ _p->write(appender); }
  char* read(char *vdata){ return _p->read(vdata); }
};
std::ostream & operator<<(std::ostream &os, const value &r){ r._p->write(os); return os; }  

template<typename T>
value createValue(){ return value(new valueContainer<T>()); }

class Table{
  std::vector<std::string> headers;
  std::vector<std::vector<value> > values;
  std::vector<value> ref_row; //a reference row used as a basis for duplication when adding rows; its actual values should never be used other than when forming a new row and remain uninitialized
  std::unordered_map<std::string, int> colmap;
  std::string name;
  friend std::ostream & operator<<(std::ostream &os, const Table &r);

  void reset(){
    headers.clear();
    colmap.clear();
    values.clear();
    ref_row.clear();
  }
    
public:
  Table(const std::string &name): name(name){}
  
  template<typename T>
  void addColumn(const std::string &name){
    assert(colmap.find(name) == colmap.end());
    ref_row.push_back(createValue<T>());
    for(int r=0;r<values.size();r++){
      values[r].push_back(createValue<T>());
    }
    headers.push_back(name);
    colmap[name] = ref_row.size()-1;
  }
  
  void addColumn(const std::string &name, duckdb_type type){
    switch(type){
    case DUCKDB_TYPE_DOUBLE:
      this->addColumn<double>(name); break;
    case DUCKDB_TYPE_INTEGER:
      this->addColumn<int>(name); break;
    case DUCKDB_TYPE_VARCHAR:
      this->addColumn<std::string>(name); break;
    case DUCKDB_TYPE_UBIGINT:
      this->addColumn<uint64_t>(name); break;
    case DUCKDB_TYPE_BIGINT:
      this->addColumn<int64_t>(name); break;
    default:
      throw std::runtime_error("Unknown duckdb_type of index "+std::to_string(type));
    }
  }

  //Add a new unpopulated row and return its index
  int addRow(){
    values.push_back(ref_row);
    return values.size()-1;
  }

  int rows() const{ return values.size(); }

  //Resize the number of rows. Added rows will be initialized according to the unpopulated ref_row
  void resizeRows(size_t to){
    values.resize(to, ref_row);
  }
  
  value & operator()(const int row, const std::string &name){
    auto it = colmap.find(name); assert(it != colmap.end());
    return values[row][it->second];
  }
  const value & operator()(const int row, const std::string &name) const{
    auto it = colmap.find(name); assert(it != colmap.end());
    return values[row][it->second];
  }
  value & operator()(const int i, const int j){ return values[i][j]; }
  const value & operator()(const int i, const int j) const{ return values[i][j]; }

  void define(duckdb_connection con){
    std::ostringstream ss;
    ss << "CREATE TABLE " << name << " (";
    for(int c=0;c<headers.size();c++)
      ss << headers[c] << " " << ref_row[c].typeString() << (c!=headers.size()-1 ? ", " : "");
    ss << ");";
    std::cout << "Creating table with: " << ss.str() << std::endl;
    assert( duckdb_query(con, ss.str().c_str(), NULL) != DuckDBError );
  }
  void write(duckdb_connection con){
    Timer t;
    t.start();
    duckdb_appender appender;
    assert(duckdb_appender_create(con, NULL, name.c_str(), &appender) != DuckDBError);
    double tcreate = t.elapsed_us();
    t.start();
    for(int r=0;r<values.size();r++){
      for(int c=0;c<values[r].size();c++)
	values[r][c].write(appender);
      duckdb_appender_end_row(appender);
    }
    double tappend = t.elapsed_us();
    t.start();
    duckdb_appender_destroy(&appender);
    double tdestroy = t.elapsed_us();
    std::cout << "write timings - appender_create:" << tcreate << "us  append:" << tappend << "us  appender_destroy:" << tdestroy << "us" << std::endl;
  }
  void read(duckdb_result &res){
    reset();
    int ncol = duckdb_column_count(&res);
    for(int c=0;c<ncol;c++)
      this->addColumn(duckdb_column_name(&res,c), duckdb_column_type(&res,c) );

    int row_off = 0;
    while(true){
      duckdb_data_chunk result = duckdb_fetch_chunk(res);
      if (!result) break;

      int chunk_rows = duckdb_data_chunk_get_size(result);
      for(int r=0;r<chunk_rows;r++)
	addRow();

      //std::cout << "ROWS " << values.size() << " EXPECT " << row_off+chunk_rows << std::endl;
      assert(values.size() == row_off + chunk_rows);
      
      for(int c=0;c<ncol;c++){
	duckdb_vector col = duckdb_data_chunk_get_vector(result, c);
	char* vdata = (char*)duckdb_vector_get_data(col);
	for(int r=0;r<chunk_rows;r++)
	  vdata = values[row_off + r][c].read(vdata);	
      }
      duckdb_destroy_data_chunk(&result);
      row_off += chunk_rows;
    }   
  }
  void read(duckdb_connection con){
    std::ostringstream q;
    q << "SELECT * FROM " + name + " ;";
    
    duckdb_result res;
    duckdb_query(con, q.str().c_str(), &res);
    read(res);
    duckdb_destroy_result(&res);
  }

  //Populate the table with the results of a query
  void fromQuery(const std::string &query, duckdb_connection con){
    duckdb_result result;
    assert(duckdb_query(con, query.c_str(), &result) != DuckDBError);
    read(result);
    duckdb_destroy_result(&result);
  }

  //Remove data from table but keep structure
  void clear(){
    values.clear();
  }

  //commit aka checkpoint DDBs in-memory buffer to disk
  void commit(duckdb_connection con){
    assert(duckdb_query(con, "CHECKPOINT;", NULL) != DuckDBError);
  }

  Table databaseSize(duckdb_connection con) const{
    Table t("database_size");
    t.fromQuery("CALL pragma_database_size();",con);
    return t;
  }
  
};
std::ostream & operator<<(std::ostream &os, const Table &t){
  for(int i=0;i<t.headers.size();i++){ os << t.headers[i] << (i!=t.headers.size()-1 ? " " : "\n"); }
  for(int r=0;r<t.values.size();r++)
    for(int c=0;c<t.values[r].size();c++)
      os << t.values[r][c] << (c!=t.values[r].size()-1 ? " " : "\n");
  return os;
}



void test_table(duckdb_connection con){
  Table t("my_table");
  t.addRow();
  t.addColumn<int>("A");
  t(0,"A") = 3;
  t.addColumn<double>("B");
  t(0,"B") = 3.141;
  t.addColumn<std::string>("C");
  t(0,"C") = std::string("hello");
  
  t.addRow();

  t(1,"A") = 5;
  t(1,"B") = -1.314;
  t(1,"C") = std::string("abracadabra_magic_man"); //deliberately > 12 chars
  
  std::cout << t << std::endl;

  t.define(con);
  t.write(con);



  Table u("my_table");
  u.read(con);
  std::cout << "REPRO:\n" << u << std::endl;
}
  

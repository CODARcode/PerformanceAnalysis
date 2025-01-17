#include "ddb_wrapper.h"
#include "pdb_schema.h"

void benchmark1(){
  //Test a simple table with float columns as we scale #rows and #cols
  Timer timer;

  //Hot loop; row batches are all added to the same table. Table is *not* flushed to disk
  for(int c=1; c<101; c+=10){
    remove("database.ddb");

    timer.start();
    duckdb_database db;
    duckdb_connection con;
    if (duckdb_open("database.ddb", &db) == DuckDBError) assert(0);
    if (duckdb_connect(db, &con) == DuckDBError) assert(0);

    { //disable checkpointing to disk
      assert(duckdb_query(con, "SET checkpoint_threshold = '100 GB';", NULL) != DuckDBError);
      //assert(duckdb_query(con, "SET checkpoint_threshold = '8 B';", NULL) != DuckDBError);
    }    
    
    Table table("mytable");
    //setup the table
    for(int cc=0;cc<c;cc++) table.addColumn<double>("col"+std::to_string(cc));
    //tell duckdb about our table
    table.define(con); 
    double topen = timer.elapsed_us();

    for(int rbatch_sz=10; rbatch_sz<100000; rbatch_sz*=2){
      table.resizeRows(rbatch_sz); //note that the table represents the unwritten portion here
      for(int r=0;r<rbatch_sz;r++)
	for(int cc=0;cc<c;cc++)
	  table(r,cc) = cc+c*r+3.14;

      for(int rpt=0;rpt<4;rpt++){     
	timer.start();
	table.write(con);
	double twrite = timer.elapsed_us();

	std::cout << "PRE COMMIT: " << table.databaseSize(con) << std::endl;

	//assert(duckdb_query(con, "PRAGMA enable_profiling;", NULL) != DuckDBError);
	//assert(duckdb_query(con, "SET enable_profiling = 'query_tree';", NULL) != DuckDBError);
	//assert(duckdb_query(con, "SET profiling_mode = 'detailed';", NULL) != DuckDBError);
	timer.start();
	table.commit(con);
	double tcommit = timer.elapsed_us();
	//assert(duckdb_query(con, "PRAGMA disable_profiling;", NULL) != DuckDBError);
	
	std::cout << "POST COMMIT: " << table.databaseSize(con) << std::endl;
	
	std::cout << rbatch_sz << "," << c << " " << rbatch_sz * c << " : "
		  << "total append "<< twrite << "us  commit " << tcommit << "us"
		  << ", per row append " << twrite/rbatch_sz << "us  commit " << tcommit/rbatch_sz << "us"
		  << ", per elem append " << twrite/rbatch_sz/c << "us  commit " << tcommit/rbatch_sz/c << "us"
		  << std::endl;
      }
    }
      
    timer.start();
    duckdb_disconnect(&con);
    duckdb_close(&db);
    double tclose = timer.elapsed_us();
  }


  
}
int main(void){
  benchmark1();
  return 0;
}

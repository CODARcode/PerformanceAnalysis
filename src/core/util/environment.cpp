#include<chimbuko/core/util/environment.hpp>
#include<chimbuko/core/util/error.hpp>
#include <unistd.h>
#include <cstring>
#include <cstdlib>

namespace chimbuko{
  
  const std::string &getHostname(){
    static std::string h;
    if(h.size() == 0){
      //Implementation thanks to http://www.microhowto.info/howto/determine_the_fully_qualified_hostname_of_the_local_machine_in_c.html
      size_t hostname_len=128;
      char* hostname=0;
      while (1) {
	// (Re)allocate buffer of length hostname_len.
	char* realloc_hostname=(char*)realloc(hostname,hostname_len);
	if (realloc_hostname==0) {
	  free(hostname);
	  fatal_error("Failed to get hostname (out of memory)");
	}
	hostname=realloc_hostname;

	// Terminate the buffer.
	hostname[hostname_len-1]=0;

	// Offer all but the last byte of the buffer to gethostname.
	if (gethostname(hostname,hostname_len-1)==0) {
	  size_t count=strlen(hostname);
	  if (count<hostname_len-2) {
	    // Break from loop if hostname definitely not truncated
	    break;
	  }
	}

	// Double size of buffer and try again.
	hostname_len*=2;
      }
      h = std::string(hostname);
      free(hostname);
    }
    return h;
  }


}

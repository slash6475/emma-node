#include "emma-node-example.h"
#include "erbium.h"
#include "emma-conf.h"
#include "emma-init.h"

#define LDEBUG 1
#if (LDEBUG | GDEBUG)
	#define PRINT(...) OUTPUT_METHOD("[EMMA EXAMPLE] " __VA_ARGS__)
	#define PRINTS(...) OUTPUT_METHOD(__VA_ARGS__)
#else
	#define PRINT(...)
	#define PRINTS(...)
#endif

PROCESS(emma_node_example, "EMMA Node Example");
AUTOSTART_PROCESSES(&emma_node_example);

PROCESS_THREAD(emma_node_example, ev, data)
{
  PROCESS_BEGIN();
	
	PRINTS("\n***********************************\n");
  PRINTS("**** Starting EMMA Node example ****\n");
  PRINTS("***********************************\n\n");
  
  // Init REST engine
  rest_init_engine();
  
  // Init EMMA
  emma_init();
  
 	PROCESS_END();
}

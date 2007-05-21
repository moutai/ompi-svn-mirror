#include <stdlib.h>
#include <stdio.h>

#include "orte/runtime/runtime.h"

int main( int argc, char **argv ) 
{
    int rc;
    
    if (ORTE_SUCCESS != (rc = orte_init(ORTE_NON_INFRASTRUCTURE, ORTE_NON_BARRIER))) {
        fprintf(stderr, "couldn't init orte - error code %d\n", rc);
        return rc;
    }
    sleep(1);
    orte_finalize();
    
    return 0;
}

/*
 * $HEADER$
 */

#include "mpi.h"
#include "mca/mpi/io/io.h"
#include "io_romio.h"


int mca_io_romio_module_open(void);
int mca_io_romio_module_close(void);
mca_io_1_0_0_t* mca_io_romio_module_init(int* priority, int* min_thread, int* max_thread);



mca_io_base_module_1_0_0_t mca_io_romio_module = {
    /* First, the mca_base_module_t struct containing meta information
       about the module itself */
                                                                                                                            
    {
    /* Indicate that we are a io v1.0.0 module (which also implies a
       specific MCA version) */

    MCA_IO_BASE_VERSION_1_0_0,
                                                                                                                            
    "romio", /* MCA module name */
    1,  /* MCA module major version */
    0,  /* MCA module minor version */
    0,  /* MCA module release version */
    mca_io_romio_module_open,  /* module open */
    mca_io_romio_module_close  /* module close */
    },
                                                                                                                            
    /* Next the MCA v1.0.0 module meta data */
                                                                                                                            
    {
    /* Whether the module is checkpointable or not */
                                                                                                                            
    false
    },

    mca_io_romio_module_init  /* module init */
};
                                                                                                                            

int mca_io_romio_module_open(void)
{
    return LAM_SUCCESS;
    /* register all mca parameters */
}


int mca_io_romio_module_close(void)
{
    return LAM_SUCCESS;
}




mca_io_1_0_0_t fp;

mca_io_1_0_0_t* mca_io_romio_module_init(int* priority, int* min_thread, int* max_thread)
{

    *priority=10;
    *min_thread = MPI_THREAD_SINGLE;
    *max_thread = MPI_THREAD_SERIALIZED;

    fp.io_File_open         = mca_io_romio_File_open;
    fp.io_File_close        = mca_io_romio_File_close;
    fp.io_File_delete       = mca_io_romio_File_delete;
    fp.io_File_set_size     = mca_io_romio_File_set_size;
    fp.io_File_preallocate  = mca_io_romio_File_preallocate;
    fp.io_File_get_size     = mca_io_romio_File_get_size;
    fp.io_File_get_group    = mca_io_romio_File_get_group;
    fp.io_File_get_amode    = mca_io_romio_File_get_amode;    
    fp.io_File_set_info     = mca_io_romio_File_set_info;
    fp.io_File_get_info     = mca_io_romio_File_get_info;


    return &fp;  

}

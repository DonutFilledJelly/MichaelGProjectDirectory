#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>

#include "mpi.h"

void Get_data2(int* a_ptr, int* b_ptr, int* count_ptr, int my_rank){//Takes in the data for the sizes and steps
	if (my_rank == 0){
		scanf("%d %d %d", a_ptr, b_ptr, count_ptr);
		
	}
	MPI_Bcast(a_ptr, 1, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Bcast(b_ptr, 1, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Bcast(count_ptr, 1, MPI_INT, 0, MPI_COMM_WORLD);
	
};



void main(int argc, char* argv[]){
	MPI_Status status;
	int provide;
	MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provide); //starts the thread split   
	double start, end;    
    	int *xsize, *ysize, *stepcount, *rank, rc;
	rank = (int *) malloc(sizeof(int));
	xsize = (int *) malloc(sizeof(int));
	ysize = (int *) malloc(sizeof(int));
	stepcount = (int *) malloc(sizeof(int));
	MPI_Comm_rank(MPI_COMM_WORLD, rank);
		
	Get_data2(xsize, ysize, stepcount, *rank);//gets xsize,ysize, and stepcount
	
	float **grid = (float **)malloc((*ysize) * sizeof(float*));
	for(int i = 0; i < *ysize; i++){//allocates grid
		grid[i] = (float *)malloc((*xsize)*sizeof(float));
	}
	if(*rank == 0){
//	printf("OG grid: \n");	
	for(int i = 0; i < *ysize; i++){//scans grid only on manager thread
			for(int p = 0; p < *xsize; p++){
				scanf("%f", &grid[i][p]);	
				}
		}
	}
   //printf("Created grid\n"); 

    int i, p;
    
    double time0, time1;
//    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int commsize;
    int processstart, processend;

	/*
		Processstart = Where on the y axis will the process start
		Processend = Where on the y axis  the process will end
	*/
    MPI_Comm_size(MPI_COMM_WORLD, &commsize);    
    time0 = MPI_Wtime();//keeps track of time
	if(*ysize%commsize != 0){//Tries to split work evenly
		processstart = (*ysize/commsize)*(*rank) +(*rank);
		processend = (*ysize/commsize)*(*rank) + (*rank) + (*ysize/commsize) + 1;
	}
	else{
		processstart = (*ysize/commsize)*(*rank);
		processend = (*ysize/commsize)*(*rank) + (*ysize/commsize);
	}
	
	/*
 * 	This for loop  goes through each step, starting off by updating the grid, and then
 * 	finding the new values within the permissioned perimeters. It then sends to root node, 
 * 	where the root node will pick up and add it to a new grid. Repeating the process til
 * 	all steps are complete.
 * 	*/
   for(int curstep = 0; curstep < *stepcount; curstep++){    
	//printf("in for loop\n");	
	
	for(int l = 0; l < *ysize; l++){//update grid
        	MPI_Bcast(grid[l], *xsize , MPI_FLOAT, 0, MPI_COMM_WORLD);
        }
	
	
	int redblack = (curstep%2);//which type of step are we on?
        float averagetmp[(*xsize)*(*ysize)/(commsize)/2];//Used to hold the values. Allocation is an estimate
        
        int k = 0;
	//printf("Before for\n");
	for(i = processstart; i < processend; i++){
            if(i > 0 && i < *ysize-1){
                for(p = redblack+(i%2); p < *xsize - 1; p = p+2){
			if(p != 0){
                       	
                        averagetmp[k] = (grid[(i+1)][p] + grid[(i-1)][p] + grid[i][(p+1)] + grid[i][(p-1)] + grid[i][p])/5;
                    
			k++;//keeps track of how many elements are in averagetmp
                	          
            }
            }
            }
            
            
        }
	
	float *averagetmps;//used by Manager to store all tmps
		
	if(*rank != 0){//these save the tmp
		MPI_Send(&k, 1, MPI_INT, 0, 1, MPI_COMM_WORLD);
		MPI_Send(averagetmp, k, MPI_FLOAT, 0, 1, MPI_COMM_WORLD);	
	}	
        if(*rank == 0){
	    
            averagetmps = (float *)malloc(sizeof(float)*(*xsize)*(*ysize));
      	  int tmpcount = 0;
        
	for(int e = 0; e < commsize; e++){	
		if(e != 0) {
			MPI_Recv(&k, 1, MPI_INT, e, 1, MPI_COMM_WORLD, &status);		
			MPI_Recv(averagetmp, k, MPI_FLOAT, e, 1, MPI_COMM_WORLD, &status);
		}
		for(int r = 0; r < k; r++){
			averagetmps[tmpcount] = averagetmp[r];
			tmpcount++;		
		}
	}	
	int k = 0;
	for(i = 1; i < *ysize-1; i++){
            for(p = redblack+(i%2); p < *xsize-1; p = p+2){
                if(p != 0){
                    
                    grid[i][p] = averagetmps[k];//Saves the new tmps into the grid
                    k++;
                }
            }

        }
	}
	
        
    }//end of for loop
	
  

if(*rank == 0){//prints out the completed grid and time
    for(int i = 0; i < *ysize; i++){
		
        	for(int p = 0; p < *xsize; p++){
        	    	printf(" %f", grid[i][p]);
    	}
	printf("\n");
    }

        printf("\n");  
        printf("TIME %.5f s\n", MPI_Wtime()-time0);
}



free(xsize);
free(ysize);
free(stepcount);
free(rank);
MPI_Finalize();
}

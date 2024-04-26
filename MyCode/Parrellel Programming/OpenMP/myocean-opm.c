#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>

#include "omp.h"


void main(){
    double start, end;
    start = omp_get_wtime();
    int threadnum, xsize, ysize, stepcount;
    scanf("%d", &threadnum);
    scanf("%d", &xsize);
    scanf("%d", &ysize); 
    scanf("%d", &stepcount);
    omp_set_num_threads(threadnum);
    
    float grid[xsize*8][ysize*8];
    for(int i = 0; i < ysize; i++){
        for(int p = 0; p < xsize; p++){
            scanf("%f", &grid[p*8][i*8]);
        }
    }
    int i, p;
    #pragma omp parallel private(i, p) shared(grid)
    for(int curstep = 0; curstep < stepcount; curstep++){
        int redblack = (curstep%2);
        #pragma omp for 
        for(i = 1; i < ysize-1; i++){
 
            for(p = redblack+(i%2); p < xsize - 1; p = p+2){
                if(p != 0){
                float averagetmp;
                averagetmp = (grid[8*(p+1)][8*i] + grid[8*(p-1)][8*i] + grid[8*p][8*(i+1)] + grid[8*p][8*(i-1)] + grid[8*p][8*i])/5;
 
                grid[p*8][i*8] = averagetmp;
            }
            }
            
        }
        #pragma omp barrier
    }
    

    for(int i = 0; i < ysize; i++){
        printf("\n");
        for(int p = 0; p < xsize; p++){
            printf(" %f", grid[p*8][i*8]);
        }
    }
    printf("\n");  
    end = omp_get_wtime();
    printf("TIME %.5f s\n", end-start);
}
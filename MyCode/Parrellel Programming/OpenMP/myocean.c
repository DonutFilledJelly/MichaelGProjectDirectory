#include <stdio.h>
#include <omp.h>
#include <stdlib.h>

void main(){
    double start, end;
    start = omp_get_wtime();
    int xsize, ysize, stepcount;
    scanf("%d", &xsize);
    scanf("%d", &ysize); 
    scanf("%d", &stepcount);
    float grid[xsize][ysize];
    for(int i = 0; i < ysize; i++){
        for(int p = 0; p < xsize; p++){
            scanf("%f", &grid[p][i]);
        }
    }
    
    for(int curstep = 0; curstep < stepcount; curstep++){
        int redblack = (curstep%2);
        for(int i = 1; i < ysize - 1; i++){
            for(int p = redblack+(i%2); p < xsize - 1; p = p+2){
                if(p != 0){
                float averagetmp;
                averagetmp = (grid[p][i+1] + grid[p][i-1] + grid[p-1][i] + grid[p+1][i] + grid[p][i])/5;
                grid[p][i] = averagetmp;
                }
            }
        }
    }
    for(int i = 0; i < ysize; i++){
        printf("\n");
        for(int p = 0; p < xsize; p++){
            printf(" %f", grid[p][i]);
        }
    }
    end = omp_get_wtime();
    printf("\n");
    printf("TIME %.5f s\n", end-start);
}
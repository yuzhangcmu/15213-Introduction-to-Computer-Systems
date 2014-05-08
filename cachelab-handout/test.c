#include <stdio.h>
#include <stdlib.h>

int main(){

	int n, sn, m, sm, i, j; 

	// for (n = 0; n < 57; n += 8){
 //        for(m = 0; m < 57; m += 8 ){
 //            if(m != n){
 //                for(i = n; i < n+8; i++){
 //                    for(j =m; j < m+8; j++){
 //                        printf("[%d,%d] ", i, j);
 //                    }
 //                    printf("\n");
 //                }
 //            }
 //        }
 //    }

    for (n = 0; n < 57; n += 8){
        for(i = n; i < n+8; i++){
            for(j = n; j < n+8; j++){
                if(i != j){
                 	printf("[%d,%d] ", i, j);
                }
            }
            printf("[%d,%d] ", i, i);
            printf("\n");    
            
        }    

    }

	// for(n = 0; n < 57; n +=8){
 //        for(m = 0; m < 57; m += 8){
 //            if(m != n){
 //                sn = n;
 //                for(sm = m; sm < m + 5; sm += 4){
 //                    for(i = sn; i < sn+4; i++){
 //                        for(j = sm; j < sm+4; j++){
 //                            // if(i!=j)
 //                            // B[j][i] = A[i][j];
 //                            printf("[%d,%d] ", i, j);
 //                        }
 //                        printf("\n");
 //                    }
 //                }
 //                // sn = n+4;
 //                // for(sm = m + 4; sm > m-1; sm -=4){
 //                //     for(i = sn; i < sn+4; i++){
 //                //         for(j = sm; j < sm+4; j++){
 //                //             // if(i!=j)
 //                //             B[j][i] = A[i][j];
 //                //         }       
 //                //     }   
 //                // }
 //          }
 //        }   
 //    }
    // for(n = 0; n < 57; n+=8){
    //     sn = n;
    //     for(i = sn; i < sn+4; i++){
    //         for(j = sn; j < sn+4; j++){
    //             if(i != j){
    //                 B[j][i] = A[i][j];
    //             }
    //         }
    //         B[i][i] = A[i][i];
    //     }

    //     for(i = sn; i < sn+4; i++){
    //         for(j = sn+4; j < sn+8; j++){
    //             B[j][i] = A[i][j];
    //         }
    //     }
    //     sn = n+4;
    //     for(i = sn; i < sn+4; i++){
    //         for(j = sn; j < sn+4; j++){
    //             if(i != j){
    //                 B[j][i] = A[i][j];
    //             }
    //         }
    //         B[i][i] = A[i][i];
    //     }

    //     for(i = sn; i < sn+4; i++){
    //         for(j = sn-4; j < sn; j++){
    //             B[j][i] = A[i][j];
    //         }
    //     }
    // }   
}
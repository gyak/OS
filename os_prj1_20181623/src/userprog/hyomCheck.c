#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>

int main()
{
    FILE* fp = fopen("build/results","r");
    char line[22][500];
    char temp[500];
    memset(temp, 0, sizeof(temp));
    for(int i = 1; fgets(temp, sizeof(temp), fp) != NULL; i++){
        if(i == 1)
            strcpy(line[1], temp); 
        else if(i == 2)
            strcpy(line[2], temp);                    
        else if(i == 3)
            strcpy(line[3], temp); 
        else if(i == 4)
            strcpy(line[4], temp); 
        else if(i == 5)
            strcpy(line[5], temp); 
        else if(i == 44)
            strcpy(line[6], temp); 
        else if(i == 49)
            strcpy(line[7], temp); 
        else if(i == 45)
            strcpy(line[8], temp); 
        else if(i == 52)
            strcpy(line[9], temp); 
        else if(i == 53)
            strcpy(line[10], temp); 
        else if(i == 56)
            strcpy(line[11], temp); 
        else if(i == 12)
            strcpy(line[12], temp); 
        else if(i == 11)
            strcpy(line[13], temp); 
        else if(i == 51)
            strcpy(line[14], temp); 
        else if(i == 50)
            strcpy(line[15], temp); 
        else if(i == 7)
            strcpy(line[16], temp); 
        else if(i == 6)
            strcpy(line[17], temp); 
        else if(i == 8)
            strcpy(line[18], temp); 
        else if(i == 9)
            strcpy(line[19], temp); 
        else if(i == 55)
            strcpy(line[20], temp); 
        else if(i == 54)
            strcpy(line[21], temp); 
        memset(temp, 0, sizeof(temp));
    }
    fclose(fp);
    
    for(int i = 1; i<22; i++)
        printf("NO.%d\t: %s", i, line[i]);
    return 0;
}

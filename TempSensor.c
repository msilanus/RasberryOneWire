#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>

int main(int argc, char * argv[])
{
    int fd;
    char *path = "/sys/bus/w1/devices/28-0000034a8e9d/w1_slave";
	
	struct timeval tv;
    	char buffer[1000];
    
	
    while (1) {
        // Ouvrir le fichier
	if ((fd = open(path, O_RDONLY)) < 0) 
	{
		perror("Erreur ouverture de value de w1_slave");
		exit(EXIT_FAILURE);
	}
        int len=read(fd, buffer, sizeof(buffer));
	char temp[10];
	strncpy(temp, buffer+len-6, 5);	
	float temperature;
	temperature=atof(temp)/1000;
        printf("Temp = %2.3f°C\n", temperature);
	close(fd);
   	sleep(1);

		
    }
    
    return EXIT_SUCCESS;
}

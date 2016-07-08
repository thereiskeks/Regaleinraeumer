#include <stdio.h> //printf
#include "HRL_Steuerung.h"
#include "simulation.h"
#include "readcommand.h"
#include "visualisierung.h"

bool belegung[10][5];

void main_user_input();
void main_init();

main(void){
	printf("System starting \n");
	
	
	visualisierung_init();
	if ( Simulation_init() == (-1) ){
		printf("Simulation_init fehlgeschlagen");
		return 0;
	}
	else if ( HRL_Steuerung_init() == (-1) ){
		printf("HRL_Steuerung_init fehlgeschlagen");
		return 0;
	}
	main_init();
	
	main_user_input();
}

void main_init(){
	int x, y;
	for (x = 0; x < 10; x++) {
		for (y = 0; y < 5; y++) {
			belegung[x][y]=false;
			belegungsMatrix[x][y]=false;
		}
	}
}

void main_user_input(){
	command cmd;
	cmdQdata cmdQ;
	
	taskPrioritySet(taskIdSelf(), Priority_Main ); //eigene Prio runter 
	
	
	while(1){
		cmd = readcommand();
		cmd.par2=4-cmd.par2;
		
		if (cmd.parse_ok){
			if ( (cmd.par1 < 0) || (cmd.par1 > 9) ){
				printf("x-Adressierung au�erhalb von 0 bis 9 \nUngueltige Eingabe ! \n");
				cmd.parse_ok = false;
			}
			else if ( (cmd.par2 < 0) || (cmd.par2 > 4) ){
				printf("y-Adressierung au�erhalb von 0 bis 4 \nUngueltige Eingabe ! \n");
				cmd.parse_ok = false;
			}
			else {
				//printf("cmd: %s; belegung: %d \n", cmd.cmd, belegung[cmd.par1][cmd.par2] );
				if ( (strcmp(cmd.cmd, "insert") == 0) && (belegung[cmd.par1][cmd.par2]) ){
					printf("insert nicht m�glich - angegebene Position[%d][%d] ist belegt\nUngueltige Eingabe ! \n", cmd.par1, 4-cmd.par2);
					cmd.parse_ok=false;
				}
				else if ( (strcmp(cmd.cmd, "remove")==0) && (belegung[cmd.par1][cmd.par2] == false) ){
					printf("remove nicht m�glich - angegebene Position[%d][%d] ist leer\nUngueltige Eingabe ! \n", cmd.par1, 4-cmd.par2);
					cmd.parse_ok=false;
				}
			}
		}
		else{
			printf("Ein Befehl + 2 Koordinaten \nUngueltige Eingabe ! \n");
		}
		if (cmd.parse_ok) {
			cmdQ.bits.x = cmd.par1;
			cmdQ.bits.y = cmd.par2;			
			if ( strcmp(cmd.cmd, "vsetspace") == 0 ){
				belegung[cmd.par1][cmd.par2] = true;
				cmdQ.bits.highprio=1;
				cmdQ.bits.cmd=1;	
				//printf("Nach n�chstem Job: + Belegung + (%d - %d) \n", cmd.par1, cmd.par2);
				
			}
			else if (strcmp(cmd.cmd, "clearspace") == 0){
				belegung[cmd.par1][cmd.par2] = false;
				cmdQ.bits.highprio=1;
				cmdQ.bits.cmd=0;
				//printf("Nach n�chstem Job: - Belegung - (%d - %d) \n", cmd.par1, cmd.par2);
							
			}
			else if (strcmp(cmd.cmd, "insert") == 0){
				belegung[cmd.par1][cmd.par2] = true;
				cmdQ.bits.highprio=0;
				cmdQ.bits.cmd=1;
				//printf("In Queue aufgenommen: + Einlagerung + (%d - %d) \n", cmd.par1, cmd.par2);
			}
			else if (strcmp(cmd.cmd, "remove") == 0){
				belegung[cmd.par1][cmd.par2] = false;
				cmdQ.bits.highprio=0;
				cmdQ.bits.cmd=0;
				//printf("In Queue aufgenommen: - Auslagerung - (%d - %d) \n", cmd.par1, cmd.par2);
			}
			
			if((msgQSend(mesgQueueIdCmd, cmdQ.charvalue, sizeof(cmdQ.charvalue), WAIT_FOREVER, cmdQ.bits.highprio)) == ERROR)
				printf("msgQSend in User-Input failed\n");			
		}
		taskDelay(1);
	}
}

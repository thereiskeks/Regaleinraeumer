#include "HRL_Steuerung.h"

/*
 * HRL_Steuerung.c
 *
 *  Created on: 27.06.2016
 *      Author: Zauberer
 */
#define DontCare (-1)
#define Z_IO 2
#define Z_Middle 1
#define Z_Inside 0
#define Carry_Get 1
#define Carry_Give 0
#define IO_GetBreak 1
#define IO_GetFree 0

bool shutdown_HRL_Steuerung;
bool belegungsMatrix[10][5];
int lastSensorX;
int lastSensorY;
int lastSensorZ;
int lastOutputState;
int lastInputState;
int lastCarryState;

typedef struct {
	int x;
	int y;
	int z;
	int IO;
	int carry;
} NextMovement;

typedef union{
	NextMovement move;
	char charvalue[sizeof(NextMovement)+1];
}NextMovementUNION;

MSG_Q_ID mesgQueueIdNextMovement;
MSG_Q_ID mesgQueueIdAktorDataPush;

void HRL_Steuerung_AktorDataPush();
void HRL_Steuerung_Movement();
void HRL_Steuerung_Movement_GetSensorBusData();
void HRL_Steuerung_Movement_GetSensorBusData_ERROR(char* msg);
void HRL_Steuerung_GetNewJob();
NextMovement HRL_Steuerung_GetNewJob_DontCareState();
void HRL_Steuerung_GetNewJob_Qsend();

int HRL_Steuerung_init(){
	printf("Start: HRL-Steuerung \n");
	bool abort = false;
	//TODO: sizeof() hinzuf�gen
	if ((mesgQueueIdNextMovement = msgQCreate(5,3,MSG_Q_FIFO))	== NULL){
		printf("msgQCreate (NextMovement) in HRL_Steuerung_init failed\n");
		abort = true;
	}
	
	if ((mesgQueueIdCmd = msgQCreate(MSG_Q_CMD_MAX_Messages,1,MSG_Q_PRIORITY))	== NULL){
		printf("msgQCreate (CMD) in HRL_Steuerung_init failed\n");
		abort = true;
	}
	
	// Aktor Data Push
	if ((mesgQueueIdAktorDataPush = msgQCreate(2,1,MSG_Q_FIFO))	== NULL){
		printf("msgQCreate (AktorDataPush) in HRL_Steuerung_init failed\n");
		abort = true;
	}
	
	if (abort){
		return (-1);
	}
	else{
		taskSpawn("HRL_Steuerung_AktorDataPush",120,0x100,2000,(FUNCPTR)HRL_Steuerung_AktorDataPush,0,0,0,0,0,0,0,0,0,0);
		
		return 0;
	}
		
	
}

void HRL_Steuerung_AktorDataPush(){
	abusdata transmit;
	while (1){
		if(msgQReceive(mesgQueueIdAktorDataPush, transmit.amsg, sizeof(transmit.amsg), WAIT_FOREVER) == ERROR) 
			printf("msgQReceive in HRL_Steuerung_AktorDataPush failed\n");
		else{
			semTake(semBinary_SteuerungToSimulation, WAIT_FOREVER); 
			SteuerungToSimulation = transmit;
			semGive(semBinary_SteuerungToSimulation);
		}
	}
}

void HRL_Steuerung_Movement(){
	
	//TODO: aktorik msgQ darf trotz pause nicht voll laufen
	
	NextMovementUNION nextmove;
	bool waitForSensor;
	abusdata aktorData;
	sbusdata sensorData;
	while(1){
		HRL_Steuerung_GetNewJob(); //auto pause
		
		while(msgQNumMsgs(mesgQueueIdNextMovement) > 0){
			if(msgQReceive(mesgQueueIdNextMovement, nextmove.charvalue, sizeof(nextmove.charvalue), WAIT_FOREVER) == ERROR)
				printf("msgQ (NextMove) Receive in HRL_Steuerung_Movement failed\n");	
			
			waitForSensor = true;
			while (waitForSensor){
				HRL_Steuerung_Movement_GetSensorBusData(); //auto pause
				waitForSensor = false;
				
				//  X
				if ( (lastSensorX != nextmove.move.x) && (nextmove.move.x != DontCare) ){
					waitForSensor = true;
					//TODO: speed nutzen
					if (lastSensorX < nextmove.move.x) //TODO: vereinfachen?
						aktorData.abits.axr = 1;
					else 
						aktorData.abits.axr = 0;
					
					if (lastSensorX > nextmove.move.x)
						aktorData.abits.axl = 1;
					else 
						aktorData.abits.axl = 0;					
				}
				
				//  Y
				if ( (lastSensorY != nextmove.move.y) && (nextmove.move.y != DontCare) ){
					waitForSensor = true;
					if (lastSensorY < nextmove.move.y)
						aktorData.abits.ayu = 1;
					else 
						aktorData.abits.ayu = 0;
					
					if (lastSensorY > nextmove.move.y)
						aktorData.abits.ayo = 1;
					else 
						aktorData.abits.ayo = 0;					
				}
				else 
					nextmove.move.y = DontCare;
				
				//  Z
				if ( (lastSensorZ != nextmove.move.z) && (nextmove.move.z != DontCare) ){
					waitForSensor = true;
					if (lastSensorZ < nextmove.move.z)
						aktorData.abits.azv = 1;
					else 
						aktorData.abits.azv = 0;
					
					if (lastSensorY > nextmove.move.y)
						aktorData.abits.azh = 1;
					else 
						aktorData.abits.azh = 0;					
				}
				
				//  Licht
				// Licht-Taster
				if ( (lastCarryState != nextmove.move.carry) && (nextmove.move.carry != DontCare) ){
					if ( (nextmove.move.carry == Carry_Get) && (!waitForSensor) ) {
						aktorData.abits.ayu = 0;
						aktorData.abits.ayo = 1;
					}
					if ( (nextmove.move.carry == Carry_Give) && (!waitForSensor) ) {
						aktorData.abits.ayu = 1;
						aktorData.abits.ayo = 0;
					}
					waitForSensor = true;
				}
				
				// Lichtschranke
				if (nextmove.move.IO != DontCare){
					waitForSensor = true;
					if ( (lastSensorX == PositionXinput) && (lastSensorY == PositionYinput) ){ //wir sind beim Input
						if (nextmove.move.IO == IO_GetBreak){
							aktorData.abits.aealre=1;
							aktorData.abits.aealra=0;
						}
						else{ //GetFree
							aktorData.abits.ayu = 0;
							aktorData.abits.ayo = 1;
						}
					}
					else if ( (lastSensorX == PositionXOutput) && (lastSensorY == PositionYOutput) ){ //wir sind beim Output
						if (nextmove.move.IO == IO_GetFree){
							aktorData.abits.aearra=1;
							aktorData.abits.aearre=0;
						}
						else{ //GetBreak
							aktorData.abits.ayo = 0;
							aktorData.abits.ayu = 1;
						}
					}
				}//end Lichtschranke
			}//end waitForSensor
		}//MovementQ
	}//while(1)	
}

void HRL_Steuerung_Movement_clearAktor(){
	//abusdata
}


void HRL_Steuerung_Movement_GetSensorBusData(){
	sbusdata returnvalue;
	int i, errorcount;
	if(msgQReceive(mesgQueueIdSensorData, returnvalue.smsg, sizeof(returnvalue.smsg), WAIT_FOREVER) == ERROR){ 
		printf("msgQReceive in HRL_Steuerung_Movement_GetSensorBusData failed\n");
		returnvalue.l = 0;
	}
	else{
		
		// X-Achse
		errorcount= (-1);
		for (i = 0; i < 10; i++) {
			if ( (returnvalue.l & (1<(i+10)) ) == 0)
			{
				lastSensorX = i;
				errorcount++;
			}
		}
		if (errorcount>0){
			HRL_Steuerung_Movement_GetSensorBusData_ERROR("mehrere X-Sensoren ausgel�st");
		}
		
		// Y-Achse
		errorcount= (-1);
		//unten
		for (i = 0; i < 5; i++) {
			if ( (returnvalue.l & (1<i) ) == 0)
			{
				lastSensorY = i*2+1;
				errorcount++;
			}
		}
		//oben
		for (i = 0; i < 5; i++) {
			if ( (returnvalue.l & (1<(i+5)) ) == 0)
			{
				lastSensorY = i*2;
				errorcount++;
			}
		}
		if (errorcount>0){
			HRL_Steuerung_Movement_GetSensorBusData_ERROR("mehrere Y-Sensoren ausgel�st");
		}
		
		// Z-Achse
		errorcount= (-1);
		for (i = 0; i < 3; i++) {
			if ( (returnvalue.l & (1<(i+20)) ) == 0)
			{
				lastSensorZ = i;
				errorcount++;
			}
		}
		if (errorcount>0){
			HRL_Steuerung_Movement_GetSensorBusData_ERROR("mehrere Z-Sensoren ausgel�st");
		}
		
		//licht
		if (!returnvalue.sbits.lL){
			lastInputState = 1;
		}
		if (!returnvalue.sbits.lR){
			lastOutputState = 1;
		}
		if (!returnvalue.sbits.lT){
			lastCarryState = 1;
		}
		
	}
	//return returnvalue;
}

void HRL_Steuerung_Movement_GetSensorBusData_ERROR(char* msg){
	printf("Sensoric ERROR: %s \nSystem Stop\n", msg);
	abusdata transmit;
	transmit.i=0;
	msgQReceive(mesgQueueIdAktorDataPush, transmit.amsg, sizeof(transmit.amsg), WAIT_FOREVER);
	taskDelete(taskIdSelf());
}

void HRL_Steuerung_GetNewJob()
{
	//obere Prio zuerst //danach normal
	NextMovement next3D;
	cmdQdata cmdData;

	static int pause;
	int timeout[2] = {WAIT_FOREVER, 350};
	
	if(msgQReceive(mesgQueueIdCmd, cmdData.charvalue, sizeof(cmdData.charvalue), timeout[pause]) == ERROR) {//etwa 5 Sekunden Timeout
		if( msgQNumMsgs(mesgQueueIdCmd) == 0){
			pause = 0;
			// Pause-Modus einleiten - Timeout
			pause = true;
			next3D = HRL_Steuerung_GetNewJob_DontCareState();
			next3D.x = PositionXinput;
			next3D.y = PositionYinput;
			HRL_Steuerung_GetNewJob_Qsend(next3D);		
		}
	}
	else{
		pause = 1;
		if (cmdData.bits.highprio){
			belegungsMatrix[cmdData.bits.x][cmdData.bits.y] = cmdData.bits.cmd;
		}else if (cmdData.bits.cmd){//insert xy
			// 1 - zum Einlader
			next3D = HRL_Steuerung_GetNewJob_DontCareState();
			next3D.x = PositionXinput;
			next3D.y = PositionYinput+1;
			HRL_Steuerung_GetNewJob_Qsend(next3D);
			// 2 - Arm ausfahren und auf P�ckchen warten
			next3D = HRL_Steuerung_GetNewJob_DontCareState();
			next3D.z = Z_IO;
			next3D.IO = IO_GetBreak;
			HRL_Steuerung_GetNewJob_Qsend(next3D);
			// 3 - P�ckchen auf Arm fahren
			next3D = HRL_Steuerung_GetNewJob_DontCareState();
			next3D.carry = Carry_Get;
			next3D.IO = IO_GetFree;
			next3D.y = PositionYinput-1; 
			HRL_Steuerung_GetNewJob_Qsend(next3D);
			// 4 - Arm einfahren
			next3D = HRL_Steuerung_GetNewJob_DontCareState();
			next3D.z = Z_Middle;
			HRL_Steuerung_GetNewJob_Qsend(next3D);
			// 5 - zu Einlagerungsstelle fahren (y-oben)
			next3D = HRL_Steuerung_GetNewJob_DontCareState();
			next3D.x = cmdData.bits.x;
			next3D.y = cmdData.bits.y*2;
			HRL_Steuerung_GetNewJob_Qsend(next3D);
			// 6 - Arm ausfahren
			next3D = HRL_Steuerung_GetNewJob_DontCareState();
			next3D.z = Z_Inside;
			HRL_Steuerung_GetNewJob_Qsend(next3D);
			// 7 - in Box absenken (y-unten)
			next3D = HRL_Steuerung_GetNewJob_DontCareState();
			next3D.y = cmdData.bits.y*2+1;
			HRL_Steuerung_GetNewJob_Qsend(next3D);
			// 8 - Arm einfahren
			next3D = HRL_Steuerung_GetNewJob_DontCareState();
			next3D.z = Z_Middle;
			HRL_Steuerung_GetNewJob_Qsend(next3D);	
			
		}else{//remove xy
			// 1 - zu Einlagerungsstelle fahren (y-unten)
			next3D = HRL_Steuerung_GetNewJob_DontCareState();
			next3D.x = cmdData.bits.x;
			next3D.y = cmdData.bits.y*2+1;
			HRL_Steuerung_GetNewJob_Qsend(next3D);
			// 2 - Arm ausfahren
			next3D = HRL_Steuerung_GetNewJob_DontCareState();
			next3D.z = Z_Inside;
			HRL_Steuerung_GetNewJob_Qsend(next3D);
			// 3 - in Box heben (y-oben)
			next3D = HRL_Steuerung_GetNewJob_DontCareState();
			next3D.y = cmdData.bits.y*2;
			HRL_Steuerung_GetNewJob_Qsend(next3D);
			// 4 - Arm einfahren
			next3D = HRL_Steuerung_GetNewJob_DontCareState();
			next3D.z = Z_Middle;
			HRL_Steuerung_GetNewJob_Qsend(next3D);
			// 5 - zum Auslader
			next3D = HRL_Steuerung_GetNewJob_DontCareState();
			next3D.x = PositionXOutput;
			next3D.y = PositionYOutput-1;
			HRL_Steuerung_GetNewJob_Qsend(next3D);
			// 6 - Arm ausfahren und auf freien Slot warten
			next3D = HRL_Steuerung_GetNewJob_DontCareState();
			next3D.z = Z_IO;
			next3D.IO = IO_GetFree;
			HRL_Steuerung_GetNewJob_Qsend(next3D);
			// 7 - P�ckchen von Arm fahren
			next3D = HRL_Steuerung_GetNewJob_DontCareState();
			next3D.carry = Carry_Give;
			next3D.IO = IO_GetBreak;
			next3D.y = PositionYOutput+1;
			HRL_Steuerung_GetNewJob_Qsend(next3D);
			// 8 - Arm einfahren
			next3D = HRL_Steuerung_GetNewJob_DontCareState();
			next3D.z = Z_Middle;
			HRL_Steuerung_GetNewJob_Qsend(next3D);
		}
	}		
}

NextMovement HRL_Steuerung_GetNewJob_DontCareState(){
	NextMovement returnValue; // 0=stop f�r den Motor; 0<sensorwert-1
	returnValue.carry = DontCare;
	returnValue.IO= DontCare;
	returnValue.x=  DontCare;
	returnValue.y=  DontCare;
	returnValue.z=  DontCare;

	return returnValue;
}

void HRL_Steuerung_GetNewJob_Qsend(NextMovement move){
	NextMovementUNION transmit;
	transmit.move = move;
	if((msgQSend(mesgQueueIdNextMovement, transmit.charvalue, sizeof(transmit.charvalue), WAIT_FOREVER, MSG_PRI_NORMAL)) == ERROR)
		printf("msgQSend in HRL_Steuerung_GetNewJob failed\n");		
}



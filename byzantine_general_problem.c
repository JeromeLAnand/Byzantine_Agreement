#include <stdlib.h>
#include <stdio.h>
#define __USE_GNU
#include <pthread.h>
#include <sys/time.h>

//#define VERBOSE
#ifdef VERBOSE
	/*verbose logs*/
	#define printV printf
#else
	#define printV
#endif

#define MAX_PROCESSORS 20
#define MAX_MESSAGES 1024
#define GENERAL_INDEX 1

typedef enum {
	LOYAL		= 1,
	TRAITOR		= 2,
}eLtType;

typedef enum {
	ATTACK		= 1,
	RETREAT		= 2,
	COMPROMISED	= 3,
	CONFUSE_PEER	= 4,
	UNKNOWN		= 5,
	MSG_MAX		= 6,
} eMessage;

typedef struct Processor {
	pthread_mutex_t 	p_mutex;
	pthread_cond_t  	p_cond;
	int 			proc_index;
	char* 			proc_name;
	pthread_t 		proc_thrd;
	int 			num_dsystems;
	eLtType	 		LtType;
	eMessage 		txmsg;
	int 			rxMsgQ[MAX_MESSAGES];
	int 			WaitingMsgCnt;
	int 			RetreatMsgCnt;
	int 			AttackMsgCnt;
	int 			CompromisedMsgCnt;
	int	 		FaultyMsgCnt;
	int 			UnknownMsgCnt;
	int 			majority;
	int 			waiting;
	int 			consensus;
} s_prcr;


/*globals*/
pthread_cond_t  p_cond_ACK  = PTHREAD_COND_INITIALIZER;
pthread_cond_t  p_cond      = PTHREAD_COND_INITIALIZER;
pthread_mutex_t a_mutex     =  PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
static int _lieutenants_tx_processed = 0;

struct Processor* Processor_List[MAX_PROCESSORS];
static int gMessage = ATTACK;
static int gTraitorCount = 0;

#define ARR_SIZE(arr) sizeof(arr)/sizeof(arr[1])

char* IntToStr(int msg) {
	switch(msg) {
		case 1:
			return "ATTACK";
		case 2:
			return "RETREAT";
		case 3:
			return "COMPROMISED";
		case 4:
			return "CONFUSE_PEER";
		default:
			return "UNKNOWN";
	}
}

void TransmitMessage(struct Processor *srcNode, struct Processor* dstnNode) {

	int msgcnt = dstnNode->WaitingMsgCnt;
	pthread_mutex_lock(&dstnNode->p_mutex);
	printf("\n\t Sending Message '%s' from Lieutenant '%d' to '%d'", IntToStr(srcNode->txmsg),srcNode->proc_index, dstnNode->proc_index);
	dstnNode->rxMsgQ[msgcnt++] = srcNode->txmsg;
	dstnNode->WaitingMsgCnt++;
	pthread_mutex_unlock(&dstnNode->p_mutex);
	return;
}

void SendMessage (void *param, int om, int parent) {
	int numsys, idx=0, inIdx, newIdx, ncount = 0, nparent = 0;
	struct Processor* pNode = (struct Processor*)param;

	numsys = pNode->num_dsystems;
	
	printf("\n --OM(%d)--",om);
	if (om<0) return;
	--om;


	inIdx = pNode->proc_index;
	for (idx=1 ;idx<=numsys; idx++) {
		/*dont send to general*/
		if ((idx == GENERAL_INDEX) || ((parent != GENERAL_INDEX)&&(inIdx == GENERAL_INDEX))) {
			continue;
		}
		/*dont send to self*/
		if ((idx == inIdx)) {
			continue;
		}
		/*dont send to parent*/
		if (idx == parent) {
			continue;
		}
		TransmitMessage(pNode, Processor_List[idx]);
		++ncount;
	}
	printV("\n number of children = %d", ncount);

	newIdx = (inIdx+1)%numsys;
	if (newIdx == 0) newIdx = numsys;
	if (newIdx == GENERAL_INDEX) newIdx = newIdx+1;


	if (inIdx != GENERAL_INDEX) {
		while (ncount > 0) {	
			nparent = Processor_List[newIdx]->proc_index;
			SendMessage(Processor_List[newIdx], om, parent);
			newIdx = (nparent+1)%numsys;
			if (newIdx == 0) newIdx = numsys;
			if (newIdx == inIdx) newIdx = newIdx+1;
			if (newIdx == GENERAL_INDEX) newIdx = newIdx+1;
			--ncount;
		};
	}
}

int FindMajorityMessage(void *param) {

	struct Processor* pNode = (struct Processor*)param;
	int attack, retreat,compromise,faulty,unknown;
	int majority;

	/*this looks ugly - need to redesign*/
	attack	    = pNode->AttackMsgCnt;
	retreat     = pNode->RetreatMsgCnt;
	compromise  = pNode->CompromisedMsgCnt;
	faulty      = pNode->FaultyMsgCnt;
	unknown	    = pNode->UnknownMsgCnt;

	printV("\nattack = %d, retreat = %d, compromis = %d, faulty = %d, unknown = %d",attack,retreat,compromise,faulty,unknown);
	if ((attack > retreat) && (attack > compromise) && (attack > faulty) && (attack > unknown))
		majority = ATTACK;
	else if ((retreat > attack) && (retreat > compromise) && (retreat > faulty) && (retreat > unknown))
		majority = RETREAT;
	else if ((compromise > retreat) && (compromise > attack) && (compromise > faulty) && (compromise > unknown))
		majority = COMPROMISED;
	else if ((faulty > retreat) && (faulty > compromise) && (faulty > attack) && (faulty > unknown))
		majority = CONFUSE_PEER;
	else if ((unknown > retreat) && (unknown > compromise) && (unknown > faulty) && (unknown > attack))
		majority = UNKNOWN;
	else
		majority = UNKNOWN;

	printV("\nmajority = %d",majority);
	return majority;
}

void ProcessMessage(void *param) {

	struct Processor* pNode = (struct Processor*)param;
	int msgcnt = pNode->WaitingMsgCnt;

	pthread_mutex_lock(&pNode->p_mutex);

	printf("\n\n\t Processing %d Messages Recieved ... \n",msgcnt);	
	printf("\n\t Lieutenant '%d' Message Map \n",pNode->proc_index);
	printf("\t ----------------------------\n");
	
	while (msgcnt >= 0) {
		if (pNode->rxMsgQ[msgcnt] == ATTACK) {
			pNode->AttackMsgCnt++;
		}
		if (pNode->rxMsgQ[msgcnt] == RETREAT) {
			pNode->RetreatMsgCnt++;
		}
		if (pNode->rxMsgQ[msgcnt] == COMPROMISED) {
			pNode->CompromisedMsgCnt++;
		}
		if (pNode->rxMsgQ[msgcnt] == CONFUSE_PEER) {
			pNode->FaultyMsgCnt++;
		}
		else if (pNode->rxMsgQ[msgcnt] == UNKNOWN) {
			pNode->UnknownMsgCnt++;
		}

		--msgcnt;
	}

	printf("\t Total Attack Messages      : %d \n",pNode->AttackMsgCnt);
	printf("\t Total Retreat Messages     : %d \n",pNode->RetreatMsgCnt);
	printf("\t Total Compromised Messages : %d \n",pNode->CompromisedMsgCnt);
	printf("\t Total Faulty Messages      : %d \n",pNode->FaultyMsgCnt);
	
	pthread_mutex_unlock(&pNode->p_mutex);
	return;
}

void Check_Consensus(void *param) {

	struct Processor* pNode = (struct Processor*)param;
	/*As per Byzantine Agreement: Impossibility condition -More than thwo thirds of lieutenants must be loyal*/

	printV("\n\t****************************************************************************************");
	printV("\n\t Total number of Lieutenant \t : %d", pNode->num_dsystems);
	printV("\n\t Total number of Traitors   \t : %d", gTraitorCount);
	printf("\n\t Majority Message           \t : %s", IntToStr(pNode->majority));
	if ((pNode->majority == UNKNOWN) || (pNode->majority == CONFUSE_PEER) /*|| (pNode->LtType == TRAITOR)*/) {
		printf("\n\n\t\t** (N<=3F) Byzatine Agreement Impossibility condition attained **");
		printf("\n\t\t** CONSENSUS CANNOT BE REACHED in Lieutenant%d **\n",pNode->proc_index);
		pNode->consensus = 0;
	}
	else {
		pNode->consensus = 1;
		printf("\n\n\t\t** CONSENSUS REACHED in Lieutenant%d **\n",pNode->proc_index);
	}
	printV("\n\t****************************************************************************************");
}

void *t_func(void *param) {

	int rc = 0, idx = 0;
	char tname[255];
	pthread_t pid;
	int done = 0;
	struct Processor* pNode = (struct Processor*)param;
	
	rc = pthread_mutex_lock(&a_mutex);
	if (rc) { /* an error has occurred */
		printf("\n pthread_mutex_lock \n");
	        pthread_exit(NULL);
	}

	sleep(2);

	pid = pthread_self();
	rc = pthread_getname_np(pid, tname, 255);

	printf("\n %s:: Enter --> \n", tname);

	if (pNode->proc_index == GENERAL_INDEX)
		SendMessage(pNode, gTraitorCount, pNode->proc_index);
	else
		SendMessage(pNode, gTraitorCount -1, pNode->proc_index);

	++_lieutenants_tx_processed;
	printV("\n tx count = %d",_lieutenants_tx_processed);
	if (_lieutenants_tx_processed >= pNode->num_dsystems) {
		for (idx=1; idx<=pNode->num_dsystems; idx++) {
			if (Processor_List[idx]->waiting == 1) {
				printV("\n Signal Lieutenant %d",idx);
				pthread_cond_signal(&Processor_List[idx]->p_cond);
			}
		}
	} else {
		pNode->waiting = 1;
		printV("\n Lieutenant %d blocked", pNode->proc_index);
		pthread_cond_wait(&pNode->p_cond,&a_mutex);
		printV("\n Lieutenant %d unblocked", pNode->proc_index);
		pNode->waiting = 0;

	}

	ProcessMessage(pNode);
	pNode->majority = FindMajorityMessage(pNode);	
	//printf("\n\t****************************************************************************************");
	//printf("\n\t Majority message recieved by Lieutenant '%d' is '%s' ",pNode->proc_index,IntToStr(pNode->majority));
	//printf("\n\t****************************************************************************************");
	Check_Consensus(pNode);

	printf("\n %s:: <--Exit  \n", tname);
	pthread_mutex_unlock(&a_mutex);

	pthread_exit(&rc);
	return NULL;
}

static void construct_input_graph(struct Processor* pNode[MAX_PROCESSORS], int num_dsystems) {
	
	int idx = 0;
	int rc = 0;
	char tname[255];
	int eType;

	printf("\nconstructing byzantine nodes ...\n");

	if (MAX_PROCESSORS <= num_dsystems) {
		printf("[ERROR] The system currently supports only %d Lieutenants. Please try with a value less than %d", MAX_PROCESSORS, MAX_PROCESSORS);
		return;
	}

	for (idx=1; idx<=num_dsystems; idx++) {

		/* Create Individual Processor Nodes (General and Lieutenants)*/
		pNode[idx] = (struct Processor*) malloc(sizeof(struct Processor));
		if(!pNode[idx]) {
			printf("\n [ERROR] Failed to allocate Processor - Exit");
			exit(1);
		}

		pNode[idx]->num_dsystems = num_dsystems;
		pNode[idx]->proc_index = idx;

enter_lt_type:
		if (idx == GENERAL_INDEX) {
			printf("\n\t Enter the General Type    (1-LOYAL, 2-TRAITOR)  \t : ");
			scanf("%d",&eType);
		} else {
			printf("\n\t Enter the Lieutenant Type (1-LOYAL, 2-TRAITOR)  \t : ");
			scanf("%d",&eType);
		}
		pNode[idx]->LtType = eType;
		if (pNode[idx]->LtType == TRAITOR)
			gTraitorCount++;
		//if (gTraitorCount > 1) {
		//	printf("\n\t Only one traitor allowed per session - Try again \n\n");
		//	goto enter_lt_type;
		//}

		if((pNode[idx]->LtType != TRAITOR) && (pNode[idx]->LtType != LOYAL)) {
			printf("\n\t Invalid Lieutenant Type - Try again !!\n\n");
			goto enter_lt_type;
		}
	}

	printf("\n\t Enter Message to be transmitted (1-ATTACK, 2-RETREAT, 3-COMPROMISED)\t : ");
	scanf("%d",&gMessage);

	if ((gMessage!= ATTACK) && (gMessage!=RETREAT) && (gMessage!=COMPROMISED)) {
		printf("\n\t Unrecognized input. UNKNOWN Messasge will be transmitted\n\n");
		gMessage = UNKNOWN;
	}	

	printf("\n\t ***ASSUMPTION:First Lieutenant(node) is always considered as General***");
	for (idx=1; idx<=num_dsystems; idx++) {

		if (pNode[idx]->LtType == TRAITOR)
			pNode[idx]->txmsg = CONFUSE_PEER;
		else
			pNode[idx]->txmsg = gMessage;

		/*create individual thread for every processor*/
		rc = pthread_create(&pNode[idx]->proc_thrd, NULL, t_func, pNode[idx]);
		if (rc) {
			printf("\n [ERROR] thread creation failed");
			exit(1);
		}

		if (idx == 1)
			sprintf(tname,"Gen%d",idx);
		else
		        sprintf(tname,"Lt%d",idx);
		printV("\n\t Distributed System '%d' name = '%s'",idx,tname);
		rc = pthread_setname_np(pNode[idx]->proc_thrd, tname);
		//sleep(2);

	}/*for*/

	printf("\n\n \t Please wait -- byzantine nodes are getting generated!! Dont press ENTER !! \n");
}

static void free_graph_nodes(struct Processor* pNode[MAX_PROCESSORS], int num_dsystems) {

	int idx = 0;
	int ret;
	
	for (idx=1; idx<=num_dsystems; idx++) {
		pthread_join(pNode[idx]->proc_thrd, NULL);
		free(pNode[idx]);
	}
	printf("\n  Lieutenant nodes Freed ..\n");
}

void print_final_decision()
{

	struct Processor* pNode;
	int idx, consensus_count=0;
	int num_dsystems = Processor_List[1]->num_dsystems;

	for (idx=1; idx<=num_dsystems; idx++) {
		pNode = Processor_List[idx];
		if (pNode->consensus) {
			++consensus_count;
		}
	}
	printf("\n\t***********************************************************************************************");
	printf("\n\n\t Total Consensus Count       = %d", consensus_count);
	printf("\n\n\t Total number of Lieutenants = %d", pNode->num_dsystems);
	printf("\n\n\t Total number of Traitors    = %d", gTraitorCount);
	if (consensus_count > (pNode->num_dsystems/2)) {

		printf("\n\n\tCONSENSUS CAN BE REACHED");
		printf("\n\t***********************************************************************************************");
	} else {
		printf("\n\n\tCONSENSUS IS NOT POSSIBLE - Two thirds of Lieutenants are not loyal");
		printf("\n\t***********************************************************************************************");
	}
	return;
}
int main()
{
	int num_lieutenants = 0;
	/* number of generals is assumed to be one always and all other lieutenants
	 * obey/ dis-obey to this generals command*/
	int num_general = 1;
	int num_dsystems = 0;

	printf("\n\n Byzantine General's Problem DEMO **START** \n\n");

	printf("\n\t First Lieutenant is considered to be General !!\n ");
	printf("\n\t Enter the additional number of Lieutenants needed (minus 1 for general) \t : ");
	scanf("%d",&num_lieutenants);

	/* total number of distributed systems under consideration*/
	num_dsystems = num_general + num_lieutenants;
	printf("\n\t******************************************************************" );
	printf("\n\t Total number of Lieutenants are (Lieutenants+General)\t %d",num_dsystems);
	printf("\n\t******************************************************************\n" );

	construct_input_graph(Processor_List, num_dsystems);

	getchar();

	free_graph_nodes(Processor_List, num_dsystems);
	
	printf("\n\n\n PRESS ENTER TO FIND OUT IF CONSENSUS CAN BE REACHED !! \n");
	getchar();
	print_final_decision();
	printf("\n\n\n PRESS ENTER TO EXIT \n");
	getchar();
	printf("\n\n\n Byzantine General's Problem DEMO **END** \n\n");
	return 0;
}

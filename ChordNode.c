#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <time.h>
#include <process.h> 

#define FNameMax 32       /* Max length of File Name */
#define FileMax  32 /* Max number of Files */
#define baseM 6 /* base number */ 
#define ringSize 64 /* ringSize = 2^baseM */
#define fBufSize 20000 /* file buffer size */
#define LENGTH 512
typedef struct {
	/* Node Info Type Structure */
	int ID;                 /* ID */
	struct sockaddr_in addrInfo;  /* Socket address */
} nodeInfoType;

typedef struct {
	/* File Type Structure */
	char Name[FNameMax]; /* File Name */
	int  Key; /* File Key */
	nodeInfoType owner; /* Owner's Node */
	nodeInfoType refOwner; /* Ref Owner's Node */
} fileRefType;

typedef struct {
	/* Global Information of Current Files */
	unsigned int fileNum;  /* Number of files */
	fileRefType  fileRef[FileMax]; /* The Number of Current Files */
} fileInfoType;

typedef struct {
	/* Finger Table Structure */
	nodeInfoType Pre;   /* Predecessor pointer */
	nodeInfoType finger[baseM]; /* Fingers (array of pointers) */
} fingerInfoType;

typedef struct {
	/* Chord Information Structure */
	fileInfoType   FRefInfo; /* File Ref Own Information */
	fingerInfoType fingerInfo;  /* Finger Table Information */
} chordInfoType;

typedef struct {
	/* Node Structure */
	nodeInfoType  nodeInfo;   /* Node's IPv4 Address */
	fileInfoType  fileInfo;   /* File Own Information */
	chordInfoType chordInfo;  /* Chord Data Information */
} nodeType;

typedef struct {
	unsigned short msgID;   // message ID 
	unsigned short msgType; // message type (0: request, 1: response) 
	nodeInfoType   nodeInfo;// node address info 
	short       moreInfo; // more info 
	fileRefType    fileInfo; // file (reference) info
	unsigned int   bodySize; // body size in Bytes
} chordHeaderType;           // CHORD message header type



unsigned int str_hash(const char*);
void printmenu();
int modIn(int modN, int targNum, int range1, int range2, int leftmode, int rightmode);
int twoPow(int power);

int modMinus(int modN, int minuend, int subtrand);

int modPlus(int modN, int addend1, int addend2);
void fileSender(void*);
void fileReceiver(void*);
void procRecvMsg(void*);
void procPP(void*);
void procFF(void*);


nodeType myNode = { 0 };  // node information -> global variable 
SOCKET rqSock, rpSock, flSock, frSock, fsSock, ffSock, ppSock;       // when multiplexing is used HANDLE hMutex;

int exitFlag = 0;
HANDLE hMutex;
HANDLE hThread[3];

int mute = 0;

char filename[FNameMax];
int main(int argc, char* argv[])
{
	WSADATA wsaData;
	int i;
	int addrlen = sizeof(struct sockaddr);
	int createOK = 0;	//create Ȯ��
	int joinOK = 0;		//join Ȯ��
	char select;		//�Է��� �޴��� ������ ����
	int optVal = 5000;  // 5 seconds
	char Sock_addr[21] = { 0 };//�ڽ� ������ ������ ������ ���ڿ� �迭
	char helpIP[16] = { 0 };	//��������� IP�� ������ ���ڿ� �迭
	int helpPort;				//��������� ��Ʈ��ȣ
	int fkey;					//������ Ű���� ������ ����
	int retVal; //���� ���� ����

	chordHeaderType helpMsg, resMsg;
	nodeInfoType succ, pred, helper, refOwner;

	helpMsg.nodeInfo.addrInfo.sin_family = AF_INET;
	resMsg.nodeInfo.addrInfo.sin_family = AF_INET;


	printf("*****************************************************************\n");
	printf("*         DHT-Based P2P Protocol (CHORD) Node Controller        *\n");
	printf("*                   Ver. 0.1     Nov. 5, 2018                   *\n");
	printf("*               (c) * Lee Ki jong *\n");
	printf("*****************************************************************\n");

	myNode.nodeInfo.addrInfo.sin_family = AF_INET;

	if (argc != 3) {
		printf("[ERROR] Usage : %s <IP> <port>\n", argv[0]);
		printf("���α׷��� �����մϴ�.");
		exit(1);
	}

	if (inet_addr(argv[1]) != INADDR_NONE)
		myNode.nodeInfo.addrInfo.sin_addr.s_addr = inet_addr(argv[1]);
	else {
		printf("[ERROR] IP �ּҸ� �߸� �Է��Ͽ����ϴ�.");
		printf("���α׷��� �����մϴ�.");
		exit(1);
	}



	if (atoi(argv[2]) > 65535 || atoi(argv[2]) < 49152) {
		printf("[ERROR] ��Ʈ ��ȣ�� 49152 <= port <= 65535 �� �����ּ���.\n");
		printf("���α׷��� �����մϴ�.");
		exit(1);
	}
	else
		myNode.nodeInfo.addrInfo.sin_port = htons(atoi(argv[2]));

	strcpy(Sock_addr, argv[2]);
	strcat(Sock_addr, argv[1]);
	myNode.nodeInfo.ID = str_hash(Sock_addr);

	printf(">>> Welcome to ChordNode Program!\n");

	printf(">>> Your IP address: %s, Port No: %s, ID: %d\n", argv[1], argv[2], myNode.nodeInfo.ID);

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		printf("\nCHORD> �ʱ�ȭ�� �����Ͽ����ϴ�.\n\n");


	while (1) {
		do {
			printmenu();

			scanf("%s", &select);
			switch (select) {

			case 'c':

				if (createOK) {
					printf("\n[ERROR] �̹� ��Ʈ��ũ�� �����Ͽ����ϴ�.\n");
					printf("[ERROR] �ٽ� ������ �ּ���.\n\n");
					continue;
				}

				myNode.chordInfo.fingerInfo.Pre = myNode.nodeInfo;

				for (i = 0; i < baseM; i++)
					myNode.chordInfo.fingerInfo.finger[i] = myNode.nodeInfo;

				myNode.fileInfo.fileNum = 0;
				myNode.chordInfo.FRefInfo.fileNum = 0;

				rqSock = socket(AF_INET, SOCK_DGRAM, 0);

				retVal = setsockopt(rqSock, SOL_SOCKET, SO_RCVTIMEO, (char*)& optVal, sizeof(optVal));

				if (retVal == SOCKET_ERROR) {
					printf("\n[ERROR] rqSock �ɼ� ���� �� ������ ������ϴ�.\n");
					printf("[ERROR] ���α׷��� �����մϴ�.\n\n");
					exit(1);
				}

				rpSock = socket(AF_INET, SOCK_DGRAM, 0);

				if (bind(rpSock, (struct sockaddr*) & myNode.nodeInfo.addrInfo, sizeof(myNode.nodeInfo.addrInfo)) < 0) {
					printf("\n[ERROR] rpSock �ּ� ���ε� �� ������ ������ϴ�.\n");
					printf("[ERROR] ���α׷��� �����մϴ�.\n\n");
					exit(1);
				}

				ffSock = socket(AF_INET, SOCK_DGRAM, 0);

				retVal = setsockopt(ffSock, SOL_SOCKET, SO_RCVTIMEO, (char*)& optVal, sizeof(optVal));

				if (retVal == SOCKET_ERROR) {
					printf("\n[ERROR] rqSock �ɼ� ���� �� ������ ������ϴ�.\n");
					printf("[ERROR] ���α׷��� �����մϴ�.\n\n");
					exit(1);
				}

				ppSock = socket(AF_INET, SOCK_DGRAM, 0);

				retVal = setsockopt(ppSock, SOL_SOCKET, SO_RCVTIMEO, (char*)& optVal, sizeof(optVal));

				if (retVal == SOCKET_ERROR) {
					printf("\n[ERROR] rqSock �ɼ� ���� �� ������ ������ϴ�.\n");
					printf("[ERROR] ���α׷��� �����մϴ�.\n\n");
					exit(1);
				}

				createOK = 1;
				printf("\nCHORD> �ڵ� ��Ʈ��ũ�� �����Ͽ����ϴ�.\n\n");

				memset(&helpMsg, 0, sizeof(helpMsg));
				memset(&resMsg, 0, sizeof(resMsg));

				hThread[0] = (HANDLE)_beginthreadex(NULL, 0, (void*)procRecvMsg, (void*)& exitFlag, 0, NULL);
				hThread[1] = (HANDLE)_beginthreadex(NULL, 0, (void*)procPP, (void*)& exitFlag, 0, NULL);
				hThread[2] = (HANDLE)_beginthreadex(NULL, 0, (void*)procFF, (void*)& exitFlag, 0, NULL);

				break;


			case 'j':


				if (joinOK) {
					printf("\n[ERROR] �̹� ��Ʈ��ũ ���� �ֽ��ϴ�. ���� �� �����ϴ�.");
					printf("\n[ERROR] �ٽ� ������ �ּ���.\n");
					continue;
				}

				myNode.chordInfo.fingerInfo.Pre.ID = -1;
				for (i = 0; i < baseM; i++)
					myNode.chordInfo.fingerInfo.finger[i].ID = -1;

				myNode.fileInfo.fileNum = 0;
				myNode.chordInfo.FRefInfo.fileNum = 0;



				memset(&helper, 0, sizeof(helper));

				helper.addrInfo.sin_family = AF_INET;

				while (1) {
					printf("\nCHORD> ���� ����� IP �ּҸ� �Է��ϼ��� : ");
					memset(helpIP, 0, 16);
					scanf("%s", helpIP);
					fflush(stdin);
					if (inet_addr(helpIP) != INADDR_NONE) {
						helper.addrInfo.sin_addr.s_addr = inet_addr(helpIP);
						break;
					}
					else {
						printf("\n[ERROR] IP �ּҸ� �߸� �Է��Ͽ����ϴ�.\n");
						continue;
					}
				}

				while (1) {
					printf("\nCHORD> ���� ����� Port ��ȣ�� �Է��ϼ��� : ");
					scanf("%d", &helpPort);
					fflush(stdin);
					if (helpPort > 65535 || helpPort < 49152) {
						printf("[ERROR] ��Ʈ ��ȣ�� 49152 <= port <= 65535 �� �����ּ���.\n");
						continue;
					}
					else {
						helper.addrInfo.sin_port = htons(helpPort);
						break;
					}
				}


				rqSock = socket(AF_INET, SOCK_DGRAM, 0);

				retVal = setsockopt(rqSock, SOL_SOCKET, SO_RCVTIMEO, (char*)& optVal, sizeof(optVal));

				if (retVal == SOCKET_ERROR) {
					printf("\n[ERROR] rqSock �ɼ� ���� �� ������ ������ϴ�.\n");
					printf("[ERROR] ���α׷��� �����մϴ�.\n\n");
					exit(1);
				}

				rpSock = socket(AF_INET, SOCK_DGRAM, 0);

				if (bind(rpSock, (struct sockaddr*) & myNode.nodeInfo.addrInfo, sizeof(myNode.nodeInfo.addrInfo)) < 0) {
					printf("\n[ERROR] rpSock �ּ� ���ε� �� ������ ������ϴ�.\n");
					printf("[ERROR] ���α׷��� �����մϴ�.\n\n");
					exit(1);
				}

				ffSock = socket(AF_INET, SOCK_DGRAM, 0);

				retVal = setsockopt(ffSock, SOL_SOCKET, SO_RCVTIMEO, (char*)& optVal, sizeof(optVal));

				if (retVal == SOCKET_ERROR) {
					printf("\n[ERROR] ffSock �ɼ� ���� �� ������ ������ϴ�.\n");
					printf("[ERROR] ���α׷��� �����մϴ�.\n\n");
					exit(1);
				}

				ppSock = socket(AF_INET, SOCK_DGRAM, 0);

				retVal = setsockopt(ppSock, SOL_SOCKET, SO_RCVTIMEO, (char*)& optVal, sizeof(optVal));

				if (retVal == SOCKET_ERROR) {
					printf("\n[ERROR] ppSock �ɼ� ���� �� ������ ������ϴ�.\n");
					printf("[ERROR] ���α׷��� �����մϴ�.\n\n");
					exit(1);
				}

				//Join Info
				helpMsg.msgID = 1;
				helpMsg.msgType = 0;
				helpMsg.nodeInfo = myNode.nodeInfo;
				helpMsg.moreInfo = 0;
				helpMsg.bodySize = 0;
				sendto(rqSock, (char*)& helpMsg, sizeof(helpMsg), 0,
					(struct sockaddr*) & helper.addrInfo, sizeof(helper.addrInfo));

				retVal = -1;
				while (1) {

					retVal = recvfrom(rqSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & helper.addrInfo, &addrlen);
					if (retVal >= 0)
						break;
				}

				succ = resMsg.nodeInfo;
				myNode.chordInfo.fingerInfo.finger[0] = succ;

				printf("\nCHORD> SUCC = IP : %s, Port : %d, ID : %d\n", inet_ntoa(succ.addrInfo.sin_addr), ntohs(succ.addrInfo.sin_port), succ.ID);




				//moveKeys
				memset(&helpMsg, 0, sizeof(helpMsg));
				helpMsg.msgID = 2;
				helpMsg.msgType = 0;
				helpMsg.nodeInfo = myNode.nodeInfo;
				helpMsg.moreInfo = 0;
				helpMsg.bodySize = 0;

				sendto(rqSock, (char*)& helpMsg, sizeof(helpMsg), 0,
					(struct sockaddr*) & myNode.chordInfo.fingerInfo.finger[0].addrInfo, sizeof(myNode.chordInfo.fingerInfo.finger[0].addrInfo));

				memset(&resMsg, 0, sizeof(resMsg));

				retVal = -1;
				while (1) {

					retVal = recvfrom(rqSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & myNode.chordInfo.fingerInfo.finger[0].addrInfo, &addrlen);
					if (retVal >= 0)
						break;
				}

				int filecount = 0;
				filecount = resMsg.moreInfo;
				printf("\nCHORD> You got %d keys from your succNode", filecount);
				if (filecount > 0) {
					for (i = 0; i < filecount; i++)
					{
						retVal = -1;
						while (1) {

							retVal = recvfrom(rqSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & myNode.chordInfo.fingerInfo.finger[0].addrInfo, &addrlen);
							if (retVal >= 0)
								break;
						}

						myNode.chordInfo.FRefInfo.fileRef[myNode.chordInfo.FRefInfo.fileNum] = resMsg.fileInfo;
						myNode.chordInfo.FRefInfo.fileRef[myNode.chordInfo.FRefInfo.fileNum++].refOwner = myNode.nodeInfo;

					}
				}



				//stabilize

				memset(&helpMsg, 0, sizeof(helpMsg));
				helpMsg.msgID = 3;
				helpMsg.msgType = 0;
				helpMsg.nodeInfo = myNode.nodeInfo;
				helpMsg.moreInfo = 0;
				helpMsg.bodySize = 0;



				sendto(rqSock, (char*)& helpMsg, sizeof(helpMsg), 0,
					(struct sockaddr*) & myNode.chordInfo.fingerInfo.finger[0].addrInfo, sizeof(myNode.chordInfo.fingerInfo.finger[0].addrInfo));

				memset(&resMsg, 0, sizeof(resMsg));
				retVal = -1;
				while (1) {
					retVal = recvfrom(rqSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & myNode.chordInfo.fingerInfo.finger[0].addrInfo, &addrlen);
					if (retVal >= 0)
						break;
				}
				myNode.chordInfo.fingerInfo.Pre = resMsg.nodeInfo;

				printf("\nCHORD> PRED = IP : %s, Port : %d, ID : %d\n", inet_ntoa(myNode.chordInfo.fingerInfo.Pre.addrInfo.sin_addr), ntohs(myNode.chordInfo.fingerInfo.Pre.addrInfo.sin_port), myNode.chordInfo.fingerInfo.Pre.ID);


				memset(&helpMsg, 0, sizeof(helpMsg));
				helpMsg.msgID = 6;
				helpMsg.msgType = 0;
				helpMsg.nodeInfo = myNode.nodeInfo;
				helpMsg.moreInfo = 0;
				helpMsg.bodySize = 0;

				sendto(rqSock, (char*)& helpMsg, sizeof(helpMsg), 0,
					(struct sockaddr*) & myNode.chordInfo.fingerInfo.Pre.addrInfo, sizeof(myNode.chordInfo.fingerInfo.Pre.addrInfo));

				memset(&resMsg, 0, sizeof(resMsg));
				retVal = -1;
				while (1) {
					retVal = recvfrom(rqSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & myNode.chordInfo.fingerInfo.Pre.addrInfo, &addrlen);
					if (retVal >= 0)
						break;
				}
				if (resMsg.moreInfo == 1)
				{
					printf("\nCHORD> Your Pred's Succ has been updated as your Node\n");
				}
				memset(&helpMsg, 0, sizeof(helpMsg));
				helpMsg.msgID = 4;
				helpMsg.msgType = 0;
				helpMsg.nodeInfo = myNode.nodeInfo;
				helpMsg.moreInfo = 0;
				helpMsg.bodySize = 0;

				sendto(rqSock, (char*)& helpMsg, sizeof(helpMsg), 0,
					(struct sockaddr*) & myNode.chordInfo.fingerInfo.finger[0].addrInfo, sizeof(myNode.chordInfo.fingerInfo.finger[0].addrInfo));

				memset(&resMsg, 0, sizeof(resMsg));
				retVal = -1;
				while (1) {

					retVal = recvfrom(rqSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & myNode.chordInfo.fingerInfo.finger[0].addrInfo, &addrlen);
					if (retVal >= 0)
						break;
				}
				if (resMsg.moreInfo == 1)
				{
					printf("\nCHORD> Your Succ's Pred has been updated as your Node\n");
				}
				memset(&helpMsg, 0, sizeof(helpMsg));
				memset(&resMsg, 0, sizeof(resMsg));

				printf("CHORD> Node Join Success!!\n");

				hThread[0] = (HANDLE)_beginthreadex(NULL, 0, (void*)procRecvMsg, (void*)& exitFlag, 0, NULL);

				hThread[1] = (HANDLE)_beginthreadex(NULL, 0, (void*)procPP, (void*)& exitFlag, 0, NULL);

				hThread[2] = (HANDLE)_beginthreadex(NULL, 0, (void*)procFF, (void*)& exitFlag, 0, NULL);

				joinOK = 1;
				break;

			case 'l':

				exitFlag = 1;

				WaitForMultipleObjects(3, hThread, TRUE, INFINITE);

				break;

			case 'a':

				printf("CHORD> �߰��� ������ ���α׷��� ���� ���� �ȿ� ��ġ�Ͽ��� �մϴ�.\n");
				printf("CHORD> ���� �̸��� �ִ� ũ��� 32�Դϴ�.\n");
				printf("CHORD> �߰��� ���ϸ��� �Է��ϼ��� : ");
				scanf("%s", filename);
				fkey = str_hash(filename);
				printf("CHORD> ����� ���ϸ� : %s, Key: %d\n", filename, fkey);

				FILE* fd;

				if ((fd = fopen(filename, "rt")) == NULL) {
					printf("[ERROR] %s��� �̸��� ������ �������� �ʽ��ϴ�.\n", filename);
					break;
				}

				fclose(fd);

				for (i = 0; i < myNode.fileInfo.fileNum; i++) {
					if (myNode.fileInfo.fileRef[i].Key == fkey) {
						printf("[ERROR] %s ������ �̹� ��� �Ǿ����ϴ�.", filename);
						break;

					}
				}

				if (myNode.nodeInfo.ID == myNode.chordInfo.fingerInfo.Pre.ID) {	//��尡 �ϳ����
					myNode.fileInfo.fileRef[myNode.fileInfo.fileNum].Key = fkey;//��忡 ������ Ű �� �Է�
					strcpy(myNode.fileInfo.fileRef[myNode.fileInfo.fileNum].Name, filename);//��忡 ���� �̸� �Է�
					myNode.fileInfo.fileRef[myNode.fileInfo.fileNum].owner = myNode.nodeInfo;//��忡 ������ �����ָ� �ڽ����� �Է�
					myNode.fileInfo.fileRef[myNode.fileInfo.fileNum].refOwner = myNode.nodeInfo;//��忡 ������ �����ָ� �ڽ����� �Է�
					myNode.fileInfo.fileNum++;//��尡 ������ �ִ� ���� �� ������Ŵ

					myNode.chordInfo.FRefInfo.fileRef[myNode.chordInfo.FRefInfo.fileNum]
						= myNode.fileInfo.fileRef[myNode.fileInfo.fileNum - 1];
					myNode.chordInfo.FRefInfo.fileNum++;

				}
				else {
					nodeInfoType keyfile;
					keyfile.ID = fkey;
					helper = myNode.chordInfo.fingerInfo.Pre;

					helper = myNode.chordInfo.fingerInfo.finger[0];

					helpMsg.msgID = 7;
					helpMsg.msgType = 0;
					helpMsg.nodeInfo = keyfile;
					helpMsg.moreInfo = 0;
					helpMsg.bodySize = 0;

					sendto(rqSock, (char*)& helpMsg, sizeof(helpMsg), 0,
						(struct sockaddr*) & helper.addrInfo, sizeof(helper.addrInfo));
					memset(&resMsg, 0, sizeof(resMsg));
					retVal = -1;
					while (1) {
						retVal = recvfrom(rqSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & helper.addrInfo, &addrlen);
						if (retVal >= 0)
							break;

					}

					pred = resMsg.nodeInfo;
					if (resMsg.moreInfo)
						printf("CHORD> File�� Pred IP�ּ� : %s, Port ��ȣ : %d, ID : %d\n", inet_ntoa(pred.addrInfo.sin_addr), ntohs(pred.addrInfo.sin_port), pred.ID);


					helpMsg.msgID = 5;
					helpMsg.msgType = 0;
					helpMsg.nodeInfo = myNode.nodeInfo;
					helpMsg.moreInfo = 0;
					helpMsg.bodySize = 0;

					sendto(rqSock, (char*)& helpMsg, sizeof(helpMsg), 0,
						(struct sockaddr*) & pred.addrInfo, sizeof(pred.addrInfo));
					memset(&resMsg, 0, sizeof(resMsg));
					retVal = -1;
					while (1) {
						retVal = recvfrom(rqSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & pred.addrInfo, &addrlen);
						if (retVal >= 0)
							break;

					}


					//fkey�� Successor
					succ = resMsg.nodeInfo;

					if (resMsg.moreInfo)
						printf("CHORD> File�� Successor IP�ּ� : %s, Port ��ȣ : %d, ID : %d\n", inet_ntoa(succ.addrInfo.sin_addr), ntohs(succ.addrInfo.sin_port), succ.ID);

					//�ߺ� Ȯ��
					memset(&helpMsg, 0, sizeof(helpMsg));
					helpMsg.msgID = 11;
					helpMsg.msgType = 0;
					helpMsg.nodeInfo = myNode.nodeInfo;
					helpMsg.fileInfo.Key = fkey;
					helpMsg.moreInfo = 0;
					helpMsg.bodySize = 0;


					sendto(rqSock, (char*)& helpMsg, sizeof(helpMsg), 0,
						(struct sockaddr*) & succ.addrInfo, sizeof(succ.addrInfo));
					memset(&resMsg, 0, sizeof(resMsg));
					retVal = -1;
					while (1) {
						retVal = recvfrom(rqSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & succ.addrInfo, &addrlen);
						if (retVal >= 0)
							break;
					}

					if (resMsg.moreInfo) {
						printf("[ERROR] %s ������ �̹� ��� �Ǿ����ϴ�.", filename);
						break;
					}

					myNode.fileInfo.fileRef[myNode.fileInfo.fileNum].Key = fkey;//��忡 ������ Ű �� �Է�
					strcpy(myNode.fileInfo.fileRef[myNode.fileInfo.fileNum].Name, filename);//��忡 ���� �̸� �Է�
					myNode.fileInfo.fileRef[myNode.fileInfo.fileNum].owner = myNode.nodeInfo;//��忡 ������ �����ָ� �ڽ����� �Է�
					myNode.fileInfo.fileRef[myNode.fileInfo.fileNum].refOwner = succ;//��忡 ������ �����ָ� �ڽ����� �Է�
					myNode.fileInfo.fileNum++;//��尡 ������ �ִ� ���� �� ������Ŵ

											  //key�� Successor���� ������ �������� �Է¿�û
											  //File Reference Add Request
					memset(&helpMsg, 0, sizeof(helpMsg));
					helpMsg.msgID = 8;
					helpMsg.msgType = 0;
					helpMsg.nodeInfo = myNode.nodeInfo;
					helpMsg.fileInfo = myNode.fileInfo.fileRef[myNode.fileInfo.fileNum - 1];
					helpMsg.moreInfo = 0;
					helpMsg.bodySize = 0;

					sendto(rqSock, (char*)& helpMsg, sizeof(helpMsg), 0,
						(struct sockaddr*) & succ.addrInfo, sizeof(succ.addrInfo));
					memset(&resMsg, 0, sizeof(resMsg));
					retVal = -1;
					while (1) {
						retVal = recvfrom(rqSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & succ.addrInfo, &addrlen);
						if (retVal >= 0)
							break;
					}
					if (!mute)
						printf("CHORD> FILE REF ADD RESPONSE MSG HAS BEEN RECEIVED!\n");
					if (resMsg.moreInfo == 1) {
						printf("CHORD> ���� ���������� Successor���� ���������� ���������ϴ�.\n");
					}
				}
				printf("\nCHORD> ���� �߰� �Ϸ�!!\n");
				break;

			case 'd':
				printf("CHORD> ������ ���ϸ��� �Է��ϼ��� : ");
				scanf("%s", filename);

				fkey = str_hash(filename);

				//��尡 �ϳ��� ��
				if (myNode.nodeInfo.ID == myNode.chordInfo.fingerInfo.finger[0].ID)
				{
					if (myNode.fileInfo.fileNum == 0)
					{
						printf("[ERROR] file(%s)�� �������� �ʽ��ϴ�.\n", filename);
					}
					for (i = 0; i < myNode.fileInfo.fileNum; i++)
					{
						if (myNode.fileInfo.fileRef[i].Key == fkey)
						{
							myNode.fileInfo.fileNum--;
							myNode.chordInfo.FRefInfo.fileNum--;
							break;
						}
						if (i == myNode.fileInfo.fileNum - 1)
						{
							printf("[ERROR] file(%s)�� �������� �ʽ��ϴ�.\n", filename);
						}
					}

				}
				else
				{
					int mycheck = 0;		//�ڽ��� ������ �����ߴ��� üũ
					int fingercheck = 0;	//�ڽ��� �ΰ� ��尡 ������ �����ߴ��� üũ
					for (i = 0; i < myNode.fileInfo.fileNum; i++)
					{
						if (myNode.fileInfo.fileRef[i].Key == fkey)
						{
							mycheck = 1;
							refOwner = myNode.fileInfo.fileRef[i].refOwner;

							//File Reference Delete Request					
							memset(&helpMsg, 0, sizeof(helpMsg));
							helpMsg.msgID = 9;
							helpMsg.msgType = 0;
							helpMsg.fileInfo.Key = fkey;
							helpMsg.moreInfo = 0;
							helpMsg.bodySize = 0;
							sendto(rqSock, (char*)& helpMsg, sizeof(helpMsg), 0,
								(struct sockaddr*) & refOwner.addrInfo, sizeof(refOwner.addrInfo));
							//File Reference Delete Response				
							memset(&resMsg, 0, sizeof(resMsg));
							retVal = -1;
							while (1) {
								retVal = recvfrom(rqSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & refOwner.addrInfo, &addrlen);
								if (retVal >= 0)
									break;

							}

							if (resMsg.moreInfo)
								printf("CHORD> Owner's FILE %s has been deleted!\n", filename);
							//myNode�� ���� ����	
							for (i = 0; i < myNode.fileInfo.fileNum; i++) {
								if (myNode.fileInfo.fileRef[i].Key == fkey) {
									for (int j = i; j < myNode.fileInfo.fileNum - 1; j++) {
										myNode.fileInfo.fileRef[j] = myNode.fileInfo.fileRef[j + 1];
									}
								}
							}
							myNode.fileInfo.fileNum--;

							break;
						}
					}
					if (mycheck) {
						printf("CHORD> file(%s) ���� �Ϸ�\n", filename);
						break;
					}
					for (i = 0; i < myNode.chordInfo.FRefInfo.fileNum; i++)
					{
						if (myNode.chordInfo.FRefInfo.fileRef[i].Key == fkey)
						{
							fingercheck = 1;
							printf("[ERROR] file(%s)�� N%d�� �ֽ��ϴ�.\n", filename, myNode.chordInfo.FRefInfo.fileRef[i].owner.ID);
							printf("[ERROR] ������ �����ָ� ������ �� �ֽ��ϴ�.\n");
							break;
						}
					}
					if (fingercheck)
						break;

					helper = myNode.chordInfo.fingerInfo.finger[0];

					helpMsg.msgID = 7;
					helpMsg.msgType = 0;
					helpMsg.nodeInfo.ID = fkey;
					helpMsg.moreInfo = 0;
					helpMsg.bodySize = 0;

					sendto(rqSock, (char*)& helpMsg, sizeof(helpMsg), 0,
						(struct sockaddr*) & helper.addrInfo, sizeof(helper.addrInfo));
					memset(&resMsg, 0, sizeof(resMsg));
					retVal = -1;
					while (1) {
						retVal = recvfrom(rqSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & helper.addrInfo, &addrlen);
						if (retVal >= 0)
							break;

					}
					pred = resMsg.nodeInfo;
					if (resMsg.moreInfo)
						printf("CHORD> File�� Pred IP�ּ� : %s, Port ��ȣ : %d, ID : %d\n", inet_ntoa(pred.addrInfo.sin_addr), ntohs(pred.addrInfo.sin_port), pred.ID);

					helpMsg.msgID = 5;
					helpMsg.msgType = 0;
					helpMsg.nodeInfo = myNode.nodeInfo;
					helpMsg.moreInfo = 0;
					helpMsg.bodySize = 0;

					sendto(rqSock, (char*)& helpMsg, sizeof(helpMsg), 0,
						(struct sockaddr*) & pred.addrInfo, sizeof(pred.addrInfo));
					memset(&resMsg, 0, sizeof(resMsg));
					retVal = -1;
					while (1) {
						retVal = recvfrom(rqSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & pred.addrInfo, &addrlen);
						if (retVal >= 0)
							break;

					}
					refOwner = resMsg.nodeInfo;
					if (resMsg.moreInfo)
						printf("CHORD> File�� refOwner IP�ּ� : %s, Port ��ȣ : %d, ID : %d\n", inet_ntoa(refOwner.addrInfo.sin_addr), ntohs(refOwner.addrInfo.sin_port), refOwner.ID);

					helpMsg.msgID = 11;
					helpMsg.msgType = 0;
					helpMsg.nodeInfo = myNode.nodeInfo;
					helpMsg.fileInfo.Key = fkey;
					helpMsg.moreInfo = 0;
					helpMsg.bodySize = 0;

					sendto(rqSock, (char*)& helpMsg, sizeof(helpMsg), 0,
						(struct sockaddr*) & refOwner.addrInfo, sizeof(refOwner.addrInfo));
					memset(&resMsg, 0, sizeof(resMsg));
					retVal = -1;
					while (1) {
						retVal = recvfrom(rqSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & refOwner.addrInfo, &addrlen);
						if (retVal >= 0)
							break;

					}
					if (resMsg.moreInfo) {
						printf("[ERROR] file(%s)�� N%d�� �ֽ��ϴ�.\n", filename, resMsg.fileInfo.owner.ID);
						printf("[ERROR] ������ �����ָ� ������ �� �ֽ��ϴ�.\n");
					}
					else
						printf("[ERROR] file(%s)�� �������� �ʽ��ϴ�.\n", filename);

				}

				break;

			case 's':
				printf("CHORD> �ٿ�ε��� ���ϸ��� �Է��ϼ��� : ");
				scanf("%s", filename);
				fkey = str_hash(filename);

				nodeInfoType Owner;

				if (myNode.nodeInfo.ID == myNode.chordInfo.fingerInfo.finger[0].ID)
				{
					if (myNode.fileInfo.fileNum == 0)
					{
						printf("[ERROR] file(%s)�� �������� �ʽ��ϴ�.\n", filename);
					}
					for (i = 0; i < myNode.fileInfo.fileNum; i++)
					{
						if (myNode.fileInfo.fileRef[i].Key == fkey)
						{
							printf("[ERROR] file(%s)�� �̹� ������ �ֽ��ϴ�.\n", filename);
							break;
						}
						if (i == myNode.fileInfo.fileNum - 1)
						{
							printf("[ERROR] file(%s)�� �������� �ʽ��ϴ�.\n", filename);
						}
					}

				}
				else
				{
					int mycheck = 0;
					int fingercheck = 0;
					for (i = 0; i < myNode.fileInfo.fileNum; i++)
					{
						if (myNode.fileInfo.fileRef[i].Key == fkey)
						{
							mycheck = 1;
							printf("[ERROR] file(%s)�� �̹� ������ �ֽ��ϴ�.\n", filename);
							break;
						}
					}
					if (mycheck)
						break;

					for (i = 0; i < myNode.chordInfo.FRefInfo.fileNum; i++)
					{
						if (myNode.chordInfo.FRefInfo.fileRef[i].Key == fkey)
						{
							Owner = myNode.chordInfo.FRefInfo.fileRef[i].owner;
							fingercheck = 1;
							break;
						}
					}
					if (!fingercheck) {

						helper = myNode.chordInfo.fingerInfo.finger[0];
						helpMsg.msgID = 7;
						helpMsg.msgType = 0;
						helpMsg.nodeInfo.ID = fkey;
						helpMsg.moreInfo = 0;
						helpMsg.bodySize = 0;

						sendto(rqSock, (char*)& helpMsg, sizeof(helpMsg), 0,
							(struct sockaddr*) & helper.addrInfo, sizeof(helper.addrInfo));
						memset(&resMsg, 0, sizeof(resMsg));
						retVal = -1;
						while (1) {
							retVal = recvfrom(rqSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & helper.addrInfo, &addrlen);
							if (retVal >= 0)
								break;

						}
						pred = resMsg.nodeInfo;
						if (resMsg.moreInfo)
							printf("CHORD> File�� Pred IP�ּ� : %s, Port ��ȣ : %d, ID : %d\n", inet_ntoa(pred.addrInfo.sin_addr), ntohs(pred.addrInfo.sin_port), pred.ID);

						helpMsg.msgID = 5;
						helpMsg.msgType = 0;
						helpMsg.nodeInfo = myNode.nodeInfo;
						helpMsg.moreInfo = 0;
						helpMsg.bodySize = 0;

						sendto(rqSock, (char*)& helpMsg, sizeof(helpMsg), 0,
							(struct sockaddr*) & pred.addrInfo, sizeof(pred.addrInfo));
						memset(&resMsg, 0, sizeof(resMsg));
						retVal = -1;
						while (1) {
							retVal = recvfrom(rqSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & pred.addrInfo, &addrlen);
							if (retVal >= 0)
								break;

						}
						refOwner = resMsg.nodeInfo;
						printf("CHORD> File�� refOwner IP�ּ� : %s, Port ��ȣ : %d, ID : %d\n", inet_ntoa(refOwner.addrInfo.sin_addr), ntohs(refOwner.addrInfo.sin_port), refOwner.ID);

						helpMsg.msgID = 11;
						helpMsg.msgType = 0;
						helpMsg.nodeInfo = myNode.nodeInfo;
						helpMsg.fileInfo.Key = fkey;
						helpMsg.moreInfo = 0;
						helpMsg.bodySize = 0;

						sendto(rqSock, (char*)& helpMsg, sizeof(helpMsg), 0,
							(struct sockaddr*) & refOwner.addrInfo, sizeof(refOwner.addrInfo));
						memset(&resMsg, 0, sizeof(resMsg));
						retVal = -1;
						while (1) {
							retVal = recvfrom(rqSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & refOwner.addrInfo, &addrlen);

							if (retVal >= 0)
								break;

						}
						if (resMsg.moreInfo == 1) {
							Owner = resMsg.fileInfo.owner;
						}
						else {
							fingercheck = 2;
							printf("[ERROR] file(%s)�� �������� �ʽ��ϴ�.\n", filename);
						}

					}
					if (fingercheck == 2)
						break;

					helpMsg.msgID = 10;
					helpMsg.msgType = 0;
					helpMsg.nodeInfo = myNode.nodeInfo;
					helpMsg.fileInfo.Key = fkey;
					helpMsg.moreInfo = 0;
					helpMsg.bodySize = 0;
					sendto(rqSock, (char*)& helpMsg, sizeof(helpMsg), 0,
						(struct sockaddr*) & Owner.addrInfo, sizeof(Owner.addrInfo));
					if (!mute)
						printf("\nCHORD> FILE DOWN REQUEST MSG HAS BEEN SENT!\n");
					flSock = socket(AF_INET, SOCK_STREAM, 0); // for accepting file down request 
					if (bind(flSock, (struct sockaddr*) & myNode.nodeInfo.addrInfo, sizeof(myNode.nodeInfo.addrInfo)) == SOCKET_ERROR) {
						printf("\n[ERROR] flSock ���ε� �� ������ ������ϴ�.\n");
						printf("[ERROR] ���α׷��� �����մϴ�.\n\n");
						exit(1);
					}

					listen(flSock, SOMAXCONN);
					if (flSock == SOCKET_ERROR) {
						printf("\n[ERROR] flSock ���� �� ������ ������ϴ�.\n");
						printf("[ERROR] ���α׷��� �����մϴ�.\n\n");
						exit(1);
					}


					memset(&resMsg, -1, sizeof(resMsg));
					retVal = -1;
					while (1) {
						retVal = recvfrom(rqSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & Owner.addrInfo, &addrlen);
						if (retVal >= 0)
							break;


					}
					if (resMsg.moreInfo)
					{
						printf("CHORD> FILE OWNER = IP : %s, Port : %d, ID : %d\n", inet_ntoa(Owner.addrInfo.sin_addr), ntohs(Owner.addrInfo.sin_port), Owner.ID);

						printf("\nCHORD> �ٿ�ε� ���� ������ �̸� : %s\n", filename);
						printf("\nCHORD> �ٿ�ε� ���� ������ Ű : %d\n", fkey);
						printf("CHORD> �ٿ�ε� ���� ������ ũ�� : %d\n", resMsg.bodySize);
					}

					struct sockaddr_in client;
					int addr_len = sizeof(client);
					if ((frSock = accept(flSock, &client, &addr_len)) == -1) {
						fprintf(stderr, "[ERROR] error_name : %s\n", strerror(errno));
						exit(1);
					}

					(HANDLE)_beginthreadex(NULL, 0, (void*)fileReceiver, (void*)& exitFlag, 0, NULL);

				}


				break;
			case 'f':
				printf("\nCHORD>Finger Table \n");
				printf("CHORD> Pre = IP : %s, Port : %d, ID : %d\n", inet_ntoa(myNode.chordInfo.fingerInfo.Pre.addrInfo.sin_addr), ntohs(myNode.chordInfo.fingerInfo.Pre.addrInfo.sin_port), myNode.chordInfo.fingerInfo.Pre.ID);

				for (i = 0; i < baseM; i++) {
					printf("CHORD> finger[%d] = IP : %s, Port : %d, ID : %d\n", i, inet_ntoa(myNode.chordInfo.fingerInfo.finger[i].addrInfo.sin_addr), ntohs(myNode.chordInfo.fingerInfo.finger[i].addrInfo.sin_port), myNode.chordInfo.fingerInfo.finger[i].ID);
				}
				break;
			case 'i':
				printf("CHORD> myInfo = IP : %s, Port : %d, ID : %d\n", inet_ntoa(myNode.nodeInfo.addrInfo.sin_addr), ntohs(myNode.nodeInfo.addrInfo.sin_port), myNode.nodeInfo.ID);

				for (i = 0; i < myNode.fileInfo.fileNum; i++) {
					printf("CHORD> File Info = num : %d��, ���� key : %d, �����̸� : %s, ���� ������ : ID %d, �������� ������ : ID %d \n",
						myNode.fileInfo.fileNum,
						myNode.fileInfo.fileRef[i].Key,
						myNode.fileInfo.fileRef[i].Name,
						myNode.fileInfo.fileRef[i].owner.ID,
						myNode.fileInfo.fileRef[i].refOwner.ID);
				}
				for (i = 0; i < myNode.chordInfo.FRefInfo.fileNum; i++) {
					printf("CHORD> FileRef = num : %d��, ���� key : %d, �����̸� : %s, ���� ������ : ID %d, �������� ������ : ID %d \n",
						myNode.chordInfo.FRefInfo.fileNum,
						myNode.chordInfo.FRefInfo.fileRef[i].Key,
						myNode.chordInfo.FRefInfo.fileRef[i].Name,
						myNode.chordInfo.FRefInfo.fileRef[i].owner.ID,
						myNode.chordInfo.FRefInfo.fileRef[i].refOwner.ID);
				}
				break;

			case 'm':
				if (mute) {
					printf("\nCHORD> Mute Off!!\n");
					mute = 0;
				}
				else {
					printf("\nCHORD> Mute On!!\n");
					mute = 1;
				}
				break;
			case 'h':
				break;
			}

		} while (select != 'q');
		if (select == 'q') {
			printf("\nCHORD> Chord Program Exit!!\n");
			exit(1);
		}
	}
	return 0;
}


static const unsigned char sTable[256] =
{
	0xa3,0xd7,0x09,0x83,0xf8,0x48,0xf6,0xf4,0xb3,0x21,0x15,0x78,0x99,0xb1,0xaf,0xf9,
	0xe7,0x2d,0x4d,0x8a,0xce,0x4c,0xca,0x2e,0x52,0x95,0xd9,0x1e,0x4e,0x38,0x44,0x28,
	0x0a,0xdf,0x02,0xa0,0x17,0xf1,0x60,0x68,0x12,0xb7,0x7a,0xc3,0xe9,0xfa,0x3d,0x53,
	0x96,0x84,0x6b,0xba,0xf2,0x63,0x9a,0x19,0x7c,0xae,0xe5,0xf5,0xf7,0x16,0x6a,0xa2,
	0x39,0xb6,0x7b,0x0f,0xc1,0x93,0x81,0x1b,0xee,0xb4,0x1a,0xea,0xd0,0x91,0x2f,0xb8,
	0x55,0xb9,0xda,0x85,0x3f,0x41,0xbf,0xe0,0x5a,0x58,0x80,0x5f,0x66,0x0b,0xd8,0x90,
	0x35,0xd5,0xc0,0xa7,0x33,0x06,0x65,0x69,0x45,0x00,0x94,0x56,0x6d,0x98,0x9b,0x76,
	0x97,0xfc,0xb2,0xc2,0xb0,0xfe,0xdb,0x20,0xe1,0xeb,0xd6,0xe4,0xdd,0x47,0x4a,0x1d,
	0x42,0xed,0x9e,0x6e,0x49,0x3c,0xcd,0x43,0x27,0xd2,0x07,0xd4,0xde,0xc7,0x67,0x18,
	0x89,0xcb,0x30,0x1f,0x8d,0xc6,0x8f,0xaa,0xc8,0x74,0xdc,0xc9,0x5d,0x5c,0x31,0xa4,
	0x70,0x88,0x61,0x2c,0x9f,0x0d,0x2b,0x87,0x50,0x82,0x54,0x64,0x26,0x7d,0x03,0x40,
	0x34,0x4b,0x1c,0x73,0xd1,0xc4,0xfd,0x3b,0xcc,0xfb,0x7f,0xab,0xe6,0x3e,0x5b,0xa5,
	0xad,0x04,0x23,0x9c,0x14,0x51,0x22,0xf0,0x29,0x79,0x71,0x7e,0xff,0x8c,0x0e,0xe2,
	0x0c,0xef,0xbc,0x72,0x75,0x6f,0x37,0xa1,0xec,0xd3,0x8e,0x62,0x8b,0x86,0x10,0xe8,
	0x08,0x77,0x11,0xbe,0x92,0x4f,0x24,0xc5,0x32,0x36,0x9d,0xcf,0xf3,0xa6,0xbb,0xac,
	0x5e,0x6c,0xa9,0x13,0x57,0x25,0xb5,0xe3,0xbd,0xa8,0x3a,0x01,0x05,0x59,0x2a,0x46
};


#define PRIME_MULT 1717

unsigned int str_hash(const char* str)  /* Hash: String to Key */
{
	unsigned int len = sizeof(str);
	unsigned int hash = len, i;


	for (i = 0; i != len; i++, str++) {
		hash ^= sTable[(*str + i) & 255];
		hash = hash * PRIME_MULT;
	}

	return hash % ringSize;
}

void printmenu()
{
	printf("CHORD> Enter a command - (c)reate: Create the chord network\n");
	printf("CHORD> Enter a command - (j)oin  : Join the chord network\n");
	printf("CHORD> Enter a command - (l)eave : Leave the chord network\n");
	printf("CHORD> Enter a command - (a)dd   : Add a file to the network\n");
	printf("CHORD> Enter a command - (d)elete: Delete a file to the network\n");
	printf("CHORD> Enter a command - (s)earch: File search and download\n");
	printf("CHORD> Enter a command - (f)inger: Show the finger table\n");
	printf("CHORD> Enter a command - (i)nfo  : Show the node information\n");
	printf("CHORD> Enter a command - (m)ute  : Toggle the silent mode\n");
	printf("CHORD> Enter a command - (h)elp  : Show the help message\n");
	printf("CHORD> Enter a command - (q)uit  : Quit the program\n");
	printf("CHORD> Enter your command ('help' for help message).\n");

	printf("CHORD> �޴��� �����ϼ��� : ");
}




int modIn(int modN, int targNum, int range1, int range2, int leftmode, int rightmode)
// leftmode, rightmode: 0 => range boundary not included, 1 => range boundary included   
{
	int result = 0;

	if (range1 == range2) {
		if ((leftmode == 0) || (rightmode == 0))
			return 0;
	}

	if (modPlus(ringSize, range1, 1) == range2) {
		if ((leftmode == 0) && (rightmode == 0))
			return 0;
	}

	if (leftmode == 0)
		range1 = modPlus(ringSize, range1, 1);
	if (rightmode == 0)
		range2 = modMinus(ringSize, range2, 1);

	if (range1 < range2) {
		if ((targNum >= range1) && (targNum <= range2))
			result = 1;
	}
	else if (range1 > range2) {
		if (((targNum >= range1) && (targNum < modN))
			|| ((targNum >= 0) && (targNum <= range2)))
			result = 1;
	}
	else if ((targNum == range1) && (targNum == range2))
		result = 1;

	return result;


}

int twoPow(int power)
{
	int i;
	int result = 1;

	if (power >= 0)
		for (i = 0; i < power; i++)
			result *= 2;
	else
		result = -1;

	return result;
}

int modMinus(int modN, int minuend, int subtrand)
{
	if (minuend - subtrand >= 0)
		return minuend - subtrand;
	else
		return (modN - subtrand) + minuend;
}

int modPlus(int modN, int addend1, int addend2)
{
	if (addend1 + addend2 < modN)
		return addend1 + addend2;
	else
		return (addend1 + addend2) - modN;
}
void procRecvMsg(void* SOCKET) {

	while (!exitFlag) {
		int retVal, i;
		chordHeaderType helpMsg, resMsg;
		struct sockaddr_in clintaddr;
		int addrlen = sizeof(clintaddr);

		memset(&helpMsg, 0, sizeof(helpMsg));
		memset(&resMsg, 0, sizeof(resMsg));


		retVal = -1;
		while (1) {
			retVal = recvfrom(rpSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & clintaddr, &addrlen);
			if (retVal >= 0) {
				break;
			}

		}

		if (resMsg.msgID == 1) {
			//Join Info
			if (myNode.nodeInfo.ID == myNode.chordInfo.fingerInfo.finger[0].ID)
			{
				helpMsg.msgID = 1;
				helpMsg.msgType = 1;
				helpMsg.nodeInfo = myNode.nodeInfo;
				helpMsg.moreInfo = 0;
				helpMsg.bodySize = 0;
				sendto(rpSock, (char*)& helpMsg, sizeof(helpMsg), 0,
					(struct sockaddr*) & clintaddr, addrlen);

			}
			else {
				if (modIn(ringSize, resMsg.nodeInfo.ID, myNode.chordInfo.fingerInfo.Pre.ID, myNode.nodeInfo.ID, 0, 1)) {
					helpMsg.msgID = 1;
					helpMsg.msgType = 1;
					helpMsg.nodeInfo = myNode.nodeInfo;
					helpMsg.moreInfo = 0;
					helpMsg.bodySize = 0;
					sendto(rpSock, (char*)& helpMsg, sizeof(helpMsg), 0,
						(struct sockaddr*) & clintaddr, addrlen);
				}
				else if (modIn(ringSize, resMsg.nodeInfo.ID, myNode.nodeInfo.ID, myNode.chordInfo.fingerInfo.finger[0].ID, 0, 1)) {
					helpMsg.msgID = 1;
					helpMsg.msgType = 1;
					helpMsg.nodeInfo = myNode.chordInfo.fingerInfo.finger[0];
					helpMsg.moreInfo = 0;
					helpMsg.bodySize = 0;
					sendto(rpSock, (char*)& helpMsg, sizeof(helpMsg), 0,
						(struct sockaddr*) & clintaddr, addrlen);
				}
				else {
					//Find Predecessor Req
					nodeInfoType helper, pred;
					helper = myNode.chordInfo.fingerInfo.Pre;
					memset(&helpMsg, 0, sizeof(helpMsg));
					helpMsg.msgID = 7;
					helpMsg.msgType = 0;
					helpMsg.nodeInfo = resMsg.nodeInfo;
					helpMsg.moreInfo = 0;
					helpMsg.bodySize = 0;
					sendto(rqSock, (char*)& helpMsg, sizeof(helpMsg), 0,
						(struct sockaddr*) & helper.addrInfo, sizeof(helper.addrInfo));

					retVal = -1;
					while (1) {

						retVal = recvfrom(rqSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & helper.addrInfo, addrlen);
						if (retVal >= 0)
							break;
					}

					pred = resMsg.nodeInfo;




					//Successor Info Req
					memset(&helpMsg, 0, sizeof(helpMsg));
					helpMsg.msgID = 5;
					helpMsg.msgType = 0;
					helpMsg.nodeInfo = myNode.nodeInfo;
					helpMsg.moreInfo = 0;
					helpMsg.bodySize = 0;

					sendto(rqSock, (char*)& helpMsg, sizeof(helpMsg), 0,
						(struct sockaddr*) & pred.addrInfo, sizeof(pred.addrInfo));

					memset(&resMsg, 0, sizeof(resMsg));
					retVal = -1;
					while (1) {

						retVal = recvfrom(rqSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & pred.addrInfo, addrlen);
						if (retVal >= 0)
							break;
					}

					helpMsg.msgID = 1;
					helpMsg.msgType = 1;
					helpMsg.nodeInfo = resMsg.nodeInfo;
					helpMsg.moreInfo = 0;

					sendto(rpSock, (char*)& helpMsg, sizeof(helpMsg), 0,
						(struct sockaddr*) & clintaddr, addrlen);
				}
			}



		}
		else if (resMsg.msgID == 2) {
			//MoveKeys
			int keyscount = 0;

			for (i = 0; i < myNode.chordInfo.FRefInfo.fileNum; i++) {
				if (modIn(ringSize, myNode.chordInfo.FRefInfo.fileRef[i].Key, myNode.chordInfo.fingerInfo.Pre.ID, resMsg.nodeInfo.ID, 0, 1)) {
					keyscount++;

				}
			}

			helpMsg.moreInfo = keyscount;
			sendto(rpSock, (char*)& helpMsg, sizeof(helpMsg), 0,
				(struct sockaddr*) & clintaddr, addrlen);

			for (i = 0; i < myNode.chordInfo.FRefInfo.fileNum; i++) {

				if (modIn(ringSize, myNode.chordInfo.FRefInfo.fileRef[i].Key, myNode.chordInfo.fingerInfo.Pre.ID, resMsg.nodeInfo.ID, 0, 1)) {

					helpMsg.msgID = 2;
					helpMsg.msgType = 1;
					helpMsg.nodeInfo = myNode.nodeInfo;
					helpMsg.fileInfo = myNode.chordInfo.FRefInfo.fileRef[i];
					helpMsg.moreInfo = keyscount;
					helpMsg.bodySize = sizeof(keyscount);

					sendto(rpSock, (char*)& helpMsg, sizeof(helpMsg), 0,
						(struct sockaddr*) & clintaddr, addrlen);



					if (myNode.chordInfo.FRefInfo.fileRef[i].owner.ID == myNode.nodeInfo.ID) {

						for (int j = i; j < myNode.chordInfo.FRefInfo.fileNum; j++) {
							myNode.chordInfo.FRefInfo.fileRef[j] = myNode.chordInfo.FRefInfo.fileRef[j + 1];
						}
						myNode.fileInfo.fileRef[i].refOwner = resMsg.nodeInfo;
						myNode.chordInfo.FRefInfo.fileNum--;
						i--;
						continue;
					}
					helpMsg.msgID = 12;
					helpMsg.msgType = 0;
					helpMsg.nodeInfo = resMsg.nodeInfo;
					helpMsg.fileInfo = myNode.chordInfo.FRefInfo.fileRef[i];
					helpMsg.moreInfo = 0;
					helpMsg.bodySize = 0;


					sendto(rqSock, (char*)& helpMsg, sizeof(helpMsg), 0,
						(struct sockaddr*) & myNode.chordInfo.FRefInfo.fileRef[i].owner.addrInfo, addrlen);

					chordHeaderType resMsg2;

					while (1) {
						retVal = recvfrom(rqSock, (char*)& resMsg2, sizeof(resMsg2), 0, (struct sockaddr*) & myNode.chordInfo.FRefInfo.fileRef[i].owner.addrInfo, &addrlen);
						if (retVal >= 0)
							break;
					}

					for (int j = i; j < myNode.chordInfo.FRefInfo.fileNum; j++) {
						myNode.chordInfo.FRefInfo.fileRef[j] = myNode.chordInfo.FRefInfo.fileRef[j + 1];
					}
					myNode.chordInfo.FRefInfo.fileNum--;
					i--;

				}

			}
		}
		else if (resMsg.msgID == 3) {
			//pred info
			helpMsg.msgID = 3;
			helpMsg.msgType = 1;
			helpMsg.nodeInfo = myNode.chordInfo.fingerInfo.Pre;
			helpMsg.moreInfo = 0;
			helpMsg.bodySize = 0;
			sendto(rpSock, (char*)& helpMsg, sizeof(helpMsg), 0,
				(struct sockaddr*) & clintaddr, addrlen);
		}
		else if (resMsg.msgID == 4) {
			//pred update
			myNode.chordInfo.fingerInfo.Pre = resMsg.nodeInfo;
			helpMsg.msgID = 4;
			helpMsg.msgType = 1;
			helpMsg.nodeInfo = myNode.nodeInfo;
			helpMsg.moreInfo = 1;
			helpMsg.bodySize = 0;
			sendto(rpSock, (char*)& helpMsg, sizeof(helpMsg), 0,
				(struct sockaddr*) & clintaddr, addrlen);
		}
		else if (resMsg.msgID == 5) {
			//succ info

			helpMsg.msgID = 5;
			helpMsg.msgType = 1;
			helpMsg.nodeInfo = myNode.chordInfo.fingerInfo.finger[0];
			helpMsg.moreInfo = 0;
			helpMsg.bodySize = 0;
			sendto(rpSock, (char*)& helpMsg, sizeof(helpMsg), 0,
				(struct sockaddr*) & clintaddr, addrlen);
		}
		else if (resMsg.msgID == 6) {
			//succ update
			myNode.chordInfo.fingerInfo.finger[0] = resMsg.nodeInfo;
			helpMsg.msgID = 6;
			helpMsg.msgType = 1;
			helpMsg.nodeInfo = myNode.nodeInfo;
			helpMsg.moreInfo = 1;
			helpMsg.bodySize = 0;
			sendto(rpSock, (char*)& helpMsg, sizeof(helpMsg), 0,
				(struct sockaddr*) & clintaddr, addrlen);
		}
		else if (resMsg.msgID == 7) {
			//Find Predecessor

			if (myNode.nodeInfo.ID == myNode.chordInfo.fingerInfo.finger[0].ID) //the initial node
			{
				helpMsg.msgID = 7;
				helpMsg.msgType = 1;
				helpMsg.nodeInfo = myNode.nodeInfo;
				helpMsg.moreInfo = 0;
				helpMsg.bodySize = 0;
				sendto(rpSock, (char*)& helpMsg, sizeof(helpMsg), 0,
					(struct sockaddr*) & clintaddr, addrlen);
			}
			else {
				nodeInfoType pred;
				pred = myNode.chordInfo.fingerInfo.Pre;
				while (1) {
					if (modIn(ringSize, resMsg.nodeInfo.ID, pred.ID, myNode.nodeInfo.ID, 0, 1)) {
						helpMsg.msgID = 7;
						helpMsg.msgType = 1;
						helpMsg.nodeInfo = pred;
						helpMsg.moreInfo = 0;
						helpMsg.bodySize = 0;
						sendto(rpSock, (char*)& helpMsg, sizeof(helpMsg), 0,
							(struct sockaddr*) & clintaddr, addrlen);
						break;
					}
					else if (modIn(ringSize, resMsg.nodeInfo.ID, myNode.nodeInfo.ID, myNode.chordInfo.fingerInfo.finger[0].ID, 0, 1)) {

						helpMsg.msgID = 7;
						helpMsg.msgType = 1;
						helpMsg.nodeInfo = myNode.nodeInfo;
						helpMsg.moreInfo = 0;
						helpMsg.bodySize = 0;
						sendto(rpSock, (char*)& helpMsg, sizeof(helpMsg), 0,
							(struct sockaddr*) & clintaddr, addrlen);
						break;
					}
					else {
						helpMsg.msgID = 3;
						helpMsg.msgType = 0;
						helpMsg.nodeInfo = myNode.nodeInfo;
						helpMsg.moreInfo = 0;
						helpMsg.bodySize = 0;
						sendto(rqSock, (char*)& helpMsg, sizeof(helpMsg), 0,
							(struct sockaddr*) & pred.addrInfo, addrlen);

						chordHeaderType resMsg2;
						retVal = -1;
						while (1) {
							retVal = recvfrom(rqSock, (char*)& resMsg2, sizeof(resMsg2), 0, (struct sockaddr*) & pred.addrInfo, &addrlen);
							if (retVal >= 0)
								break;
						}
						pred = resMsg2.nodeInfo;

					}
				}
			}

		}
		else if (resMsg.msgID == 8) {
			// File Reference Add

			//Succ�� �������� �ֱ�			
			myNode.chordInfo.FRefInfo.fileRef[myNode.chordInfo.FRefInfo.fileNum]
				= resMsg.fileInfo;
			myNode.chordInfo.FRefInfo.fileRef[myNode.chordInfo.FRefInfo.fileNum].refOwner = myNode.nodeInfo;
			myNode.chordInfo.FRefInfo.fileNum++;

			helpMsg.msgID = 8;
			helpMsg.msgType = 1;
			helpMsg.nodeInfo = myNode.nodeInfo;
			helpMsg.fileInfo = myNode.chordInfo.FRefInfo.fileRef[myNode.chordInfo.FRefInfo.fileNum - 1];
			helpMsg.moreInfo = 1;
			helpMsg.bodySize = 0;
			sendto(rpSock, (char*)& helpMsg, sizeof(helpMsg), 0, (struct sockaddr*) & clintaddr, addrlen);
		}
		else if (resMsg.msgID == 9) {
			//File Reference Delete 
			for (i = 0; i < myNode.chordInfo.FRefInfo.fileNum; i++) {
				if (myNode.chordInfo.FRefInfo.fileRef[i].Key == resMsg.fileInfo.Key) {
					memset(&myNode.chordInfo.FRefInfo.fileRef[i], 0, sizeof(myNode.chordInfo.FRefInfo.fileRef[i]));//Succ�� �������� ����		
					for (int j = i; j < myNode.chordInfo.FRefInfo.fileNum - 1; j++) {
						myNode.chordInfo.FRefInfo.fileRef[j] = myNode.chordInfo.FRefInfo.fileRef[j + 1];//Succ�� �������� ������				
					}
				}
			}
			myNode.chordInfo.FRefInfo.fileNum--;

			memset(&helpMsg, 0, sizeof(helpMsg));

			helpMsg.msgID = 9;
			helpMsg.msgType = 1;
			helpMsg.moreInfo = 1;
			helpMsg.bodySize = 0;
			sendto(rpSock, (char*)& helpMsg, sizeof(helpMsg), 0,
				(struct sockaddr*) & clintaddr, addrlen);

		}
		else if (resMsg.msgID == 10) {
			//FileDown Request

			fsSock = socket(AF_INET, SOCK_STREAM, 0);
			while (1) {
				if (connect(fsSock, (struct sockaddr*) & resMsg.nodeInfo.addrInfo, sizeof(struct sockaddr_in)) != -1) {
					break;
				}
			}




			for (i = 0; i < myNode.fileInfo.fileNum; i++) {
				if (resMsg.fileInfo.Key == myNode.fileInfo.fileRef[i].Key) {
					strcpy(filename, myNode.fileInfo.fileRef[i].Name);
					FILE* fp = fopen(filename, "r");
					int filesize = 0;
					if (fp == NULL) {
						perror("���� ����� ����");
						return -1;
					}
					fseek(fp, 0, SEEK_END);
					filesize = ftell(fp);
					rewind(fp);
					fclose(fp);
					helpMsg.msgID = 10;
					helpMsg.msgType = 1;
					helpMsg.moreInfo = 1;
					helpMsg.bodySize = filesize;
					sendto(rpSock, (char*)& helpMsg, sizeof(helpMsg), 0, (struct sockaddr*) & clintaddr, addrlen);
					(HANDLE)_beginthreadex(NULL, 0, (void*)fileSender, (void*)& exitFlag, 0, NULL);
					break;
				}
			}


		}
		else if (resMsg.msgID == 11) {
			//File Reference Info 
			memset(&helpMsg, 0, sizeof(helpMsg));
			helpMsg.moreInfo = 0;
			for (i = 0; i < myNode.chordInfo.FRefInfo.fileNum; i++) {
				if (resMsg.fileInfo.Key == myNode.chordInfo.FRefInfo.fileRef[i].Key) {
					helpMsg.fileInfo = myNode.chordInfo.FRefInfo.fileRef[i];
					helpMsg.moreInfo = 1;
					break;
				}
			}
			helpMsg.msgID = 11;
			helpMsg.msgType = 1;
			helpMsg.nodeInfo = myNode.nodeInfo;
			helpMsg.bodySize = 0;
			sendto(rpSock, (char*)& helpMsg, sizeof(helpMsg), 0, (struct sockaddr*) & clintaddr, addrlen);

		}
		else if (resMsg.msgID == 12)
		{
			memset(&helpMsg, 0, sizeof(helpMsg));
			helpMsg.moreInfo = 1;
			for (i = 0; i < myNode.fileInfo.fileNum; i++) {
				if (resMsg.fileInfo.Key == myNode.fileInfo.fileRef[i].Key) {
					myNode.fileInfo.fileRef[i].refOwner = resMsg.nodeInfo;
					break;
				}
			}
			sendto(rpSock, (char*)& helpMsg, sizeof(helpMsg), 0, (struct sockaddr*) & clintaddr, addrlen);
		}
		else if (resMsg.msgID == 13)
		{
			//��� ���� Ȯ��
			memset(&helpMsg, 0, sizeof(helpMsg));
			sendto(rpSock, (char*)& helpMsg, sizeof(helpMsg), 0, (struct sockaddr*) & clintaddr, addrlen);
		}


	}
}
void procPP(void* SOCKET) {

	while (!exitFlag) {
		int i;
		int PPsucc2 = 0;
		int retVal;
		chordHeaderType helpMsg, resMsg, resMsg2;
		int addrlen = sizeof(struct sockaddr_in);
		if (myNode.chordInfo.fingerInfo.finger[0].ID == myNode.nodeInfo.ID) {
			//the init Node
		}
		else {
			memset(&helpMsg, 0, sizeof(helpMsg));
			helpMsg.msgID = 13;
			helpMsg.msgType = 0;
			helpMsg.nodeInfo = myNode.nodeInfo;
			helpMsg.moreInfo = 0;
			helpMsg.bodySize = 0;

			sendto(ppSock, (char*)& helpMsg, sizeof(helpMsg), 0,
				(struct sockaddr*) & myNode.chordInfo.fingerInfo.Pre.addrInfo, sizeof(myNode.chordInfo.fingerInfo.Pre.addrInfo));
			retVal = -1;
			retVal = recvfrom(ppSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & myNode.chordInfo.fingerInfo.Pre.addrInfo, &addrlen);

			if (retVal < 0) {

				for (i = 0; i < myNode.fileInfo.fileNum; i++)
				{
					if (myNode.fileInfo.fileRef[i].refOwner.ID == myNode.chordInfo.fingerInfo.Pre.ID)
						myNode.fileInfo.fileRef[i].refOwner = myNode.nodeInfo;
				}


				myNode.chordInfo.fingerInfo.Pre.ID = 0;
				PPsucc2 = 1;
				printf("\nCHORD> leave ������...\n");


			}
			for (i = 0; i < baseM; i++) {

				memset(&helpMsg, 0, sizeof(helpMsg));
				helpMsg.msgID = 13;
				helpMsg.msgType = 0;
				helpMsg.nodeInfo = myNode.nodeInfo;
				helpMsg.moreInfo = 0;
				helpMsg.bodySize = 0;

				if (myNode.chordInfo.fingerInfo.finger[i].ID == myNode.nodeInfo.ID) {
					//the special case
				}
				else {
					sendto(ppSock, (char*)& helpMsg, sizeof(helpMsg), 0,
						(struct sockaddr*) & myNode.chordInfo.fingerInfo.finger[i].addrInfo, sizeof(myNode.chordInfo.fingerInfo.finger[i].addrInfo));
					retVal = -1;
					retVal = recvfrom(ppSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & myNode.chordInfo.fingerInfo.finger[i].addrInfo, &addrlen);

					if (retVal < 0) {
						printf("\nCHORD> leave ������...\n");
						PPsucc2 = 1;
						nodeInfoType pred;
						if (i == baseM - 1) {
							myNode.chordInfo.fingerInfo.finger[i] = myNode.chordInfo.fingerInfo.Pre;
						}
						else {
							myNode.chordInfo.fingerInfo.finger[i] = myNode.chordInfo.fingerInfo.finger[i + 1];

						}
						if (i == 0) {
							pred = myNode.chordInfo.fingerInfo.finger[i];
						}
					}

				}

			}



		}

		if (myNode.chordInfo.fingerInfo.Pre.ID == 0) {
			nodeInfoType succ, temp;
			succ = myNode.chordInfo.fingerInfo.finger[0];

			memset(&helpMsg, 0, sizeof(helpMsg));
			helpMsg.msgID = 13;
			helpMsg.msgType = 0;
			helpMsg.nodeInfo = myNode.nodeInfo;
			helpMsg.moreInfo = 0;
			helpMsg.bodySize = 0;

			sendto(ppSock, (char*)& helpMsg, sizeof(helpMsg), 0,
				(struct sockaddr*) & succ.addrInfo, sizeof(succ.addrInfo));
			retVal = -1;
			retVal = recvfrom(ppSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & succ.addrInfo, &addrlen);

			if (retVal < 0) {
				myNode.chordInfo.fingerInfo.Pre = myNode.nodeInfo;

				for (i = 0; i < baseM; i++)
					myNode.chordInfo.fingerInfo.finger[i] = myNode.nodeInfo;


			}
			else {
				while (1) {
					memset(&helpMsg, 0, sizeof(helpMsg));
					helpMsg.msgID = 5;
					helpMsg.msgType = 0;
					helpMsg.nodeInfo = myNode.nodeInfo;
					helpMsg.moreInfo = 0;
					helpMsg.bodySize = 0;

					sendto(ppSock, (char*)& helpMsg, sizeof(helpMsg), 0,
						(struct sockaddr*) & succ.addrInfo, sizeof(succ.addrInfo));
					retVal = -1;
					retVal = recvfrom(ppSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & succ.addrInfo, &addrlen);

					if (retVal < 0) {
						myNode.chordInfo.fingerInfo.Pre = temp;

						helpMsg.msgID = 6;
						helpMsg.msgType = 1;
						helpMsg.nodeInfo = myNode.nodeInfo;
						helpMsg.moreInfo = 0;
						helpMsg.bodySize = 0;
						sendto(ppSock, (char*)& helpMsg, sizeof(helpMsg), 0,
							(struct sockaddr*) & temp.addrInfo, addrlen);

						recvfrom(ppSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & temp.addrInfo, &addrlen);

						for (i = 0; i < baseM; i++) {
							if (myNode.chordInfo.fingerInfo.finger[i].ID == 0) {
								myNode.chordInfo.fingerInfo.finger[i] = myNode.nodeInfo;
							}
						}
						break;
					}
					temp = succ;
					succ = resMsg.nodeInfo;

				}
			}
			PPsucc2 = 1;
		}

		int PPsucc = 1;
		if (myNode.chordInfo.fingerInfo.Pre.ID != 0 && PPsucc == 1 && PPsucc2 == 1) {
			for (int j = 0; j < baseM; j++)
				if (myNode.chordInfo.fingerInfo.finger[j].ID == 0)
					PPsucc = 0;
			if (PPsucc) {
				for (i = 0; i < myNode.fileInfo.fileNum; i++) {

					if (myNode.chordInfo.fingerInfo.Pre.ID == myNode.nodeInfo.ID) {
						for (int k = 0; k < myNode.fileInfo.fileNum; k++) {
							myNode.chordInfo.FRefInfo.fileRef[myNode.chordInfo.FRefInfo.fileNum]
								= myNode.fileInfo.fileRef[k];
							myNode.chordInfo.FRefInfo.fileNum++;

						}
					}
					memset(&helpMsg, 0, sizeof(helpMsg));
					helpMsg.msgID = 13;
					helpMsg.msgType = 0;
					helpMsg.nodeInfo = myNode.nodeInfo;
					helpMsg.moreInfo = 0;
					helpMsg.bodySize = 0;

					sendto(ppSock, (char*)& helpMsg, sizeof(helpMsg), 0,
						(struct sockaddr*) & myNode.fileInfo.fileRef[i].refOwner.addrInfo, sizeof(myNode.fileInfo.fileRef[i].refOwner.addrInfo));
					retVal = -1;
					retVal = recvfrom(ppSock, (char*)& resMsg2, sizeof(resMsg2), 0, (struct sockaddr*) & myNode.fileInfo.fileRef[i].refOwner.addrInfo, &addrlen);

					if (retVal < 0) {
						if (myNode.nodeInfo.ID == myNode.chordInfo.fingerInfo.Pre.ID) {
							myNode.fileInfo.fileRef[i].refOwner = myNode.nodeInfo;//��忡 ������ �����ָ� �ڽ����� �Է�


							myNode.chordInfo.FRefInfo.fileRef[myNode.chordInfo.FRefInfo.fileNum]
								= myNode.fileInfo.fileRef[i];
							myNode.chordInfo.FRefInfo.fileNum++;

						}
						else {
							nodeType tempNode;

							tempNode.nodeInfo = myNode.nodeInfo;
							tempNode.nodeInfo.ID = myNode.fileInfo.fileRef[i].Key;
							//Find Predecessor Request
							memset(&helpMsg, 0, sizeof(helpMsg));
							helpMsg.msgID = 7;
							helpMsg.msgType = 0;
							helpMsg.nodeInfo = tempNode.nodeInfo;
							helpMsg.moreInfo = 0;
							helpMsg.bodySize = 0;
							nodeInfoType helper = myNode.chordInfo.fingerInfo.Pre;
							sendto(ppSock, (char*)& helpMsg, sizeof(helpMsg), 0,
								(struct sockaddr*) & helper.addrInfo, sizeof(helper.addrInfo));

							retVal = -1;
							while (1) {
								retVal = recvfrom(ppSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & helper.addrInfo, &addrlen);
								if (retVal >= 0)
									break;
							}

							//Successor Info Request
							memset(&helpMsg, 0, sizeof(helpMsg));
							helpMsg.msgID = 5;
							helpMsg.msgType = 0;
							helpMsg.nodeInfo = tempNode.nodeInfo;
							helpMsg.moreInfo = 0;
							helpMsg.bodySize = 0;


							nodeInfoType pred = resMsg.nodeInfo;
							sendto(ppSock, (char*)& helpMsg, sizeof(helpMsg), 0,
								(struct sockaddr*) & pred.addrInfo, sizeof(pred.addrInfo));
							memset(&resMsg, 0, sizeof(resMsg));
							retVal = -1;
							while (1) {

								retVal = recvfrom(ppSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & pred.addrInfo, &addrlen);
								if (retVal >= 0)
									break;

								//fkey�� Successor
								nodeInfoType succ = resMsg.nodeInfo;

								//�ߺ� Ȯ��
								memset(&helpMsg, 0, sizeof(helpMsg));
								helpMsg.msgID = 11;
								helpMsg.msgType = 0;
								helpMsg.nodeInfo = myNode.nodeInfo;
								helpMsg.fileInfo.Key = myNode.fileInfo.fileRef[i].Key;
								helpMsg.moreInfo = 0;
								helpMsg.bodySize = 0;


								sendto(ppSock, (char*)& helpMsg, sizeof(helpMsg), 0,
									(struct sockaddr*) & succ.addrInfo, sizeof(succ.addrInfo));
								memset(&resMsg, 0, sizeof(resMsg));
								retVal = -1;
								while (1) {

									retVal = recvfrom(ppSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & succ.addrInfo, &addrlen);
									if (retVal >= 0)
										break;
								}



								myNode.fileInfo.fileRef[i].refOwner = succ;


								//key�� Successor���� ������ �������� �Է¿�û
								//File Reference Add Request
								memset(&helpMsg, 0, sizeof(helpMsg));
								helpMsg.msgID = 8;
								helpMsg.msgType = 0;
								helpMsg.nodeInfo = myNode.nodeInfo;
								helpMsg.fileInfo = myNode.fileInfo.fileRef[i];
								helpMsg.moreInfo = 0;
								helpMsg.bodySize = 0;

								sendto(ppSock, (char*)& helpMsg, sizeof(helpMsg), 0,
									(struct sockaddr*) & succ.addrInfo, sizeof(succ.addrInfo));
								memset(&resMsg, 0, sizeof(resMsg));
								retVal = -1;
								while (1) {
									retVal = recvfrom(ppSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & succ.addrInfo, &addrlen);
									if (retVal >= 0)
										break;
								}

							}


						}
					}

				}
			}
			printf("\nCHORD> leave ����...\n");
		}

	}


}
void procFF(void* SOCKET) {//fixfinger �޽��� ó�� ������

	while (!exitFlag) {
		int i;
		chordHeaderType helpMsg, resMsg;
		nodeInfoType succ;
		int addrlen = sizeof(struct sockaddr_in);

		if (myNode.nodeInfo.ID == myNode.chordInfo.fingerInfo.finger[0].ID) {
			for (i = 1; i < baseM; i++) {
				myNode.chordInfo.fingerInfo.finger[i] = myNode.nodeInfo;
			}
		}
		else {
			succ = myNode.chordInfo.fingerInfo.finger[0];

			for (i = 1; i < baseM; i++) {
				int IDKey = modPlus(ringSize, myNode.nodeInfo.ID, twoPow(i));

				if (succ.ID == IDKey) {
					myNode.chordInfo.fingerInfo.finger[i] = succ;
				}
				else if (modIn(ringSize, IDKey, myNode.nodeInfo.ID, succ.ID, 0, 1)) {
					myNode.chordInfo.fingerInfo.finger[i] = succ;
				}
				else if (modIn(ringSize, IDKey, myNode.chordInfo.fingerInfo.Pre.ID, myNode.nodeInfo.ID, 0, 1)) {
					myNode.chordInfo.fingerInfo.finger[i] = myNode.nodeInfo;
				}
				else {
					memset(&helpMsg, 0, sizeof(helpMsg));
					helpMsg.msgID = 5;
					helpMsg.msgType = 0;
					helpMsg.nodeInfo = myNode.nodeInfo;
					helpMsg.moreInfo = 0;
					helpMsg.bodySize = 0;

					sendto(ffSock, (char*)& helpMsg, sizeof(helpMsg), 0,
						(struct sockaddr*) & succ.addrInfo, sizeof(succ.addrInfo));

					while (1) {
						if (0 <= recvfrom(ffSock, (char*)& resMsg, sizeof(resMsg), 0, (struct sockaddr*) & succ.addrInfo, &addrlen))
							break;
					}
					succ = resMsg.nodeInfo;
					i--;
				}
			}

		}
	}

}
void fileSender(void* SOCKET) {

	char buf[fBufSize];

	FILE* fd = fopen(filename, "r");
	if (fd == NULL)
	{
		printf("\n[ERROR] %s��� �̸��� ������ �������� �ʽ��ϴ�.\n", filename);
		exit(1);
	}

	memset(&buf, 0, fBufSize);
	int buflen;
	while ((buflen = fread(buf, sizeof(char), fBufSize, fd)) > 0)
	{
		if (send(fsSock, buf, buflen, 0) < 0)
		{
			printf("\n[ERROR] fail to send file %s\n", filename);
			break;
		}
		memset(&buf, 0, fBufSize);
	}


	fclose(fd);
	closesocket(fsSock);

	printf("\n");
	Sleep(2000);

	printmenu();

}

void fileReceiver(void* SOCKET) {
	FILE* fd = fopen(filename, "w");
	char buf[fBufSize];
	memset(&buf, 0, fBufSize);
	int buflen = 0;
	int wrtielen;
	while (buflen = recv(frSock, buf, fBufSize, 0))
	{

		wrtielen = fwrite(buf, sizeof(char), buflen, fd);
		if (wrtielen < buflen)
		{
			printf("\n[ERROR] file write failed.\n");
		}
		else if (buflen)
		{
			break;
		}

		memset(&buf, 0, fBufSize);
	}

	Sleep(2000);
	printf("\nCHORD> ���� �ٿ�ε� �Ϸ�!!\n");



	fclose(fd);

	closesocket(flSock);
	closesocket(frSock);

	printf("\n");
	printmenu();

}
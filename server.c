#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>

struct _client{
    char ipAddress[40];
    int port;
    char name[40];
} tcpClients[4];

int nbClients;
int fsmServer; //fini state machine: l'état dans lequel se trouve le serveur: 0 en attente; 1 face au jeu
int deck[13]={0,1,2,3,4,5,6,7,8,9,10,11,12};
int tableCartes[4][8];
char *nomcartes[]=
{"Sebastian Moran", "irene Adler", "inspector Lestrade",
  "inspector Gregson", "inspector Baynes", "inspector Bradstreet",
  "inspector Hopkins", "Sherlock Holmes", "John Watson", "Mycroft Holmes",
  "Mrs. Hudson", "Mary Morstan", "James Moriarty"};
int joueurCourant;
int cpt=0;   //Compte le nombre de perdant
int perdant[4] = {-1, -1, -1, -1};

void error(const char *msg){
    perror(msg);
    exit(1);
}

void melangerDeck(){
	int i;
	int index1,index2,tmp;

	srand(time(NULL));

	for (i=0;i<1000;i++){
		index1=rand()%13;
		index2=rand()%13;

		tmp=deck[index1];
		deck[index1]=deck[index2];
		deck[index2]=tmp;
	}
}

void createTable(){
	// Le joueur 0 possede les cartes d'indice 0,1,2
	// Le joueur 1 possede les cartes d'indice 3,4,5 
	// Le joueur 2 possede les cartes d'indice 6,7,8 
	// Le joueur 3 possede les cartes d'indice 9,10,11 
	// Le coupable est la carte d'indice 12
	int i,j,c;

	for (i=0;i<4;i++)
		for (j=0;j<8;j++)
			tableCartes[i][j]=0;

	for (i=0;i<4;i++){
		for (j=0;j<3;j++){
			c=deck[i*3+j];
			switch (c){
				case 0: // Sebastian Moran
					tableCartes[i][7]++;
					tableCartes[i][2]++;
					break;
				case 1: // Irene Adler
					tableCartes[i][7]++;
					tableCartes[i][1]++;
					tableCartes[i][5]++;
					break;
				case 2: // Inspector Lestrade
					tableCartes[i][3]++;
					tableCartes[i][6]++;
					tableCartes[i][4]++;
					break;
				case 3: // Inspector Gregson 
					tableCartes[i][3]++;
					tableCartes[i][2]++;
					tableCartes[i][4]++;
					break;
				case 4: // Inspector Baynes 
					tableCartes[i][3]++;
					tableCartes[i][1]++;
					break;
				case 5: // Inspector Bradstreet 
					tableCartes[i][3]++;
					tableCartes[i][2]++;
					break;
				case 6: // Inspector Hopkins 
					tableCartes[i][3]++;
					tableCartes[i][0]++;
					tableCartes[i][6]++;
					break;
				case 7: // Sherlock Holmes 
					tableCartes[i][0]++;
					tableCartes[i][1]++;
					tableCartes[i][2]++;
					break;
				case 8: // John Watson 
					tableCartes[i][0]++;
					tableCartes[i][6]++;
					tableCartes[i][2]++;
					break;
				case 9: // Mycroft Holmes 
					tableCartes[i][0]++;
					tableCartes[i][1]++;
					tableCartes[i][4]++;
					break;
				case 10: // Mrs. Hudson 
					tableCartes[i][0]++;
					tableCartes[i][5]++;
					break;
				case 11: // Mary Morstan 
					tableCartes[i][4]++;
					tableCartes[i][5]++;
					break;
				case 12: // James Moriarty 
					tableCartes[i][7]++;
					tableCartes[i][1]++;
					break;
			}
		}
	}
} 

void printDeck() //afficher le paquet de carte
{
	int i,j;

	for (i=0;i<13;i++)
			printf("%d %s\n",deck[i],nomcartes[deck[i]]);

	for (i=0;i<4;i++){
		for (j=0;j<8;j++)
			printf("%2.2d ",tableCartes[i][j]);
		puts("");
	}
}

void printClients() //afficher la structure de données des clients: adresse IP, numéro de port pour renvoyer le message et le nom de joueur
{
	int i;

	for (i=0;i<nbClients;i++) //i: indice de la joueur
		printf("%d: %s %5.5d %s\n",i,tcpClients[i].ipAddress,
			tcpClients[i].port,
			tcpClients[i].name);
}

int findClientByName(char *name) //Trouver l'indice d'un joueur à partir de son nom
{
	int i;

	for (i=0;i<nbClients;i++)
		if (strcmp(tcpClients[i].name,name)==0)
			return i;
	return -1;
}

void sendMessageToClient(char *clientip,int clientport,char *mess) //Si je connais l'adresse IP d'un client, ainsi son numéro de port, je peux lui envoyer un message
{
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buffer[256];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    server = gethostbyname(clientip);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
        (char *)&serv_addr.sin_addr.s_addr,
        server->h_length);
    serv_addr.sin_port = htons(clientport);
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0){
		printf("ERROR connecting\n");
		exit(1);
    }

	sprintf(buffer,"%s\n",mess);
	n = write(sockfd,buffer,strlen(buffer));

    close(sockfd);
}

void broadcastMessage(char *mess) //Envoyer le message à tous les joueurs
{
	int i;

	for (i=0;i<nbClients;i++)
		sendMessageToClient(tcpClients[i].ipAddress,
			tcpClients[i].port,					
			mess);
}


int main(int argc, char *argv[])
{
	int sockfd, newsockfd, portno;
	socklen_t clilen;
	char buffer[256];
	struct sockaddr_in serv_addr, cli_addr;
	int n;
	int i;

	char com;
	char clientIpAddress[256], clientName[256];
	int clientPort;
	int id;
	char reply[256];


	if (argc < 2) { //vérifie le nombre de paramètre
		fprintf(stderr,"ERROR, no port provided\n");
		exit(1);
	}
	sockfd = socket(AF_INET, SOCK_STREAM, 0); //création d'un socket
	if (sockfd < 0) 
	error("ERROR opening socket");
	bzero((char *) &serv_addr, sizeof(serv_addr));
	portno = atoi(argv[1]);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);
	if (bind(sockfd, (struct sockaddr *) &serv_addr, //création d'unbind: associe un numéro de port à socet
		sizeof(serv_addr)) < 0) 
			error("ERROR on binding");
	listen(sockfd,5); //listen
	clilen = sizeof(cli_addr);

	printDeck();
	melangerDeck();
	createTable();
	printDeck();
	joueurCourant=0;

	for (i=0;i<4;i++){
		strcpy(tcpClients[i].ipAddress,"localhost");
		tcpClients[i].port=-1;
		strcpy(tcpClients[i].name,"-");
	}

     while (1) //Rentrer dans la boucle principale du serveur
    {    
     	newsockfd = accept(sockfd, //Je me block en attendant la connexion d'un client
                 (struct sockaddr *) &cli_addr,  //Si un client se connecte pour envoyer un message, je récupçre un nouveau descripteur de socket
                 &clilen);
     	if (newsockfd < 0) 
          	error("ERROR on accept");

     	bzero(buffer,256);
     	n = read(newsockfd,buffer,255); //Je lis le message que le client m'a envoyé
     	if (n < 0) 
		error("ERROR reading from socket");

        printf("Received packet from %s:%d\nData: [%s]\n\n", // j'affiche des informations à faire des debug
                inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), buffer);

        //Savoir ce qu'on doit faire pour chaque état
        if (fsmServer==0) //en attend de 4 connexions
        {
        	switch (buffer[0]) //je regarde le message qui m'a envoyé
        	{
				case 'C': //il y a une connexion
				sscanf(buffer,"%c %s %d %s", &com, clientIpAddress, &clientPort, clientName); //Je récupère dans les arguments du message qui m'a envoyé (buffer) : la commande de connexion, adrIP du client, son numéro de port et son nom
				printf("COM=%c ipAddress=%s port=%d name=%s\n",com, clientIpAddress, clientPort, clientName);

			// fsmServer==0 alors j'attends les connexions de tous les joueurs
				strcpy(tcpClients[nbClients].ipAddress,clientIpAddress); //On remplit la table tcpClients
				tcpClients[nbClients].port=clientPort;
				strcpy(tcpClients[nbClients].name,clientName);
				nbClients++;

				printClients();

			// rechercher l'id du joueur qui vient de se connecter

				id=findClientByName(clientName);
				printf("id=%d\n",id);

			// lui envoyer un message personnel pour lui communiquer son id

				sprintf(reply,"I %d",id);
				sendMessageToClient(tcpClients[id].ipAddress,
						tcpClients[id].port,
						reply);

			// Envoyer un message broadcast pour communiquer a tout le monde la liste des joueurs actuellement
			// connectes

				sprintf(reply,"L %s %s %s %s", tcpClients[0].name, tcpClients[1].name, tcpClients[2].name, tcpClients[3].name);
				broadcastMessage(reply);

			// Si le nombre de joueurs atteint 4, alors on peut lancer le jeu

				if (nbClients==4){
				// On envoie leurs cartes aux joueurs, ainsi que la ligne qui leur correspond dans tableCartes
					for (int i=0; i<4; i++){
						sprintf(reply, "D %d %d %d", deck[3*i], deck[3*i+1], deck[3*i+2]);
						sendMessageToClient(tcpClients[i].ipAddress, tcpClients[i].port, reply);
						for (int j=0; j<8; j++){
							sprintf(reply, "V %d %d %d", i, j, tableCartes[i][j]);
							sendMessageToClient(tcpClients[i].ipAddress, tcpClients[i].port, reply);
						}
					}

				// On envoie enfin un message a tout le monde pour definir qui est le joueur courant=0
					sprintf(reply, "M %d", joueurCourant);
					broadcastMessage(reply);
					fsmServer=1;
				}
				break;
            }
		}
		else if (fsmServer==1){
			switch (buffer[0]){
				case 'G': //Mise en accusation 
				{
					int id, guiltsel;
					sscanf(buffer, "G %d %d", &id, &guiltsel);
					if (guiltsel == deck[12]){
						sprintf(reply, "F %s a gagné! Le coupable est %s", tcpClients[id].name, nomcartes[guiltsel]);
						broadcastMessage(reply);
						printf("%s a gagné! Le coupable est %s\n Fin du jeu\n", tcpClients[id].name, nomcartes[guiltsel]);
						return 0;
					}
					else{
						sprintf(reply, "Non, %s n'est pas le coupable, tu as perdu", nomcartes[guiltsel]);
						sendMessageToClient(tcpClients[id].ipAddress, tcpClients[id].port, reply);
						perdant[cpt]=id;
						cpt++;

						joueurCourant = (joueurCourant+1)%4;
						int i=0; 
						while (i < cpt){
							if (joueurCourant == perdant[i]){
								joueurCourant = (joueurCourant+1)%4;
								i=0;
							}
							else{
								i++;
							}
						}
						sprintf(reply, "M %d", joueurCourant);

						if (cpt == 3){
							sprintf(reply, "F %s a gagné! Le coupable est %s", tcpClients[joueurCourant].name, nomcartes[deck[12]]);
							broadcastMessage(reply);
							printf("%s a gagné! Le coupable est %s\n Fin du jeu\n", tcpClients[joueurCourant].name, nomcartes[guiltsel]);
							return 0;
						}

						broadcastMessage(reply);
					}
					
					break;
				}
				case 'O': //Qui avait des objets particuliers
				{
					int id, objsel;
					sscanf(buffer, "O %d %d", &id, &objsel);
					for (int i=0; i<4; i++){
						if (i!=id){
							if (tableCartes[i][objsel]==0){
								sprintf(reply, "V %d %d %d", i, objsel, tableCartes[i][objsel]);
								broadcastMessage(reply);
							}
							else
							{
								sprintf(reply, "V %d %d 100", i, objsel);
								for (int j=0; j<4; j++){
									if (j!=i)
										sendMessageToClient(tcpClients[j].ipAddress, tcpClients[j].port, reply);
								}
							}
						}
					}
					joueurCourant = (joueurCourant+1)%4;

					int i=0; 
					while (i < cpt){
						if (joueurCourant == perdant[i]){
							joueurCourant = (joueurCourant+1)%4;
							i=0;
						}
						else{
							i++;
						}
					}
					sprintf(reply, "M %d", joueurCourant);
					broadcastMessage(reply);
					break;
				}
				case 'S': //Un joueur particulier combien il a l'objet demandé
				{
					int id, joueursel, objetsel;
					sscanf(buffer, "S %d %d %d", &id, &joueursel, &objetsel);
					sprintf(reply, "V %d %d %d", joueursel, objetsel, tableCartes[joueursel][objetsel]);
					broadcastMessage(reply);
					// RAJOUTER DU CODE ICI
					joueurCourant = (joueurCourant+1)%4;
					int i=0; 
					while (i < cpt){
						if (joueurCourant == perdant[i]){
							joueurCourant = (joueurCourant+1)%4;
							i=0;
						}
						else{
							i++;
						}
					}
					sprintf(reply, "M %d", joueurCourant);
					broadcastMessage(reply);
					break;
				}
				default:
					break;
			}
		}
		close(newsockfd);
    }
	close(sockfd);
	return 0; 
}

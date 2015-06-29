#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>
#include <pthread.h>

#define portnum 60807

//STRUCS

pthread_mutex_t lock, lockm;
char broadIP[16];
char** broadContacts;
char msgMcast[256];
int multiCount;

typedef struct mensagem{
    
    char IP[16];
    char *Text;
    char timeinfo[20];
    int direction;          // 1=IN / 0=OUT
    struct mensagem *next;
    
} msg;

typedef struct cont{
    
    char IP[16];
    char *Nome;
    char timeinfo[20];
    int online;
    struct cont *next;
    
} contato;

void error(char *msg)
{
    perror(msg);
    exit(0);
}

typedef struct Sera{
	char* hostname;

	char *message; 
}cliAdr;


//VARIAVEIS GLOBAIS

static int i =0;
static msg * messageList = NULL;
static contato * contactList = NULL;
static int ON = 1;


//FUNÇÕES DE INICIALIZAÇÃO

msg* initMsg( char *aText, char *aIp, int dir){
    
    time_t rawtime;
    time ( &rawtime );
    struct tm  *timeinfo = localtime (&rawtime);
    
    msg *aux = (msg*)malloc(sizeof(msg));
    
    strftime(aux->timeinfo, sizeof(aux->timeinfo)-1, "%d.%m.%y_%H:%M:%S", timeinfo);
    
    aux->Text = (char*)malloc(strlen(aText)*sizeof(char));
    aux->next = NULL;
    aux->direction = dir;
    strcpy(aux->Text, aText);
    strcpy(aux->IP, aIp);
    
    return aux;
    
}

contato* initContact( char *aName, char *aIp){
    
    time_t rawtime;
    time ( &rawtime );
    struct tm  *timeinfo = localtime (&rawtime);
    
    contato *aux = (contato*)malloc(sizeof(contato));
    
    strftime(aux->timeinfo, sizeof(aux->timeinfo)-1, "%d.%m.%y_%H:%M:%S", timeinfo);
    
    aux->Nome = (char*)malloc(strlen(aName)*sizeof(char));
    strcpy(aux->Nome, aName);
    strcpy(aux->IP, aIp);
    aux->next = NULL;
    aux->online = 0;
    
    return aux;
    
}


//THREADS SERVIDOR

void* messageTreament(void* A){
    
    char buffer[256];
    int *newsockfd;
    int n;
    char buf[INET_ADDRSTRLEN] = "";
    struct sockaddr_in name;
    socklen_t len = sizeof(name);
     
    newsockfd = (int*)A;                                                    //Trata parametro de input da thread (porta da conexao)
    
    if (getpeername(*newsockfd, (struct sockaddr *)&name, &len) != 0) {     //Obtem IP da origem da mensagem e salva em buf
        perror("getpeername");
    } else {
        inet_ntop(AF_INET, &name.sin_addr, buf, sizeof( buf));
    }
    
    bzero(buffer,256);
    n = read(*newsockfd,buffer,255);                                        //Le a mensagem do socket
    if (n < 0) error("ERROR reading from socket");
    
    if(strcmp(buffer, "") != 0 ){       //Só continua se buffer conter uma mensagem
    
    msg *a;                             //Cria novo no com a mensagem
    a = initMsg(buffer,buf, 1);
    pthread_mutex_lock(&lockm);

    msg *it;                            //Variavel auxiliar para percorrer lista encadeada
    it = messageList;
    
    while(it->next != NULL){            //Percorre lista ate encontrar ultima posiçao
        it = it->next;
    }
    
    it->next = a;                       //Coloca mensagem na lista encadeada
    pthread_mutex_unlock(&lockm);
    close(*newsockfd);                  //Fecha socket e a thread
    
    }
    
    pthread_exit(0);
    
}

void* listener(){
    
    pthread_t T_messageTreament;
    void* ret = NULL;
    int sockfd, portno, clilen;
    int newsockfd;
    struct sockaddr_in serv_addr; //contem um endereço
    struct sockaddr_in cli_addr;
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);      //abre socket
    
    if (sockfd < 0){ 
        error("ERROR opening socket");}
        
        bzero((char *) &serv_addr, sizeof(serv_addr)); //zera buffer
        
        portno = portnum;                                 //Porta de escuta padrão do programa
        
        serv_addr.sin_family = AF_INET;                 //Prepara argumentos para o bind
        serv_addr.sin_port = htons(portno);
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        
        if (bind(sockfd, (struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0){
            error("ERROR on binding");}
            
            while(ON){                     //Escuta enqunto thread main não dizer o contrario
                
                listen(sockfd,5);
                
                clilen = sizeof(cli_addr);
                newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);        //recebe conexao
                
                if (newsockfd < 0){
                    error("ERROR on accept");}
                    
                    pthread_create(&T_messageTreament, 0, (void *) messageTreament, (void*) &newsockfd);       //dispara thread para tratar mensagem
            }
            
            pthread_exit(0);
            
}



//THREAD CLIENTE

void* clientThread(void* A)
{
    //incialização das variaveis
    fflush(stdin);
    int sockfd, portno, n;
    int* result;                        //retorna estado da thread
    char buffer[256];
    struct sockaddr_in serv_addr;
    struct hostent *server;
    
    //seleção da porta
    portno = portnum;
    result = (int*)malloc(sizeof(int));
    
    //passagem de parametros para a thread
    cliAdr* cliadr = (cliAdr*) A; 

    sockfd = socket(AF_INET, SOCK_STREAM, 0);  //estabelece o file descriptor que sera usado para o cliente estabelecer conexoes
    if (sockfd < 0){
        *result = 0;
        pthread_exit((void*)result);}
    
    server = gethostbyname(cliadr->hostname);  //pega o ip do servidor, com base no seu nome

    if (server == NULL) {

        *result = 0;
        pthread_exit((void*)result);}

    bzero((char *) &serv_addr, sizeof(serv_addr));  //zera os buffers 
    serv_addr.sin_family = AF_INET; //nesse paso, determinamos o tipo da socket, nao para comunicação interna à maquina, mas comunicacao via rede
    bcopy((char *)server->h_addr,(char *)&serv_addr.sin_addr.s_addr,server->h_length); //copia o tamanho do endereço do servidor para h_addr
    serv_addr.sin_port = htons(portno); //faz a conversao big-endian/little-endian
    
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0){ //faz a conexão com o serv
        *result = 0;
        pthread_exit((void*)result);}
    
    bzero(buffer,256);
    
    //passagem da mensagem para o buffer
    strcpy(buffer, cliadr->message);
    
    n = write(sockfd,buffer,strlen(buffer));
    if (n < 0){
        *result = 0;
        pthread_exit((void*)result);} 
    
    free(cliadr->hostname);
    free(cliadr->message);
    free(cliadr);
    
    bzero(buffer,256);
    
    n = read(sockfd,buffer,strlen(buffer));
    
    *result = 1;
	fflush(0);
    pthread_exit((void*)result);
}



//THREAD PING

void* ping(){
    
    fflush(stdin);
    contato *it2;
    int sockfd, portno, n;
    int result;
    char buffer[256];
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buf[INET_ADDRSTRLEN] = "";
    
    int a;
    
    portno = portnum;
    
    while(ON){
        
        sleep(2);
        
        it2 = contactList;
        
        if(it2->next != NULL){
            
            it2 = it2->next;        //pula cabeça
            
            while(it2->next != NULL){
                
                sockfd = socket(AF_INET, SOCK_STREAM, 0);
                
                if (sockfd < 0){
                    printf("ERRO: SOCKFD");
                    fflush(0);
                }
                
                server = gethostbyname(it2->IP);
                
                if (server == NULL) {
                    printf("ERRO: SERVER");
                    fflush(0);
                }
                
                bzero((char *) &serv_addr, sizeof(serv_addr));
                serv_addr.sin_family = AF_INET;
                bcopy((char *)server->h_addr,(char *)&serv_addr.sin_addr.s_addr,server->h_length);
                serv_addr.sin_port = htons(portno);
                
                inet_ntop(AF_INET, &serv_addr.sin_addr, buf, sizeof( buf));
                
                a = connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
                
                if(a == 0){
                    it2->online =1;}else{
                        it2->online = 0;}
                        
                        it2 = it2->next;
                        
                        close(sockfd);}
                        
                        sockfd = socket(AF_INET, SOCK_STREAM, 0);
                        
                        server = gethostbyname(it2->IP);
                        bzero((char *) &serv_addr, sizeof(serv_addr));
                        serv_addr.sin_family = AF_INET;
                        bcopy((char *)server->h_addr,(char *)&serv_addr.sin_addr.s_addr,server->h_length);
                        serv_addr.sin_port = htons(portno);
                        
                        inet_ntop(AF_INET, &serv_addr.sin_addr, buf, sizeof( buf));
                        
                        a = connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
                        
                        if(a == 0){
                            it2->online =1;}else{
                                it2->online = 0;}
                                close(sockfd);
                                
        }
    }
    pthread_exit(0);
}

void* multicast2(){
    int i;
    
    //salva variaveis localmente, porque podem ser alteradas por serem globais
    char** saveplease = malloc(multiCount*16);
    for(i = 0; i<multiCount; i++)
        saveplease[i] = malloc(16);
    for(i = 0; i<multiCount; i++)
        strcpy(saveplease[i],broadContacts[i]);
    
    //op mantem o loop rodando
    int op = 1;
    fflush(stdin);
    contato *it2;
    int sockfd, portno, n;
    int result;
    char buffer[256];
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buf[INET_ADDRSTRLEN] = "";
    
    int a;
    
    portno = portnum;
    int iterador = 0;
    while(op){
        printf("1\n");
        fflush(0);
        
        //it2 recebe a cabeça do contact list
        it2 = contactList;
        
        if(it2->next != NULL){
            
            fflush(0);
            it2 = it2->next;        //pula cabeça
            
            while(it2->next != NULL){
                
                fflush(0);
                //cria socket
                sockfd = socket(AF_INET, SOCK_STREAM, 0);
                
                fflush(0);
                if (sockfd < 0){
                    printf("ERRO: SOCKFD");
                    fflush(0);
                }
                
                fflush(0);
                //pega o IP do servidor
                server = gethostbyname(it2->IP);
                
                if (server == NULL) {
                    printf("ERRO: SERVER");
                    fflush(0);
                }
                
                bzero((char *) &serv_addr, sizeof(serv_addr));
                serv_addr.sin_family = AF_INET;
                bcopy((char *)server->h_addr,(char *)&serv_addr.sin_addr.s_addr,server->h_length);
                serv_addr.sin_port = htons(portno);
                
                inet_ntop(AF_INET, &serv_addr.sin_addr, buf, sizeof( buf));
                
                //faz a conexão
                a = connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
                
                //se o contato estiver na lista de multicast, a mensagem eh mandada pra ele
                for(i=0; i<multiCount; i++){
                    if(strcmp(it2->IP,saveplease[i]) == 0){
                        write(sockfd,msgMcast,strlen(msgMcast));
                    }
                }
                
                
                if(a == 0){
                    it2->online =1;}else{
                        it2->online = 0;}
                        
                        it2 = it2->next;
                        
                        close(sockfd);
            }
            //Abaixo se repete o feito acima, mas a ultima interação, isso ocorre para nunca cairmos no null do ultimo elo da lista encadeada
            
            fflush(0);
            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            
            server = gethostbyname(it2->IP);
            bzero((char *) &serv_addr, sizeof(serv_addr));
            serv_addr.sin_family = AF_INET;
            bcopy((char *)server->h_addr,(char *)&serv_addr.sin_addr.s_addr,server->h_length);
            serv_addr.sin_port = htons(portno);
            
            inet_ntop(AF_INET, &serv_addr.sin_addr, buf, sizeof( buf));
            
            a = connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
            
            for(i=0; i<multiCount; i++){
                if(strcmp(it2->IP,saveplease[i]) == 0){
                    write(sockfd,msgMcast,strlen(msgMcast));
                    
                }
            }
            op = 0; 
            
            
            if(a == 0){
                it2->online =1;}else{
                    it2->online = 0;}
                    close(sockfd);
                    
        }
    }
    pthread_exit(0);
}

//THREADS E FUNÇÕES PRINCIPAIS



void printSplash(){
             
         
    system("clear");
    
    printf("\n\n\n\n\n\n##          ###    ##    ## ##     ## ########  ######   ######     ###    ##    ##  ######   ######## ########  \n##         ## ##   ###   ## ###   ### ##       ##    ## ##    ##   ## ##   ###   ## ##    ##  ##       ##     ## \n##        ##   ##  ####  ## #### #### ##       ##       ##        ##   ##  ####  ## ##        ##       ##     ## \n##       ##     ## ## ## ## ## ### ## ######    ######   ######  ##     ## ## ## ## ##   #### ######   ########  \n##       ######### ##  #### ##     ## ##             ##       ## ######### ##  #### ##    ##  ##       ##   ##   \n##       ##     ## ##   ### ##     ## ##       ##    ## ##    ## ##     ## ##   ### ##    ##  ##       ##    ##  \n######## ##     ## ##    ## ##     ## ########  ######   ######  ##     ## ##    ##  ######   ######## ##     ## \n\n\n\n");
    
    sleep(1);
    system("clear");
    
}

void printMenu(){
    printf("\n\n\n\n.-------------------------MenuPrincipal------------------------.\n");
    printf("|                                                              |\n");
    printf("|1) Adicionar Contatos                                         |\n");
    printf("|                                                              |\n");
    printf("|2) Listar Contatos                                            |\n");
    printf("|                                                              |\n");
    printf("|3) Excluir Contatos                                           |\n");
    printf("|                                                              |\n");
    printf("|4) Enviar Mensagem                                            |\n");
    printf("|                                                              |\n");
    printf("|5) Mensagem em Grupo                                          |\n");
    printf("|                                                              |\n");
    printf("|6) Listar todas mensagens                                     |\n");
    printf("|                                                              |\n");
    printf("|7) Sair                                                       |\n");
    printf("|                                                              |\n");
    printf("º-------------------------MenuPrincipal-----------------v0.1---º\n");
    printf("Entre com o comando: ");
    
    
}

int isValidIpAddress(char *ipAddress)
{
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, ipAddress, &(sa.sin_addr));
    return result != 0;
}

int contactPrint(){
    
    contato *it3;
    it3 = contactList;  //Variavel auxiliar para percorrer lista de contatos
    int k = 1; 
    
    if(it3->next != NULL){      //checa se lista esta vazia
        
        printf("Online: \n");
        it3 = it3->next;
        while(it3->next != NULL){

            if (it3->online == 1){
                printf("%d) %s    %s     \nAdicionado em: %s\n\n",k, it3->IP, it3->Nome, it3->timeinfo);    
            }
            it3 = it3->next;
            k++;
        }

        if (it3->online == 1){
        printf("%d) %s    %s     \nAdicionado em: %s\n", k, it3->IP, it3->Nome, it3->timeinfo);
        }

    }
    
    
    it3 = contactList;  //Variavel auxiliar para percorrer lista de contatos
    k = 1; 
    printf("\n\n");
    
    if(it3->next != NULL){      //checa se lista esta vazia
        
        printf("Offline: \n");
        it3 = it3->next;
        while(it3->next != NULL){

            if (it3->online == 0){
                printf("%d) %s    %s     \nAdicionado em: %s\n\n",k, it3->IP, it3->Nome, it3->timeinfo);    
            }
            it3 = it3->next;
            k++;
        }

        if (it3->online == 0){
        printf("%d) %s    %s     \nAdicionado em: %s\n", k, it3->IP, it3->Nome, it3->timeinfo);
        }
        return 1;       //retorna 1 se lista tem elementos
    }
    
    
    
    
    return 0;       //retorna 0 se vazia
}

void freeEverything(){
    
    contato *it, *auxf;
    it = contactList;               //Variavel auxiliar para percorrer lista
    

    while(it->next != NULL){
    auxf = it;
    it = it->next;
    free(auxf->Nome);
    free(auxf);
    }

    free(it->Nome);
    free(it);
    
    msg *it2, *auxf2;
    it2 = messageList;
    
    while (it->next != NULL){
    auxf2 = it2;
    it2 = it2->next;
    free(auxf2->Text);
    free(auxf2);
    }
    
    free(it2->Text);
    free(it2);

}

contato* searchTouch(char *serIP){ //Nota-se que touch é contato em inglês
	contato *it23;
	int count = 0;
	if(contactList->next == NULL){		
		return NULL;	
	}
	for(it23 = contactList; it23->next!=NULL; it23 = it23->next){
		count++;
		if(strcmp(serIP, it23->next->IP) == 0){
			return it23;		
		}
	}
	return NULL;
}

void deleteTouch(char *delIP){
    contato * aux23, *aux33;
    aux23 = searchTouch(delIP);
    if(aux23 == NULL){
        printf("Contato não achado!\n");
        fflush(0);
        sleep(2);	
    }
    else{
        aux33 = aux23->next;
        aux23->next = aux33->next;
        aux33->next = NULL;
        free(aux33);
        printf("Contato deletado com sucesso");
        fflush(0);		
        sleep(2);
    }
}

void printNoBroadContacts(char** broadContacts, int size){
	int i, pegou = 0;
	contato *it;
	for(it = contactList; it->next!=NULL; it = it->next){
		for(i = 0; i < size; i++){
			//getchar();
			if(   strcmp(it->next->IP, broadContacts[i]) == 0   )
			{pegou = 1;}
		}
	if(pegou==0){printf("\n%s    %s     \n", it->next->IP, it->next->Nome/*, it->next->timeinfo*/);}	
	pegou = 0;	
	}
}

void printBroadContacts(char** broadContacts, int size){
	int i, pegou = 0;
	contato *it;
	for(it = contactList; it->next!=NULL; it = it->next){
		for(i = 0; i < size; i++){
			//getchar();
			if(   strcmp(it->next->IP, broadContacts[i]) == 0   )
			{pegou = 1;}
		}
	if(pegou==1){printf("\n%s    %s     \n", it->next->IP, it->next->Nome/*, it->next->timeinfo*/);}	
	pegou = 0;	
	}


}

contato* getIpByNumber(int k){

	contato *it24;
	int count = 0;
	for(it24 = contactList; it24->next!=NULL; it24 = it24->next){
		count++;

		if(k == count){
			return it24->next;		
		}
	}
	printf("Numero não presente na lista de contatos");
        fflush(0);
	return NULL;
}

int printChat(char *aIP ){
    
    msg *it2;
    
    it2 = messageList;
    
    if(it2->next != NULL){
        
        it2 = it2->next;
        
        while(it2->next != NULL){
            
            if(strcmp( aIP , it2->IP) == 0){
                
                if (it2->direction == 1){
                    printf("%s\n",it2->timeinfo);
                    printf("%s     %s\n", it2->IP, it2->Text);
                    
                }else{
                    printf("%s\n      Voce     %s\n",it2->timeinfo, it2->Text);
                }
            }
            
            it2 = it2->next;
        }
        
    
    if(strcmp( aIP , it2->IP) == 0){
        
        if (it2->direction == 1){
            printf("%s\n",it2->timeinfo);
            printf("%s     %s\n", it2->IP, it2->Text);
            
        }else{
            printf("%s\n      Voce     %s\n",it2->timeinfo, it2->Text);
        }
    }
    return 1;
    
        
    }
return 0;

}

int printMessages(){
    
    msg *it2;
    fflush(0);
    it2 = messageList;
    
    if(it2->next != NULL){
        
        it2 = it2->next;
        
        while(it2->next != NULL){
            if(it2->direction == 1){
                printf("%s\n",it2->timeinfo);
                printf("%s     %s\n", it2->IP, it2->Text);
                
            }

            it2 = it2->next;
            
            
        }
    	fflush(0);

            if(it2->direction == 1){
                printf("%s\n",it2->timeinfo);
                printf("%s     %s\n", it2->IP, it2->Text);
                
            }
                
        return 1;
        
    }
    return 0;
    
}


int main(int argc, char *argv[])
{
    broadContacts = (char**) malloc(100*sizeof(char*));
    char aux[256];
    char aux2[16];
    int aux13;
    void *status;
    int option;
    
    messageList = initMsg("HEAD","-1", -1);
    
    contactList = initContact("HEAD","-1");
    
    pthread_t T_Listener; 
    pthread_t T_Sender;
    pthread_t T_Ping;
    pthread_t T_multicast2;
    
    pthread_create(&T_Listener, 0, (void *) listener, (void*) 0);
    pthread_create(&T_Ping, 0, (void *) ping, (void*) 0);
    
    printSplash();
    
    while(ON){
        
        system("clear");
        printMenu();
        
        scanf("%d", &option);
        int aux13, aux15, aux25 = 1;
        int count15 = 0;
        
        for(aux15=0; aux15<100; aux15++){
            broadContacts[aux15] = (char*)malloc(16);	
        }
        
        if (option <= 0 || option >= 8){
            
            printf("Comando invalido!\n");
            sleep(1);
            
        }else{
            
            
            switch (option){
                case 1:
                    
                    system("clear");
                    bzero(aux,256);
                    bzero(aux2,16);
                    
                    printf("\n\n\n\n-------------------------AdicionarContato---------------------\n");
                    
                    printf("Nome do contato: ");
                    fflush(stdin);
                    getchar();
                    fgets(aux, 255, stdin);
                    printf("IP do contato: ");
                    fflush(stdin);
                    scanf("%s", aux2);
                    
                    if(isValidIpAddress(aux2)){         //so procegue se IP for valido
                        
                        if(searchTouch(aux2) == NULL){
                            
                            contato *a; 
                            a = initContact(aux,aux2);      //inicializa novo contato
                            
                            contato *it;
                            pthread_mutex_lock(&lock);
                            it = contactList;               //Variavel auxiliar para percorrer lista
                            
                            while(it->next != NULL){
                                it = it->next;
                            }
                            
                            it->next = a;                   //salva novo contato ao final da lista
                            
                            printf("\n\nContato adicionado!");  //imprime confirmaçao
                            printf("\n\nNome: %s\nIP: %s", a->Nome, a->IP);
                            fflush(0);
                            pthread_mutex_unlock(&lock);
                            
                        }else{
                            
                            printf("Contato com este IP ja existe!\n");
                            
                        }
                    }else{
                        
                        printf("IP invalido\n");
                        fflush(0);
                        
                    }
                    sleep(2);
                    break;
                    
                    case 2:
                        
                        system("clear");
                        
                        printf("\n\n\n\n-------------------------ListaDeContatos---------------------\n");
                        
                        printf("\n\n");
                        
                        if(contactPrint()){
                            printf("\nDigite qualquer tecla para voltar");
                            getchar();
                            getchar();
                        }else{
                            printf("Nao ha contatos!");
                            fflush(0);
                            sleep(2);
                        }  
                        
                        break;
                    case 3:
                        
                        system("clear");
                        printf("\n\n\n\n-------------------------DeletarContatos---------------------\n");
                        printf("\n\n\n\nAperte: \n1) se desejar listar os contatos para escolher qual excluir\n2) Se você ja sabe o IP do contato a ser excluido\nOutra tecla para voltar\n");
                        scanf("%d", &aux13);
                        if(aux13 == 1){
                            system("clear");
                            
                            printf("\n\n\n\n-------------------------ListaDeContatos---------------------\n");
                            if(contactPrint()){
                                
                            }else{
                                printf("Nao ha contatos!");
                                fflush(0);
                                sleep(2);
                            }
                        }
                        if(aux13 != 1 && aux13 != 2){
                            printf("Saindo...\n");
                            sleep(1);
                            break;	
                        }
                        if(aux13 == 2)
                        {system("clear");}
                        printf("\nDigite o IP do contato a ser excluido: ");
                        char auxIP3[16];
                        
                        scanf("%s", auxIP3);
                        deleteTouch(auxIP3);
                        break;
                        
                    case 4:
                        
                        system("clear");
                        
                        printf("\n\n\n\n-------------------------EnviarMensagem---------------------\n");
                        bzero(aux,256);
                        bzero(aux2,16);
                        
                        if(contactPrint()){
                            printf("\nSelecione um Contato: ");
                            scanf("%d", &aux13);
                            
                            system("clear");
                            printf("\n\n\n\n-------------------------EnviarMensagem---------------------\n");
                            
                            
                            if(getIpByNumber(aux13)!=NULL){
                                
                                printf("\nSua conversa com %s (%s)\n\n", getIpByNumber(aux13)->Nome, getIpByNumber(aux13)->IP);
                                
                                strcpy(aux2,getIpByNumber(aux13)->IP);
                                if(printChat(aux2)){
                                    
                                }else{
                                    printf("\nAinda nao ha mensagens para esse contato\n\n");
                                }
                                
                            }
                            
                            printf("Digite sua mensagem: \n");
                            //fflush(0);
                            getchar();
                            fgets(aux, 255, stdin);
                            
                            cliAdr* msg2go = malloc(sizeof(cliAdr));
                            
                            msg2go->hostname = (char*)malloc(16*sizeof(char));
                            msg2go->message = (char*)malloc(256*sizeof(char));
                            
                            strcpy(msg2go->hostname,aux2);
                            
                            strcpy(msg2go->message,aux);
                            
                            pthread_create( &T_Sender, NULL, clientThread, (void*) msg2go);
                            
                            pthread_join(T_Sender, &status); 
                            
                            if ( *(int*)status == 1){
                                printf("\n\nMensagem enviada com sucesso!");
                                fflush(0);
                                
                                msg *a;                             //Cria novo no com a mensagem
                                a = initMsg(aux,aux2, 0);
                                
                                pthread_mutex_lock(&lockm);
                                msg *it;                            //Variavel auxiliar para percorrer lista encadeada
                                it = messageList;
                                
                                while(it->next != NULL){            //Percorre lista ate encontrar ultima posiçao
                                    it = it->next;
                                }
                                
                                it->next = a;   
                                pthread_mutex_unlock(&lockm);                    //Coloca mensagem na lista encadeada                    
                                sleep(1);
                                
                            }else{
                                printf("\n\nErro ao enviar mensagem, contato offline");
                                fflush(0);
                                sleep(2);
                            }
                            
                        }else{                
                            
                            
                            printf("Adicione contatos antes!");
                            fflush(0);
                            sleep(2);
                        }
                        break;
                        
                        case 5:
                            system("clear");
                            
                            if(contactPrint()){
                                sleep(1);
                            }else{
                                printf("Nao ha contatos!");
                                fflush(0);
                                sleep(2);
                                break;                
                            }
                            while(aux25 != -1){	
                                fflush(0);
                                printf("\n\n\nDigite o IP de um contato que deseja incluir na mensagem de grupo\n\n");
                                //broadNumber foi declarado anteriormente, se copiar essa parte lembre de clarar		
                                scanf("%s", broadIP);
                                strcpy(broadContacts[count15++],broadIP);
                                system("clear");
                                printf("\nContatos que podem ser adicionados ao multicast:\n");
                                fflush(0);			
                                printNoBroadContacts(broadContacts, count15);
                                fflush(0);			
                                printf("\n\nContatos adicionados no Broadcast: \n");
                                fflush(0);
                                printBroadContacts(broadContacts, count15);
                                fflush(0);
                                printf("\nAperte -1 se ja selecionou os contatos desejados, outra tecla para continuar");
                                scanf("%d", &aux25);
                                
                            }
                            for(i = 0; i<count15; i++)
                                printf("broadContacts[%d]: %s\n", i, broadContacts[i]);
                            sleep(1);
                            fflush(stdin);
                            printf("\n\nDigite a mensagem a ser enviada via multicast:\n");
                            getchar();
                            fgets(msgMcast, 255, stdin);
                            //funcao de multicast(broadIPs, size, msgmulticast)
                            multiCount = count15;
                            pthread_create(&T_multicast2, 0, (void *) multicast2, (void*) 0);
                            break;
                            
                        case 7:
                            
                            ON = 0;
                            freeEverything();
                            exit(0);
                            
                        case 6:
                            system("clear");
                            printf("\n\n\n\n-------------------------TodasMensagens---------------------\n");
                            
                            if(printMessages()){
                                
                                printf("\nDigite qualquer tecla para voltar");
                                getchar();
                                getchar();
                                
                            }else{
                                printf("\nNão há mensagens!\n\n");
                                sleep(2);
                                break;
                            }
                            
            }
            
        }
        
        
    }
    
    return 0;
    
}


       

     
     
     
     

     

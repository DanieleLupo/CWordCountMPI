#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include "/usr/include/mpi/mpi.h"

#define MAX_SIZE_PAROLA 100
#define SIZEBUFFER 100000000
#define MAX_PAROLE 100000000

struct word{
	char *parola;
	int occorrenza;
};

struct file{
	char *nome;
	int assegnate;
	int rimanenti;
};


MPI_Status status;
int myrank;
int indiceFiles = 0;
DIR *dir;
struct dirent *ent;
char *filePath;
struct file *files;
int fileSalvati = 0;
int paroleTotali = 0;
int p;
double start, end;
FILE *f;
char *parola;
int parolePerSlave = 0, resto = 0, paroleSlave = 0, len = 0;
int position = 0, saltare = 0, contare = 0;

char *buffer;
struct word *parole;
int sizeParole = 0;
int trovato = 0;

int main(int argc, char **argv){

	if(argc != 3){
		printf("[ERRORE] Utilizzo: %s <Cartella> <Numero di file>\n", argv[0]);
		return -1;
	}

	MPI_Status status;
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD,&myrank);

	buffer = (char*) malloc(sizeof(char*) * SIZEBUFFER);
	parola = malloc(sizeof(char*) * MAX_SIZE_PAROLA);

	if(myrank == 0){
#ifdef MASTER
		printf("DEBUG MASTER: Inizio codice master\n");
#endif

		files = malloc(sizeof(struct file) * atoi(argv[2]));
		fileSalvati = 0;

		paroleTotali = 0;

		MPI_Comm_size(MPI_COMM_WORLD, &p);

		if((dir = opendir(argv[1])) != NULL){
			while((fileSalvati < atoi(argv[2])) && ((ent = readdir(dir)) != NULL)){
				if(strcmp(ent->d_name,".") == 0 || strcmp(ent->d_name,"..") == 0) continue;
				filePath = (char*) malloc(sizeof(char*) * (strlen(argv[1]) + strlen(ent->d_name) + 1));
				sprintf(filePath, "%s%s", argv[1], ent->d_name);
				if((f = fopen(filePath, "r")) == NULL){
					printf("[ERRORE]: Impossibile aprire il file %s\n", filePath);
					MPI_Finalize();
					return -2;
				}
				files[fileSalvati].nome = (char*) malloc(sizeof(char*) * strlen(filePath) + 1);
				strcpy(files[fileSalvati].nome, filePath);
				files[fileSalvati].rimanenti = 0;
				parola[0] = '\0';
				while(EOF != (fscanf(f,"%s", parola))){
					files[fileSalvati].rimanenti++;
					parola[0] = '\0';
				}
				paroleTotali += files[fileSalvati].rimanenti;
				files[fileSalvati].assegnate = 0;
				fileSalvati++;
				filePath[0] = '\0';
				fclose(f);
			}
		} // Fine lettura file
		closedir(dir);


#ifdef DEBUG
		printf("DEBUG: Ho letto %d file su %d. Ci sono %d parole.\n", fileSalvati, atoi(argv[2]), paroleTotali);
#endif

		parole = (struct word*) malloc(sizeof(struct word*) * paroleTotali);
		sizeParole = 0;

		parolePerSlave = paroleTotali / p;
		resto = paroleTotali % p;

		indiceFiles = 0;

		paroleSlave = parolePerSlave;
		if(resto > 0){
			paroleSlave++;
			resto--;
		}
		//printf("Il MASTER deve computare %d parole su %d\n", paroleSlave, paroleTotali);
		start = MPI_Wtime();
		while(paroleSlave > 0 && indiceFiles < fileSalvati){
			f = fopen(files[indiceFiles].nome, "r");
			parola[0] = '\0';
			while(paroleSlave > 0 && (fscanf(f, "%s", parola)) != EOF){
				trovato = 0;
				for(int i = 0; i < sizeParole; i++){
					if(strcmp(parola, parole[i].parola) == 0){
						trovato = 1;
						parole[i].occorrenza++;
						break;
					}
				}
				if(trovato == 0){
					parole[sizeParole].parola = (char*) malloc(sizeof(char*) * strlen(parola) + 1);
					strcpy(parole[sizeParole].parola, parola);
					parole[sizeParole].occorrenza = 1;
					sizeParole++;
				}
				parola[0] = '\0';
				files[indiceFiles].rimanenti--;
				files[indiceFiles].assegnate++;
				if(files[indiceFiles].rimanenti == 0){
					indiceFiles++;
				}
				paroleSlave--;
				if(paroleSlave == 0) break;
			}
			fclose(f);
		}

#ifdef PROVA
		for(int i = 0; i < sizeParole; i++){
			printf("%s %d\n", parole[i].parola, parole[i].occorrenza);
		}
#endif

		indiceFiles = 0;

		for(int i = 1; i < p; i++){
			paroleSlave = parolePerSlave;
			if(resto > 0){
				paroleSlave++;
				resto--;
			}
			position = 0;
			buffer[0] = '\0';
			while(paroleSlave > 0){
				len = strlen(files[indiceFiles].nome);
				len++;
				MPI_Pack(&len, 1, MPI_INT, buffer, SIZEBUFFER, &position, MPI_COMM_WORLD);
				MPI_Pack(files[indiceFiles].nome, len, MPI_CHAR, buffer, SIZEBUFFER, &position, MPI_COMM_WORLD);
				saltare = files[indiceFiles].assegnate;
				MPI_Pack(&saltare, 1, MPI_INT, buffer, SIZEBUFFER, &position, MPI_COMM_WORLD);
				if(paroleSlave > files[indiceFiles].rimanenti){
					contare = files[indiceFiles].rimanenti;
					files[indiceFiles].rimanenti = 0;
					indiceFiles++;
				}else{
					contare = paroleSlave;
					files[indiceFiles].assegnate += contare;
					files[indiceFiles].rimanenti -= contare;
				}
				MPI_Pack(&contare, 1, MPI_INT, buffer, SIZEBUFFER, &position, MPI_COMM_WORLD);
				paroleSlave -= contare;
				MPI_Pack(&paroleSlave, 1, MPI_INT, buffer, SIZEBUFFER, &position, MPI_COMM_WORLD);
			}  
			// Invio il pacchetto allo slave
			MPI_Send(buffer, SIZEBUFFER, MPI_PACKED, i, 0, MPI_COMM_WORLD);

		}// Fine invio slave

		// Inizio ricezione dagli slave

		len = 0;
		parola[0] = '\0';
		int o = 0;
		int par = 0;
		trovato = 0;

		for(int i = 1; i < p; i++){
			buffer[0] = '\0';
			position = 0;
			MPI_Recv(buffer, SIZEBUFFER, MPI_PACKED, i, 0, MPI_COMM_WORLD, &status);
			//printf("Ricevo il pacchetto dallo slave #%d\n", i);
			do{
				MPI_Unpack(buffer, SIZEBUFFER, &position, &len, 1, MPI_INT, MPI_COMM_WORLD);
			//printf("Ricevo il pacchetto dallo slave #%d len %d\n", i, len);

				parola = (char*) malloc(sizeof(char*) * len);
				MPI_Unpack(buffer, SIZEBUFFER, &position, parola, len, MPI_CHAR, MPI_COMM_WORLD);
			//printf("Ricevo il pacchetto dallo slave #%d parola %s\n", i, parola);

				MPI_Unpack(buffer, SIZEBUFFER, &position, &o, 1, MPI_INT, MPI_COMM_WORLD);


				for(int j = 0; j < sizeParole; j++){
					trovato = 0;
					if(strcmp(parola, parole[j].parola) == 0){
						parole[j].occorrenza+=o;
						trovato = 1;
						break;
					}
				}
				if(trovato == 0){
					parole[sizeParole].parola = (char*) malloc(sizeof(char*) * (strlen(parola)+1));
					strcpy(parole[sizeParole].parola, parola);
					parole[sizeParole].occorrenza = o;
					sizeParole++;
				}
				parola[0] = '\0';
				MPI_Unpack(buffer, SIZEBUFFER, &position, &par, 1, MPI_INT, MPI_COMM_WORLD);
			//printf("Ricevo il pacchetto dallo slave #%d\n par %d\n", i, par);

			}while(par > 0);
		}
		
		end = MPI_Wtime();

	printf("Tempo impiegato: %f\n", (end-start));
	
#ifdef TOTALE
		for(int i = 0; i < sizeParole; i++){
			printf("%s %d\n", parole[i].parola, parole[i].occorrenza);
		}
#endif
	}else{ // Fine MASTER

		len = 0;
		saltare = 0;
		contare = 0;
		paroleSlave = 0;
		trovato = 0;

		parole = (struct word*) malloc(sizeof(struct word*) * MAX_PAROLE);
		buffer = (char*) malloc(sizeof(char*) * SIZEBUFFER);

		position = 0;

		filePath = (char*) malloc(sizeof(char*) * MAX_SIZE_PAROLA);



		MPI_Recv(buffer, SIZEBUFFER, MPI_PACKED, 0, 0, MPI_COMM_WORLD, &status);
		do{
			MPI_Unpack(buffer, SIZEBUFFER, &position, &len, 1, MPI_INT, MPI_COMM_WORLD);
			filePath[0] = '\0';
			MPI_Unpack(buffer, SIZEBUFFER, &position, filePath, len, MPI_CHAR, MPI_COMM_WORLD);
			if((f = fopen(filePath, "r")) == NULL){
				printf("SLAVE %d: Impossibile leggere il file %s.\n", myrank, filePath);
				MPI_Finalize();
				return -2;
			}
			MPI_Unpack(buffer, SIZEBUFFER, &position, &saltare, 1, MPI_INT, MPI_COMM_WORLD);
			MPI_Unpack(buffer, SIZEBUFFER, &position, &contare, 1, MPI_INT, MPI_COMM_WORLD);
			while(contare > 0){
				fscanf(f,"%s", parola);
				if(saltare > 0){
					saltare--;
					continue;
				}
				trovato = 0;
				for(int j = 0; j < sizeParole; j++){
					if(strcmp(parola, parole[j].parola) == 0){
						trovato = 1;
						parole[j].occorrenza++;
						break;
					}
				}
				if(trovato == 0){
					parole[sizeParole].parola = malloc(sizeof(char*) * (strlen(parola)+1));
					strcpy(parole[sizeParole].parola, parola);
					parole[sizeParole].occorrenza = 1;
					sizeParole++;
				}
				contare--;
				parola[0] = '\0';
			}
			fclose(f);
			MPI_Unpack(buffer, SIZEBUFFER, &position, &paroleSlave, 1, MPI_INT, MPI_COMM_WORLD);
			filePath[0] = '\0';

		}while(paroleSlave > 0);

		buffer[0] = '\0';
		char *b = malloc(sizeof(char*) * SIZEBUFFER);
		position = 0;
		len = 0;
		int o = 0;
		int par = 0;
		//printf("Sizeparole %d\n", sizeParole);
		for(int i = 0; i < sizeParole; i++){
			len = strlen(parole[i].parola)+1;
			MPI_Pack(&len, 1, MPI_INT,b, SIZEBUFFER, &position, MPI_COMM_WORLD);
			MPI_Pack(parole[i].parola, len, MPI_CHAR,b, SIZEBUFFER, &position, MPI_COMM_WORLD);
			o = parole[i].occorrenza;
			MPI_Pack(&o, 1, MPI_INT,b, SIZEBUFFER, &position, MPI_COMM_WORLD);
			par = sizeParole-i-1;
			MPI_Pack(&par, 1, MPI_INT,b, SIZEBUFFER, &position, MPI_COMM_WORLD);
		}
		MPI_Send(b, SIZEBUFFER, MPI_PACKED, 0, 0, MPI_COMM_WORLD);
	}// Fine slave

	MPI_Finalize();
	return 0;
}

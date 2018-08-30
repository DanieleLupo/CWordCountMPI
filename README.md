# CWordsCountMPI
Il programma è realizzato in C utilizzando MPI, sfruttando la programmazione parallela, per contare le parole all'interno di più file. Le parole all'interno del file possono non avere una formattazione ben definita ma, per semplicità, le parola all'interno dei file (archivio allegato nella repository) utilizzati per la fase di testing sono disposte una per ogni riga.

## Esecuzione
Prima di eseguire il programam bisogna estrarre i file di test. Per fare ciò, posizionarsi all'interno della directory dove è presente l'archivio `files.tar` ed eseguire il seguente comando:

`tar xf files.tar`

L'estrazione creerà la cartella files con all'interno 250000 file (per motivi di spazio su AWS).

Dopo aver compilato il sorgente C, assumendo che si chiami `wordscount`, eseguire:

`mpirun -np n wordscount files/ k`

dove `n` è il numero di processi da utilizzare, `files/` è la directory **(attenzione allo slash finale)** contenente i file da utilizzare, mentre `k` è il numero massimo di file da computare.

## Scelte progettuali
Per affrontare il problema sono state prese le seguenti scelte progettuali:

* La struttura _file_ memorizza il nome del file, numero di parole già assegnate, numero di parole da assegnare;
* La struttura _word_ memorizza la parola e il numero di occorrenze;
* Una parola non può essere più lunga di 100 caratteri
* Il master legge prima tutti i file contando anche il numero di parole. Questo permette di bilanciare al meglio il carico;
* Se il numero di parole non è divisibile equamente fra i processi, il resto viene diviso fra i primi processi, compreso il master;
* Ogni processo riceve esattamente un pacchetto dal master dove viene indicato per ogni file:
  - nome del file;
  - numero di parole da saltare, poiché già assegnate ad un altro processo;
  - numero di parole da computare.
* Ogni processo estrae le informazioni dal pacchetto ricevuto dal _master_ e cerca le varie parole nella lista delle parole già trovate. Nel caso in cui la parola viene trovata, viene incrementato il contatore delle occorrenze; altrimenti viene aggiunta alla lista con occorrenza pari ad uno;
* Finita la computazione, ogni processo invia la lista delle parole trovate, con la relativa occorrenza, al _master_ tramite un unico pacchetto;
* Il _master_ resta in ascolto degli _slave_ e ogni volta che riceve i risultati, li unisce a quelli che già ha.

## Benchmark
Per il calcolo del tempo di esecuzione è stata utilizzata la funzione `MPI_Wtime()` offerta da MPI.

### Strong scaling
Nella seguente tabella vengono riportati i risultati del test della **strong scaling efficiency**.

|Processi MPI| Tempo | Percentuale |
|--------|-----------|-------------|
|1		 | 53,85  | - |
|2       | 48,47 	 |	55,55%     |
|4       | 29,06  |	46,33%	   |
|6       | 23,77  |	37.14%	   |
|8       | 25,52 	 |	26,38%	   |
|10      | 32,52 	 |	16,56%	   |
|12      | 37,74 	 |	11,89%	   |
|14      | 43,73 	 |	8,80%	   |
|16      | 49,99	 |	6,73%	   |

![Grafico Strong scaling](https://github.com/DanieleLupo/CWordsCountMPI/blob/master/StrongScaling.PNG?raw=true "Grafico Strong Scaling")

### Weak scaling
Nella tabella seguente vengono riportati i risultati del test della **weak scaling efficiency**.

|Processi MPI| Tempo | File | Percentuale |
|--------|-----------|-------------|-------------|
|1		 | 3,37  | 15625 | - |
|2       | 7,26 |31250 	 |	46,42%     |
|4       | 9,52 | 62500  |	35,40%	   |
|6       | 15,87  | 93750  |	21,24%	   |
|8       | 22,75  | 125000 	 |	14,81%	   |
|10      | 28,91  | 156250 	 |	11,66%	   |
|12      | 35,62  | 187500 	 |	9,46%	   |
|14      | 42,96	 |  218750  |	7,84%	   |
|16      | 50,02  | 250000	 |	6,74%	   |

![Grafico Weak scaling](https://github.com/DanieleLupo/CWordsCountMPI/blob/master/WeakScaling.PNG?raw=true "Grafico Weak Scaling")

Entrambi gli sweet spot si ottengono con 2 processi, successivamente l'overhead di comunicazione peggiora le prestazioni.
